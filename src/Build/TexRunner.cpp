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

% [[links]] are rewritten by the engine into \CortexLink{<slug>}{<text>}. In SVG output the text is wrapped in an <a> pointing at the viewer's
% route for that slug (a raw dvisvgm special: no hyperref needed, no border box, and a link which wraps across lines stays one anchor);
% the viewer resolves the slug and styles the link. PDF output has no viewer to navigate, so the link is just its text.
% Users may \renewcommand it in their preamble.
\ifcortex@svg
	\newcommand{\CortexLink}[2]{\special{dvisvgm:raw <a class="cortex-link" xlink:href="\string##/a/#1">}#2\special{dvisvgm:raw </a>}}
\else
	\newcommand{\CortexLink}[2]{#2}
\fi
)tex";

TexRunner::TexRunner(std::filesystem::path buildDir, std::string_view preamble, bool producePdf, double widthCM) : BuildDir(std::move(buildDir)), Preamble(std::move(preamble)), ProducePdf(producePdf), WidthCM(widthCM)
{
	ClassDir = BuildDir.parent_path() / "class";
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

// The number of the line on which the next appended text would start
static size_t NextLine(const std::string &text)
{
	return std::count(text.begin(), text.end(), '\n') + 1;
}

std::string TexRunner::Diagnose(const Note &note, const std::filesystem::path &texFile, const std::filesystem::path &logFile, const Layout &layout) const
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

		// errors inside another file (an \input, a package) are reported as TeX gives them
		if (file.filename() != texFile.filename())
		{
			return file.string() + ":" + std::to_string(lineNumber) + ": " + message;
		}

		// otherwise, map the generated file's line back to the section of the user's files it came from
		if (lineNumber >= layout.BodyStart)
		{
			return note.RelativePath.string() + ":" + std::to_string(note.BodyLine + lineNumber - layout.BodyStart) + ": " + message;
		}
		if (lineNumber >= layout.LocalPreambleStart)
		{
			return note.RelativePath.string() + ":" + std::to_string(note.LocalPreambleLine + lineNumber - layout.LocalPreambleStart) + " (local preamble): " + message;
		}
		if (lineNumber >= layout.PreambleStart)
		{
			return ".cortex/preamble.tex:" + std::to_string(1 + lineNumber - layout.PreambleStart) + ": " + message;
		}
		return "(generated \\documentclass line): " + message;
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

TexRunner::Result TexRunner::Compile(const Note &note)
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
	// assembled piece by piece, recording where each part starts so errors can be traced back to the user's files
	Layout layout;
	std::string tex = documentClass;
	layout.PreambleStart = NextLine(tex);
	tex += Preamble + "\n";
	layout.LocalPreambleStart = NextLine(tex);
	tex += note.LocalPreamble + "\n\\begin{document}\n\\begin{cortexbody}\n";
	layout.BodyStart = NextLine(tex);
	tex += note.ProcessedBody + "\n\\end{cortexbody}\n\\end{document}\n";
	JSL::IO::writeString(texFile, tex);

	/// CHOOSE PDF OR XDV
	auto outFile = changeExtension(texFile, ProducePdf ? ".pdf" : ".xdv");
	auto logFile = changeExtension(texFile, ".log");

	// Remove previous outFiles (if they exist) so a failed run can't leave a stale result looking like a success
	JSL::IO::remove(outFile, JSL::IO::Policy::Generous);

	/// CONSTRUCT COMPILATION INSTRUCTIONS
	std::string mode = ProducePdf ? "" : "-no-pdf ";
	// cortex.cls lives in ClassDir, which isn't on TeX's search path; the trailing ':' keeps the default paths (starting with the note's own directory) after it
	// max_print_line stops TeX wrapping log lines at 79 characters, which could split the messages we search for
	// timeout kills a pass that never finishes (e.g. a recursive macro), turning a hung build into one failed note
	// -file-line-error makes TeX report errors as "<file>:<line>: <message>", which Diagnose relies on
	// -recorder writes a .fls listing every file TeX opened, so the note's dependencies (\input files, local packages, images) can be tracked
	auto cmd = "TEXINPUTS=" + Shell::Quote(ClassDir) + ": max_print_line=10000 timeout " + std::to_string(TimeoutSeconds) + " xelatex " + mode + "-interaction=nonstopmode -halt-on-error -file-line-error -recorder -output-directory=" + Shell::Quote(outDir) + " " + Shell::Quote(texFile);

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
			auto error = Diagnose(note, texFile, logFile, layout);
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
