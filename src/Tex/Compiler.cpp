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
						Settings::Compile settings, WarmPool *warm, const std::filesystem::path &warmDir, const std::filesystem::path &root)
	: BuildDir(std::move(buildDir)), ClassDir(std::move(classDir)), FragmentDir(std::move(fragmentDir)), Preamble(std::move(preamble)), Settings(std::move(settings)),
	  Warm(warmDir.empty() ? nullptr : warm)
{
	auto target = ClassDir / "cortex.cls";
	if (!std::filesystem::exists(target) || JSL::IO::getFile(target) != ClassSource())
	{
		std::filesystem::create_directories(ClassDir);
		JSL::IO::writeString(target, ClassSource());
	}

	// the workers' driver: exactly the start of every page (the class, the project preamble), then the wait for the rest. The class
	// file's text is part of the spec, so a new CorTeX (whose class differs) never uses workers which loaded the old one
	if (Warm)
	{
		WarmPool::Spec spec;
		spec.Folder = warmDir;
		spec.Root = root;
		spec.Driver = "% cortex.cls " + std::to_string(std::hash<std::string_view>{}(ClassSource())) + "\n" + ClassLine() + "\n" + Preamble +
					  (Preamble.empty() || Preamble.ends_with("\n") ? "" : "\n") + WarmPool::WaitLine() + "\n";
		spec.Arguments = Arguments();
		spec.Environment = Environment();
		Warm->Prepare(spec, Settings.Warm);
	}
}

std::string Tex::Compiler::ClassLine() const
{
	// dvisvgm is a global option so that every package choosing a driver sees it (cortex.cls must not declare it as a key:
	// \ProcessKeyOptions removes the options it uses from the global list): naming packages one by one misses any not on the list,
	// which then fall back to XeTeX's native driver (and dvisvgm silently drops their images)
	return "\\documentclass[" + std::string(Settings.Pdf ? "pdf" : "svg,dvisvgm") + ",width=" + JSL::String::makeFrom(Settings.WidthCM) +
		   "cm,margin=" + JSL::String::makeFrom(Settings.MarginPT) + "pt]{cortex}";
}

std::vector<std::string> Tex::Compiler::Arguments() const
{
	std::vector<std::string> arguments = {"xelatex"};
	if (!Settings.Pdf) arguments.push_back("-no-pdf");
	// -file-line-error makes TeX report errors as "<file>:<line>: <message>", which Diagnose relies on; -recorder writes a .fls listing
	// every file TeX opened, so the note's dependencies (\input files, local packages, images) can be tracked
	for (auto option : {"-halt-on-error", "-file-line-error", "-recorder"}) arguments.push_back(option);
	return arguments;
}

