#include "Compiler.h"
#include "ClassSource.h"
#include "Platform/Process.h"
#include <JSL.h>
#include <regex>

namespace
{
	// True if a TeX log contains one of LaTeX's (or a package's) requests for another pass. Deliberately excludes "There were undefined
	// references": a genuinely undefined label never resolves, so rerunning would just waste passes
	bool NeedsRerun(const std::string &log)
	{
		for (std::string_view signal : {"Rerun to get", "Rerun LaTeX", "Please rerun", "Label(s) may have changed"})
		{
			if (log.find(signal) != std::string::npos) return true;
		}
		return false;
	}

	// Splits text into its lines (a trailing newline doesn't make an extra empty line)
	std::vector<std::string> Lines(std::string_view text)
	{
		std::vector<std::string> out;
		size_t start = 0;
		while (start < text.size())
		{
			auto end = text.find('\n', start);
			if (end == std::string_view::npos) end = text.size();
			out.emplace_back(text.substr(start, end - start));
			start = end + 1;
		}
		return out;
	}

	std::filesystem::path WithExtension(std::filesystem::path path, const std::string &extension)
	{
		path.replace_extension(extension);
		return path;
	}

	// LaTeX's warnings about labels in a log: each specific one, not the closing "There were ..." summaries
	std::vector<std::string> LabelWarnings(const std::string &log)
	{
		static const std::regex warning(R"(LaTeX Warning: ((Label|Reference) `[^']*' [^\n]*(multiply defined|undefined))[^\n]*)");
		std::vector<std::string> out;
		std::set<std::string> seen;
		for (std::sregex_iterator match(log.begin(), log.end(), warning), end; match != end; ++match)
		{
			auto text = (*match)[1].str();
			if (seen.insert(text).second) out.push_back(text);
		}
		return out;
	}
} // namespace

std::string Tex::Hex(std::string_view text)
{
	static const char *digits = "0123456789ABCDEF";
	std::string out;
	for (unsigned char c : text)
	{
		out += digits[c >> 4];
		out += digits[c & 15];
	}
	return out;
}

std::set<std::filesystem::path> Tex::ReadInputs(const std::filesystem::path &flsFile)
{
	std::set<std::filesystem::path> inputs;
	if (!std::filesystem::exists(flsFile)) return inputs;

	// the .fls lists "PWD <dir>" first, then "INPUT <file>" for everything opened, with relative paths relative to PWD; the same file can
	// appear several times under different spellings (./part.tex, part.tex), and some entries aren't files at all (commands run by graphicx)
	std::filesystem::path pwd;
	for (auto &line : JSL::IO::getFileLines(flsFile))
	{
		if (line.starts_with("PWD "))
		{
			pwd = line.substr(4);
		}
		else if (line.starts_with("INPUT "))
		{
			std::filesystem::path file = line.substr(6);
			if (file.is_relative()) file = pwd / file;
			file = file.lexically_normal();
			if (std::filesystem::is_regular_file(file)) inputs.insert(file);
		}
	}
	return inputs;
}

std::map<std::string, std::string> Tex::ReadLabels(const std::filesystem::path &auxFile)
{
	std::map<std::string, std::string> labels;
	if (!std::filesystem::exists(auxFile)) return labels;
	// \newlabel{<name>}{{<text>}{<page>}...}: the text is the first group, which may itself contain braces
	for (auto &line : JSL::IO::getFileLines(auxFile))
	{
		if (!line.starts_with("\\newlabel{")) continue;
		auto nameEnd = line.find("}{{", 10);
		if (nameEnd == std::string::npos) continue;
		auto name = line.substr(10, nameEnd - 10);
		size_t depth = 1, i = nameEnd + 3;
		for (; i < line.size() && depth > 0; ++i)
		{
			if (line[i] == '{') ++depth;
			else if (line[i] == '}') --depth;
		}
		if (depth == 0 && !labels.contains(name)) labels[name] = line.substr(nameEnd + 3, i - 1 - nameEnd - 3);
	}
	return labels;
}

std::string Tex::Compiler::FragmentName(const Notes::Note &note)
{
	return "cortexfragment-" + note.Key;
}

Tex::Compiler::Compiler(std::filesystem::path buildDir, std::filesystem::path classDir, std::filesystem::path fragmentDir, std::string preamble,
						Settings::Compile settings)
	: BuildDir(std::move(buildDir)), ClassDir(std::move(classDir)), FragmentDir(std::move(fragmentDir)), Preamble(std::move(preamble)), Settings(std::move(settings))
{
	auto target = ClassDir / "cortex.cls";
	if (std::filesystem::exists(target) && JSL::IO::getFile(target) == ClassSource()) return;
	std::filesystem::create_directories(ClassDir);
	JSL::IO::writeString(target, ClassSource());
}

