#include "TexRunner.h"
#include "Shell.h"
#include <JSL.h>
#include <algorithm>
#include <regex>

// The class every note is compiled against. The [svg] option (the default) loads the dvisvgm drivers, which
// must be in place before any user package loads pgf, graphicx or xcolor; [pdf] leaves the native drivers alone.
static constexpr std::string_view CortexClass = R"tex(\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{cortex}[CorTeX note class]
\newif\ifcortex@svg
\cortex@svgtrue
\def\cortex@width{25cm}
\DeclareKeys[cortex]{
	svg .code = \cortex@svgtrue,
	pdf .code = \cortex@svgfalse,
	width .store = \cortex@width,
	unknown .code = \PassOptionsToClass{\CurrentOption}{standalone}
}
\ProcessKeyOptions[cortex]
\ifcortex@svg
	\def\pgfsysdriver{pgfsys-dvisvgm.def}
	\PassOptionsToPackage{dvisvgm}{graphicx}
	\PassOptionsToPackage{dvisvgm}{graphics}
	\PassOptionsToPackage{dvisvgm}{xcolor}
\fi
% a small margin on every page: without it, glyphs touch the edge of the SVG and the browser clips their tops
\PassOptionsToClass{border=4pt}{standalone}
\LoadClass{standalone}

% The engine wraps every note body in this, so all notes share one fixed width (and narrower content can be centred within it).
% minipage resets \parindent and \parskip to zero, so the values set by the preambles are captured before it opens, and restored inside.
\newlength{\cortex@parindent}
\newskip\cortex@parskip
\newenvironment{cortexbody}
{%
	\setlength{\cortex@parindent}{\parindent}%
	\setlength{\cortex@parskip}{\parskip}%
	\begin{minipage}{\cortex@width}%
	\setlength{\parindent}{\cortex@parindent}%
	\setlength{\parskip}{\cortex@parskip}%
}
{\end{minipage}}