std::vector<std::pair<std::string, std::string>> Tex::Compiler::Environment() const
{
	// cortex.cls and the fragments aren't on TeX's search path by default; the trailing separator keeps the default paths (starting with
	// the note's own folder) after them. max_print_line stops TeX wrapping log lines, which could split the messages read back from them
	auto separator = std::string(1, Platform::PathListSeparator);
	return {{"TEXINPUTS", ClassDir.string() + separator + FragmentDir.string() + separator}, {"max_print_line", "10000"}};
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

//! A note's page, assembled: the file a fresh XeLaTeX compiles (the class, the project preamble, then the rest), where each of its
//! lines came from, and the rest alone (what a warm worker, which has loaded the class and preamble already, is given)
struct Tex::Compiler::Page
{
	std::filesystem::path TexFile;
	std::vector<LineSource> Sources;
	std::string Rest;
	std::filesystem::path OutFile, LogFile;
};

namespace
{
	// The note's own files LaTeX reads back on the next pass (the .aux, and those of packages: hyperref's .out, a contents list's .toc...)
	constexpr std::string_view CarriedOver[] = {".aux", ".out", ".toc", ".lof", ".lot", ".bbl", ".nav", ".snm", ".vrb"};

	//! How far a line of LaTeX opens (+) or closes (-) brace groups: escaped braces and anything after a comment don't count
	int BraceDepthChange(std::string_view line)
	{
		int depth = 0;
		for (size_t i = 0; i < line.size(); ++i)
		{
			char c = line[i];
			if (c == '\\') ++i; // \{, \}, \%, \\: the next character is escaped
			else if (c == '%') break;
			else if (c == '{') ++depth;
			else if (c == '}') --depth;
		}
		return depth;
	}

	//! A preamble's statements, as [first, last) line ranges: a statement runs until its brace groups are closed, and carries on while
	//! the next line opens another argument ({ or [), so a definition whose body starts on its own line is one statement
	std::vector<std::pair<size_t, size_t>> Statements(const std::vector<std::string> &lines)
	{
		std::vector<std::pair<size_t, size_t>> out;
		size_t start = 0;
		int depth = 0;
		for (size_t i = 0; i < lines.size(); ++i)
		{
			depth += BraceDepthChange(lines[i]);
			if (depth > 0) continue;
			depth = 0;
			if (i + 1 < lines.size())
			{
				auto next = JSL::String::trim(lines[i + 1]);
				if (next.starts_with("{") || next.starts_with("[")) continue;
			}
			out.emplace_back(start, i + 1);
			start = i + 1;
		}
		if (start < lines.size()) out.emplace_back(start, lines.size()); // left open: TeX will say so
		return out;
	}

	//! A statement as it's compared for duplicates: its lines, trimmed; empty for blank lines and comments, which are never dropped
	std::string StatementKey(const std::vector<std::string> &lines, std::pair<size_t, size_t> range)
	{
		std::string key;
		for (size_t i = range.first; i < range.second; ++i)
		{
			auto trimmed = JSL::String::trim(lines[i]);
			if (trimmed.empty() || trimmed.starts_with("%")) continue;
			key += trimmed + "\n";
		}
		return key;
	}
} // namespace

std::optional<Tex::Result> Tex::Compiler::CompileWarm(const Job &job, const Page &page) const
{
	auto &note = *job.Note;
	auto folder = note.Source.parent_path();
	if (!Warm) return std::nullopt;
	auto texFolder = page.TexFile.parent_path();
	auto stem = page.TexFile.stem().string();

	for (int pass = 1; pass <= MaxPasses; ++pass)
	{
		auto worker = Warm->Take(folder);
		if (!worker) return std::nullopt;
		auto own = [&](std::string_view extension) { return worker->Folder / (worker->Name + std::string(extension)); };

		// what the last pass (or build) left for this one, under the worker's job name
		for (auto extension : CarriedOver)
		{
			auto file = WithExtension(page.TexFile, std::string(extension));
			if (std::filesystem::exists(file)) std::filesystem::copy_file(file, own(extension), std::filesystem::copy_options::overwrite_existing);
		}
		// the rest of the page, out of scroll mode: an error stops the run, rather than waiting for an answer
		auto rest = own("-page.tex");
		JSL::IO::writeString(rest, "\\nonstopmode\n" + page.Rest);
		worker->Run->Finish(rest.string() + "\n");
		auto outcome = worker->Run->Wait(Timeout);
		std::filesystem::remove(rest);

		// everything it wrote becomes the note's, as if it had run under the note's name
		for (auto &entry : std::filesystem::directory_iterator(worker->Folder))
		{
			auto name = entry.path().filename().string();
			if (!name.starts_with(worker->Name + ".")) continue;
			std::filesystem::rename(entry.path(), texFolder / (stem + name.substr(worker->Name.size())));
		}

		if (outcome.State == Platform::Outcome::Status::TimedOut)
		{
			return Result{std::nullopt, "xelatex was still running after " + std::to_string(Timeout.count()) + "s (an infinite loop?)", {}, {}, {}};
		}
		if (!outcome.Succeeded() || !std::filesystem::exists(page.OutFile))
		{
			// a copy among the workers' files (the note's own log is overwritten by the fresh compile)
			auto kept = own("-failed.log");
			std::error_code ignored;
			std::filesystem::copy_file(page.LogFile, kept, std::filesystem::copy_options::overwrite_existing, ignored);
			return Result{std::nullopt, "", {}, {}, {}, kept.string()};
		}
		auto log = JSL::IO::getFile(page.LogFile);
		if (!NeedsRerun(log) || pass == MaxPasses)
		{
			return Result{page.OutFile, "", ReadInputs(WithExtension(page.TexFile, ".fls")), ReadLabels(WithExtension(page.TexFile, ".aux")), LabelWarnings(log), ""};
		}
	}
	return std::nullopt;
}

Tex::Result Tex::Compiler::Compile(const Job &job, bool warm) const
{
	auto &note = *job.Note;
	Page page;
	page.TexFile = BuildDir / (note.Id + ".tex");
	auto outDir = page.TexFile.parent_path();
	std::filesystem::create_directories(outDir);

	/// ASSEMBLE THE PAGE, LINE BY LINE, RECORDING WHERE EVERY LINE CAME FROM (SO ERRORS CAN BE TRACED BACK TO THE AUTHOR'S FILES)
	std::string tex;
	auto &sources = page.Sources;
	bool inRest = false;
	auto emit = [&](const std::string &line, LineSource source = {}) {
		tex += line + "\n";
		if (inRest) page.Rest += line + "\n";
		sources.push_back(std::move(source));
	};

	emit(ClassLine());
	auto preambleLines = Lines(Preamble);
	for (size_t i = 0; i < preambleLines.size(); ++i) emit(preambleLines[i], {".cortex/preamble.tex", i + 1, ""});
	inRest = true; // from here on, the page differs from note to note: what a warm worker is given

	/// LOCAL PREAMBLES: THE NOTE'S OWN, THEN EVERY TRANSCLUDED NOTE'S (HOISTED), SKIPPING STATEMENTS ALREADY PRESENT
	// identical statements are dropped from hoisted preambles (two notes both loading tikz is the common case); any other conflict is
	// a TeX error. Whole statements, not lines: a lone "{" or a body line of a definition is only ever dropped with the rest of it
	std::set<std::string> seen;
	auto remember = [&seen](const std::vector<std::string> &lines) {
		for (auto range : Statements(lines)) seen.insert(StatementKey(lines, range));
	};
	remember(preambleLines);
	auto localLines = Lines(note.LocalPreamble);
	for (size_t i = 0; i < localLines.size(); ++i) emit(localLines[i], {note.Path.generic_string(), note.LocalPreambleLine + i, "local preamble"});
	remember(localLines);
	for (auto *hoisted : job.Hoisted)
	{
		auto lines = Lines(hoisted->LocalPreamble);
		for (auto range : Statements(lines))
		{
			auto key = StatementKey(lines, range);
			if (!key.empty() && !seen.insert(key).second) continue;
			for (size_t i = range.first; i < range.second; ++i)
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
	JSL::IO::writeString(page.TexFile, tex);

	page.OutFile = WithExtension(page.TexFile, Settings.Pdf ? ".pdf" : ".xdv");
	page.LogFile = WithExtension(page.TexFile, ".log");
	std::filesystem::remove(page.OutFile); // so a failed run can't leave a stale result looking like a success

	/// WITH WARM WORKERS, IF THEY CAN: ANY FAILURE IS TRIED AGAIN BELOW, SO IT IS REPORTED JUST AS A FRESH XELATEX WOULD REPORT IT
	std::string warmFailure;
	if (auto tried = warm ? CompileWarm(job, page) : std::nullopt)
	{
		if (tried->Output || !tried->Error.empty()) return *tried;
		warmFailure = tried->WarmFailure;
	}
	std::filesystem::remove(page.OutFile);
	auto fresh = [&](Result result) {
		result.WarmFailure = warmFailure;
		return result;
	};

	/// RUN XELATEX UNTIL REFERENCES SETTLE
	Platform::Command command;
	command.Arguments = Arguments();
	command.Arguments.push_back("-interaction=nonstopmode");
	command.Arguments.push_back("-output-directory=" + outDir.string());
	command.Arguments.push_back(page.TexFile.string());
	command.WorkingDirectory = note.Source.parent_path();
	command.Environment = Environment();
	command.OutputFile = WithExtension(page.TexFile, ".stdout");
	command.Timeout = Timeout;

	for (int pass = 1; pass <= MaxPasses; ++pass)
	{
		auto outcome = Platform::Run(command);
		if (outcome.State == Platform::Outcome::Status::TimedOut)
		{
			return fresh({std::nullopt, "xelatex was still running after " + std::to_string(Timeout.count()) + "s (an infinite loop?)", {}, {}, {}, ""});
		}
		if (outcome.State == Platform::Outcome::Status::CouldNotStart) return fresh({std::nullopt, "xelatex could not be run: " + outcome.Error, {}, {}, {}, ""});
		if (!outcome.Succeeded() || !std::filesystem::exists(page.OutFile)) return fresh({std::nullopt, Diagnose(job, page.TexFile, page.LogFile, sources), {}, {}, {}, ""});

		auto log = JSL::IO::getFile(page.LogFile);
		if (!NeedsRerun(log) || pass == MaxPasses)
		{
			return fresh({page.OutFile, "", ReadInputs(WithExtension(page.TexFile, ".fls")), ReadLabels(WithExtension(page.TexFile, ".aux")), LabelWarnings(log), ""});
		}
	}
	return fresh({});
}