std::string Tex::Compiler::Diagnose(const Job &job, const std::filesystem::path &texFile, const std::filesystem::path &logFile, const std::vector<LineSource> &sources) const
{
	if (!std::filesystem::exists(logFile)) return "no log was produced";
	auto &note = *job.Note;

	// with -file-line-error, TeX reports errors as "<file>:<line>: <message>"
	static const std::regex errorLine(R"(^(.*):([0-9]+): (.*)$)");
	// some errors (e.g. a missing \input file or package) are instead reported as "! <message>", followed by
	// "<file>:<line>: Emergency stop." when nonstopmode can't ask for a new file name: the location is right, the message isn't
	std::string bangMessage;
	for (auto &line : JSL::IO::getFileLines(logFile))
	{
		if (line.starts_with("! "))
		{
			bangMessage = line.substr(2);
			continue;
		}
		std::smatch match;
		if (!std::regex_match(line, match, errorLine)) continue;
		std::filesystem::path file = match[1].str();
		size_t lineNumber = std::stoul(match[2].str());
		std::string message = match[3].str();
		if (message.starts_with("Emergency stop") && !bangMessage.empty()) message = bangMessage;
		// graphicx's message for a missing image only talks about bounding boxes
		if (message.find("Cannot determine size of graphic") != std::string::npos) message += " (does the image file exist?)";

		/// AN ERROR IN A FRAGMENT: A NOTE'S BODY, EITHER THIS NOTE'S OR ONE TRANSCLUDED INTO IT
		// fragments are the note bodies line for line, so the line maps straight back to the note
		if (file.stem().string().starts_with("cortexfragment-"))
		{
			const Notes::Note *source = (file.stem() == FragmentName(note)) ? &note : nullptr;
			for (auto *hoisted : job.Hoisted)
			{
				if (file.stem() == FragmentName(*hoisted)) source = hoisted;
			}
			if (source)
			{
				std::string where = source->Path.generic_string() + ":" + std::to_string(source->BodyLine + lineNumber - 1);
				std::string context = (source == &note) ? "" : " (transcluded into " + note.Path.generic_string() + ")";
				return where + context + ": " + message;
			}
		}

		/// AN ERROR IN THE GENERATED WRAPPER: MAP IT THROUGH THE LINE SOURCES
		if (file.filename() == texFile.filename() && lineNumber >= 1 && lineNumber <= sources.size())
		{
			auto &source = sources[lineNumber - 1];
			if (source.File.empty()) return "(line " + std::to_string(lineNumber) + " generated by CorTeX): " + message;
			std::string context = source.Context.empty() ? "" : " (" + source.Context + ")";
			return source.File + ":" + std::to_string(source.Line) + context + ": " + message;
		}

		// errors inside another file (an \input, a package) are reported as TeX gives them
		return file.string() + ":" + std::to_string(lineNumber) + ": " + message;
	}
	return "no error message found in the log";
}