% With the InsertTitle setting, every note begins \CortexTitle{<title>}; users may \renewcommand it
\newcommand{\CortexTitle}[1]{{\Large\bfseries #1\par}\medskip}

% [[links]] are rewritten by the engine into \CortexLink{<origin>}{<slug>}{<text>}, where <origin> is the key of the note the link was written
% in: a link always resolves from where it was written, even inside a transcluded note.
% SVG output: the text is wrapped in an <a> pointing at the viewer's route for that origin and slug (a raw dvisvgm special: no hyperref, no
% border box, and a link which wraps across lines stays one anchor); the viewer resolves it and styles the link.
% PDF output: there is no viewer, so the engine resolves every link before compiling, and declares each target with
% \CortexLinkTarget{<origin>}{<slug>}{<path to the target PDF, relative to this one, hex-encoded so no filename can upset TeX>}.
% A resolved link becomes a GoToR annotation (a raw xdvipdfmx special: "open that file"), followed by PDF readers such as Okular;
% an unresolved one is just styled. Users may \renewcommand \CortexLinkText / \CortexUnresolvedLink (or \CortexLink itself) in their preamble.
\newcommand{\CortexLinkTarget}[3]{\expandafter\def\csname cortex@target@#1@#2\endcsname{#3}}
\ifcortex@svg
	\newcommand{\CortexLink}[3]{\special{dvisvgm:raw <a class="cortex-link" xlink:href="\string##/a/#1/#2">}#3\special{dvisvgm:raw </a>}}
\else
	\newcommand{\CortexLinkText}[1]{\special{color push rgb 0.1 0.37 0.7}#1\special{color pop}}
	\newcommand{\CortexUnresolvedLink}[1]{\special{color push rgb 0.75 0.1 0.15}#1\special{color pop}}
	\newcommand{\CortexLink}[3]{%
		\ifcsname cortex@target@#1@#2\endcsname
			\special{pdf:bann << /Type/Annot /Subtype/Link /Border[0 0 0] /A << /S/GoToR /F <\csname cortex@target@#1@#2\endcsname> /D[0 /Fit] >> >>}%
			\CortexLinkText{#3}%
			\special{pdf:eann}%
		\else
			\CortexUnresolvedLink{#3}%
		\fi}
\fi

% ![[transclusions]] are rewritten into \CortexTransclude{<origin>}{<slug>}. The engine resolves each one before compiling (from its origin,
% like links) and declares the fragment to \input with \CortexTranscludeTarget, or why it can't be made (no such note; it would loop back on
% itself) with \CortexTranscludeProblem; the problem is shown in place with \CortexTransclusionProblem, which users may \renewcommand.
\newcommand{\CortexTranscludeTarget}[3]{\expandafter\def\csname cortex@transclude@#1@#2\endcsname{#3}}
\newcommand{\CortexTranscludeProblem}[3]{\expandafter\def\csname cortex@transcludeproblem@#1@#2\endcsname{#3}}
\newcommand{\CortexTransclusionProblem}[1]{{\special{color push rgb 0.75 0.1 0.15}[#1]\special{color pop}}}
\newcommand{\CortexTransclude}[2]{%
	\@ifundefined{cortex@transclude@#1@#2}%
		{\@ifundefined{cortex@transcludeproblem@#1@#2}%
			{\CortexTransclusionProblem{cannot transclude #2}}%
			{\CortexTransclusionProblem{\csname cortex@transcludeproblem@#1@#2\endcsname}}}%
		{\input{\csname cortex@transclude@#1@#2\endcsname}}}
)tex";

TexRunner::TexRunner(std::filesystem::path buildDir, std::string_view preamble, bool producePdf, double widthCM, bool insertTitle) : BuildDir(std::move(buildDir)), Preamble(std::move(preamble)), ProducePdf(producePdf), WidthCM(widthCM), InsertTitle(insertTitle)
{
	ClassDir = BuildDir.parent_path() / "class";
	FragmentDir = BuildDir.parent_path() / "fragments";
	WriteClass();
}

std::string_view TexRunner::ClassSource()
{
	return CortexClass;
}

void TexRunner::WriteClass()
{
	auto target = ClassDir / "cortex.cls";
	if (std::filesystem::exists(target) && JSL::IO::getFile(target) == CortexClass)
	{
		LOG(DEBUG) << "cortex.cls is up to date";
		return;
	}

	if (auto made = JSL::IO::mkdir(ClassDir, JSL::IO::Policy::Generous); !made)
	{
		LOG(ERROR) << "Could not create the class directory " << ClassDir << ": " << made.Message;
		throw std::runtime_error("Cannot write cortex.cls");
	}
	LOG(DEBUG) << "Writing " << target;
	JSL::IO::writeString(target, CortexClass);
}

std::filesystem::path changeExtension(std::filesystem::path input, const std::string &newExtension)
{
	input.replace_extension(newExtension);
	return input;
}

// True if a TeX log contains one of LaTeX's (or a package's) requests for another pass.
// Deliberately excludes "There were undefined references": a genuinely undefined label never resolves, so rerunning would just waste passes.
static bool NeedsRerun(const std::string &log)
{
	for (std::string_view signal : {"Rerun to get", "Rerun LaTeX", "Please rerun", "Label(s) may have changed"})
	{
		if (log.find(signal) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

std::string TexRunner::FragmentName(const Note &note)
{
	return "cortexfragment-" + note.Key;
}

std::string TexRunner::Diagnose(const Note &note, const CompilePlan &plan, const std::filesystem::path &texFile, const std::filesystem::path &logFile, const std::vector<LineSource> &sources) const
{
	if (!std::filesystem::exists(logFile))
	{
		return "no log was produced";
	}

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
		if (!std::regex_match(line, match, errorLine))
		{
			continue;
		}
		std::filesystem::path file = match[1].str();
		size_t lineNumber = std::stoul(match[2].str());
		std::string message = match[3].str();
		if (message.starts_with("Emergency stop") && !bangMessage.empty())
		{
			message = bangMessage;
		}
		// graphicx's message for a missing image only talks about bounding boxes
		if (message.find("Cannot determine size of graphic") != std::string::npos)
		{
			message += " (does the image file exist?)";
		}

		/// AN ERROR IN A FRAGMENT: A NOTE'S BODY, EITHER THIS NOTE'S OR ONE TRANSCLUDED INTO IT
		// fragments are the note bodies line for line, so the line maps straight back to the note
		if (file.stem().string().starts_with("cortexfragment-"))
		{
			const Note *source = (file.stem() == FragmentName(note)) ? &note : nullptr;
			for (auto *hoisted : plan.Hoisted)
			{
				if (file.stem() == FragmentName(*hoisted)) source = hoisted;
			}
			if (source)
			{
				std::string where = source->RelativePath.generic_string() + ":" + std::to_string(source->BodyLine + lineNumber - 1);
				std::string context = (source == &note) ? "" : " (transcluded into " + note.RelativePath.generic_string() + ")";
				return where + context + ": " + message;
			}
		}

		/// AN ERROR IN THE GENERATED WRAPPER: MAP IT THROUGH THE LINE SOURCES
		if (file.filename() == texFile.filename() && lineNumber >= 1 && lineNumber <= sources.size())
		{
			auto &source = sources[lineNumber - 1];
			if (source.File.empty())
			{
				return "(line " + std::to_string(lineNumber) + " generated by CorTeX): " + message;
			}
			std::string context = source.Context.empty() ? "" : " (" + source.Context + ")";
			return source.File + ":" + std::to_string(source.Line) + context + ": " + message;
		}

		// errors inside another file (an \input, a package) are reported as TeX gives them
		return file.string() + ":" + std::to_string(lineNumber) + ": " + message;
	}
	return "no error message found in the log";
}

std::set<std::filesystem::path> TexRunner::ReadInputs(const std::filesystem::path &flsFile)
{
	std::set<std::filesystem::path> inputs;
	if (!std::filesystem::exists(flsFile))
	{
		return inputs;
	}

	// the .fls lists "PWD <dir>" first, then "INPUT <file>" for everything opened, with relative paths relative to PWD;
	// the same file can appear several times under different spellings (./part.tex, part.tex), and some entries aren't files at all (commands run by graphicx)
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
			if (file.is_relative())
			{
				file = pwd / file;
			}
			file = file.lexically_normal();
			if (std::filesystem::is_regular_file(file))
			{
				inputs.insert(file);
			}
		}
	}
	return inputs;
}

// Encodes a string as the contents of a PDF hex string (<...>): only hex digits reach TeX, so no filename can be misread as TeX
static std::string PdfHex(std::string_view text)
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

// Splits text into its lines (a trailing newline doesn't make an extra empty line)
static std::vector<std::string> Lines(std::string_view text)
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

TexRunner::Result TexRunner::Compile(const Note &note, const CompilePlan &plan)
{
	auto texFile = BuildDir / (note.Id + ".tex");

	/// CHECK DIRECTORY STRUCTURE EXISTS
	auto outDir = texFile.parent_path();
	if (!std::filesystem::exists(outDir))
	{
		LOG(INFO) << "Creating " << outDir;
		JSL::IO::mkdir(outDir, JSL::IO::Policy::Generous);
	}

	/// WRITE TO TEX FILE
	LOG(DEBUG) << "Writing tex file " << texFile;
	std::string options = std::string(ProducePdf ? "pdf" : "svg") + ",width=" + JSL::String::makeFrom(WidthCM) + "cm";
	std::string documentClass = "\\documentclass[" + options + "]{cortex}\n";
	// assembled line by line, recording where every line came from, so errors can be traced back to the user's files
	std::string tex;
	std::vector<LineSource> sources;
	auto emit = [&](const std::string &line, LineSource source = {}) {
		tex += line + "\n";
		sources.push_back(std::move(source));
	};

	emit(documentClass.substr(0, documentClass.size() - 1)); // (documentClass ends in a newline)
	auto preambleLines = Lines(Preamble);
	for (size_t i = 0; i < preambleLines.size(); ++i)
	{
		emit(preambleLines[i], {".cortex/preamble.tex", i + 1, ""});
	}

	/// LOCAL PREAMBLES: THE NOTE'S OWN, THEN EVERY TRANSCLUDED NOTE'S (HOISTED), SKIPPING LINES ALREADY PRESENT
	// identical lines are dropped from hoisted preambles (two notes both loading tikz is the common case); anything else conflicting is a normal TeX error
	std::set<std::string> seen;
	for (auto &line : preambleLines)
	{
		seen.insert(JSL::String::trim(line));
	}
	auto localLines = Lines(note.LocalPreamble);
	for (size_t i = 0; i < localLines.size(); ++i)
	{
		emit(localLines[i], {note.RelativePath.generic_string(), note.LocalPreambleLine + i, "local preamble"});
		seen.insert(JSL::String::trim(localLines[i]));
	}
	for (auto *hoisted : plan.Hoisted)
	{
		auto lines = Lines(hoisted->LocalPreamble);
		for (size_t i = 0; i < lines.size(); ++i)
		{
			auto trimmed = JSL::String::trim(lines[i]);
			bool meaningful = !trimmed.empty() && !trimmed.starts_with("%");
			if (meaningful && seen.contains(trimmed)) continue;
			seen.insert(trimmed);
			emit(lines[i], {hoisted->RelativePath.generic_string(), hoisted->LocalPreambleLine + i, "local preamble, transcluded into " + note.RelativePath.generic_string()});
		}
	}

	/// WHAT THIS NOTE'S (AND ITS TRANSCLUDED NOTES') LINKS AND TRANSCLUSIONS POINT AT
	for (auto &[origin, slug, path] : plan.LinkTargets)
	{
		emit("\\CortexLinkTarget{" + origin + "}{" + slug + "}{" + PdfHex(path) + "}");
	}
	for (auto &[origin, slug, key] : plan.Transclusions)
	{
		emit("\\CortexTranscludeTarget{" + origin + "}{" + slug + "}{cortexfragment-" + key + "}");
	}
	for (auto &[origin, slug, problem] : plan.TransclusionProblems)
	{
		emit("\\CortexTranscludeProblem{" + origin + "}{" + slug + "}{" + problem + "}");
	}

	/// THE BODY: THE NOTE'S FRAGMENT, INPUT JUST AS A TRANSCLUSION WOULD BE
	emit("\\begin{document}");
	emit("\\begin{cortexbody}");
	if (InsertTitle)
	{
		emit("\\CortexTitle{" + note.Title + "}", {note.RelativePath.generic_string(), 1, "title"});
	}
	emit("\\input{" + FragmentName(note) + "}");
	emit("\\end{cortexbody}");
	emit("\\end{document}");
	JSL::IO::writeString(texFile, tex);

	/// CHOOSE PDF OR XDV
	auto outFile = changeExtension(texFile, ProducePdf ? ".pdf" : ".xdv");
	auto logFile = changeExtension(texFile, ".log");

	// Remove previous outFiles (if they exist) so a failed run can't leave a stale result looking like a success
	JSL::IO::remove(outFile, JSL::IO::Policy::Generous);

	/// CONSTRUCT COMPILATION INSTRUCTIONS
	std::string mode = ProducePdf ? "" : "-no-pdf ";
	// cortex.cls lives in ClassDir and the fragments in FragmentDir, neither on TeX's search path by default; the trailing ':' keeps the
	// default paths (starting with the note's own directory) after them. Fragment names are key-based, so they never shadow the user's files
	// max_print_line stops TeX wrapping log lines at 79 characters, which could split the messages we search for
	// timeout kills a pass that never finishes (e.g. a recursive macro), turning a hung build into one failed note
	// -file-line-error makes TeX report errors as "<file>:<line>: <message>", which Diagnose relies on
	// -recorder writes a .fls listing every file TeX opened, so the note's dependencies (\input files, local packages, images) can be tracked
	auto cmd = "TEXINPUTS=" + Shell::Quote(ClassDir) + ":" + Shell::Quote(FragmentDir) + ": max_print_line=10000 timeout " + std::to_string(TimeoutSeconds) + " xelatex " + mode + "-interaction=nonstopmode -halt-on-error -file-line-error -recorder -output-directory=" + Shell::Quote(outDir) + " " + Shell::Quote(texFile);

	/// RUN COMPILER UNTIL REFERENCES SETTLE
	// Each pass resolves references using the .aux written by the pass before; LaTeX says in the log when that has changed things
	auto stdFile = changeExtension(texFile, ".stdcout");
	for (int pass = 1; pass <= MaxPasses; ++pass)
	{
		LOG(DEBUG) << "xelatex pass " << pass << " for " << note.RelativePath;
		int code = Shell::Run(cmd, (note.SourceAbsolutePath).parent_path(), stdFile);

		/// CHECK IF IT WORKED
		if (code == 124) // timeout's exit code when it had to kill the command
		{
			auto error = "xelatex was still running after " + std::to_string(TimeoutSeconds) + "s (an infinite loop?)";
			LOG(WARN) << "Failed to compile " << note.RelativePath << ": " << error << "; see " << logFile;
			return {std::nullopt, error, {}};
		}
		if (code != 0 || !std::filesystem::exists(outFile))
		{
			auto error = Diagnose(note, plan, texFile, logFile, sources);
			LOG(WARN) << "Failed to compile " << note.RelativePath << "\n\t" << error << "\n\tFull log: " << logFile;
			return {std::nullopt, error, {}};
		}

		/// CHECK IF ANOTHER PASS IS NEEDED
		if (!NeedsRerun(JSL::IO::getFile(logFile)))
		{
			return {outFile, "", ReadInputs(changeExtension(texFile, ".fls"))};
		}
	}

	LOG(WARN) << "References in " << note.RelativePath << " had not settled after " << MaxPasses << " passes; some may be wrong. See " << logFile;
	return {outFile, "", ReadInputs(changeExtension(texFile, ".fls"))};
}