Tex::Result Tex::Compiler::Compile(const Job &job) const
{
	auto &note = *job.Note;
	auto texFile = BuildDir / (note.Id + ".tex");
	auto outDir = texFile.parent_path();
	std::filesystem::create_directories(outDir);

	/// ASSEMBLE THE PAGE, LINE BY LINE, RECORDING WHERE EVERY LINE CAME FROM (SO ERRORS CAN BE TRACED BACK TO THE AUTHOR'S FILES)
	std::string tex;
	std::vector<LineSource> sources;
	auto emit = [&](const std::string &line, LineSource source = {}) {
		tex += line + "\n";
		sources.push_back(std::move(source));
	};

	// dvisvgm is a global option so that every package choosing a driver sees it (cortex.cls must not declare it as a key:
	// \ProcessKeyOptions removes the options it uses from the global list): naming packages one by one misses any not on the list,
	// which then fall back to XeTeX's native driver (and dvisvgm silently drops their images)
	emit("\\documentclass[" + std::string(Settings.Pdf ? "pdf" : "svg,dvisvgm") + ",width=" + JSL::String::makeFrom(Settings.WidthCM) +
		 "cm,margin=" + JSL::String::makeFrom(Settings.MarginPT) + "pt]{cortex}");
	auto preambleLines = Lines(Preamble);
	for (size_t i = 0; i < preambleLines.size(); ++i) emit(preambleLines[i], {".cortex/preamble.tex", i + 1, ""});

	/// LOCAL PREAMBLES: THE NOTE'S OWN, THEN EVERY TRANSCLUDED NOTE'S (HOISTED), SKIPPING LINES ALREADY PRESENT
	// identical lines are dropped from hoisted preambles (two notes both loading tikz is the common case); any other conflict is a TeX error
	std::set<std::string> seen;
	for (auto &line : preambleLines) seen.insert(JSL::String::trim(line));
	auto localLines = Lines(note.LocalPreamble);
	for (size_t i = 0; i < localLines.size(); ++i)
	{
		emit(localLines[i], {note.Path.generic_string(), note.LocalPreambleLine + i, "local preamble"});
		seen.insert(JSL::String::trim(localLines[i]));
	}
	for (auto *hoisted : job.Hoisted)
	{
		auto lines = Lines(hoisted->LocalPreamble);
		for (size_t i = 0; i < lines.size(); ++i)
		{
			auto trimmed = JSL::String::trim(lines[i]);
			bool meaningful = !trimmed.empty() && !trimmed.starts_with("%");
			if (meaningful && seen.contains(trimmed)) continue;
			seen.insert(trimmed);
			emit(lines[i], {hoisted->Path.generic_string(), hoisted->LocalPreambleLine + i, "local preamble, transcluded into " + note.Path.generic_string()});
		}
	}

	/// WHERE THE PAGE'S LINKS, TRANSCLUSIONS AND LABEL REFERENCES GO
	for (auto &declaration : job.Declarations) emit(declaration);

	/// THE BODY: THE NOTE'S FRAGMENT, INPUT JUST AS A TRANSCLUSION OF IT WOULD BE
	emit("\\begin{document}");
	emit("\\begin{cortexbody}");
	if (job.ShowTitle) emit("\\CortexTitle{" + note.Fields.Title + "}", {note.Path.generic_string(), 1, "title"});
	emit("\\input{" + FragmentName(note) + "}");
	emit("\\end{cortexbody}");
	emit("\\end{document}");
	JSL::IO::writeString(texFile, tex);

	/// RUN XELATEX UNTIL REFERENCES SETTLE
	auto outFile = WithExtension(texFile, Settings.Pdf ? ".pdf" : ".xdv");
	auto logFile = WithExtension(texFile, ".log");
	std::filesystem::remove(outFile); // so a failed run can't leave a stale result looking like a success

	Platform::Command command;
	command.Arguments = {"xelatex"};
	if (!Settings.Pdf) command.Arguments.push_back("-no-pdf");
	// -file-line-error makes TeX report errors as "<file>:<line>: <message>", which Diagnose relies on; -recorder writes a .fls listing
	// every file TeX opened, so the note's dependencies (\input files, local packages, images) can be tracked
	for (auto option : {"-interaction=nonstopmode", "-halt-on-error", "-file-line-error", "-recorder"}) command.Arguments.push_back(option);
	command.Arguments.push_back("-output-directory=" + outDir.string());
	command.Arguments.push_back(texFile.string());
	command.WorkingDirectory = note.Source.parent_path();
	// cortex.cls and the fragments aren't on TeX's search path by default; the trailing separator keeps the default paths (starting with
	// the note's own folder) after them. max_print_line stops TeX wrapping log lines, which could split the messages read back from them
	auto separator = std::string(1, Platform::PathListSeparator);
	command.Environment = {{"TEXINPUTS", ClassDir.string() + separator + FragmentDir.string() + separator}, {"max_print_line", "10000"}};
	command.OutputFile = WithExtension(texFile, ".stdout");
	command.Timeout = Timeout;

	for (int pass = 1; pass <= MaxPasses; ++pass)
	{
		auto outcome = Platform::Run(command);
		if (outcome.State == Platform::Outcome::Status::TimedOut)
		{
			return {std::nullopt, "xelatex was still running after " + std::to_string(Timeout.count()) + "s (an infinite loop?)", {}, {}, {}};
		}
		if (outcome.State == Platform::Outcome::Status::CouldNotStart) return {std::nullopt, "xelatex could not be run: " + outcome.Error, {}, {}, {}};
		if (!outcome.Succeeded() || !std::filesystem::exists(outFile)) return {std::nullopt, Diagnose(job, texFile, logFile, sources), {}, {}, {}};

		auto log = JSL::IO::getFile(logFile);
		if (!NeedsRerun(log) || pass == MaxPasses)
		{
			return {outFile, "", ReadInputs(WithExtension(texFile, ".fls")), ReadLabels(WithExtension(texFile, ".aux")), LabelWarnings(log)};
		}
	}
	return {};
}
