#include "TexRunner.h"
#include "Shell.h"
#include <JSL.h>

// The class every note is compiled against. The [svg] option (the default) loads the dvisvgm drivers, which
// must be in place before any user package loads pgf, graphicx or xcolor; [pdf] leaves the native drivers alone.
static constexpr std::string_view CortexClass = R"tex(\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{cortex}[CorTeX note class]
\newif\ifcortex@svg
\cortex@svgtrue
\DeclareOption{svg}{\cortex@svgtrue}
\DeclareOption{pdf}{\cortex@svgfalse}
\DeclareOption*{\PassOptionsToClass{\CurrentOption}{article}}
\ProcessOptions\relax
\ifcortex@svg
	\def\pgfsysdriver{pgfsys-dvisvgm.def}
	\PassOptionsToPackage{dvisvgm}{graphicx}
	\PassOptionsToPackage{dvisvgm}{graphics}
	\PassOptionsToPackage{dvisvgm}{xcolor}
\fi
\LoadClass{article}
)tex";

TexRunner::TexRunner(std::filesystem::path buildDir, std::string_view preamble, bool producePdf) : BuildDir(std::move(buildDir)), Preamble(std::move(preamble)), ProducePdf(producePdf)
{
	WriteClass();
}

std::string_view TexRunner::ClassSource()
{
	return CortexClass;
}

void TexRunner::WriteClass()
{
	auto target = BuildDir / "cortex.cls";
	if (std::filesystem::exists(target) && JSL::IO::getFile(target) == CortexClass)
	{
		LOG(DEBUG) << "cortex.cls is up to date";
		return;
	}

	if (auto made = JSL::IO::mkdir(BuildDir, JSL::IO::Policy::Generous); !made)
	{
		LOG(ERROR) << "Could not create the build directory " << BuildDir << ": " << made.Message;
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

std::optional<std::filesystem::path> TexRunner::Compile(const Note &note)
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
	std::string documentClass = ProducePdf ? "\\documentclass[pdf]{cortex}\n" : "\\documentclass[svg]{cortex}\n";
	JSL::IO::writeString(texFile, documentClass + Preamble + "\n" + note.LocalPreamble + "\n\\begin{document}\n" + note.Body + "\n\\end{document}\n");

	/// CHOOSE PDF OR XDV
	auto outFile = changeExtension(texFile, ProducePdf ? ".pdf" : ".xdv");
	auto logFile = changeExtension(texFile, ".log");

	// Remove previous outFiles (if they exist) so a failed run can't leave a stale result looking like a success
	JSL::IO::remove(outFile, JSL::IO::Policy::Generous);

	/// CONSTRUCT COMPILATION INSTRUCTIONS
	std::string mode = ProducePdf ? "" : "-no-pdf ";
	// cortex.cls lives in BuildDir, which isn't on TeX's search path; the trailing ':' keeps the default paths after it
	// max_print_line stops TeX wrapping log lines at 79 characters, which could split the rerun messages we search for
	auto cmd = "TEXINPUTS=" + Shell::Quote(BuildDir) + ": max_print_line=10000 xelatex " + mode + "-interaction=nonstopmode -halt-on-error -output-directory=" + Shell::Quote(outDir) + " " + Shell::Quote(texFile);

	/// RUN COMPILER UNTIL REFERENCES SETTLE
	// Each pass resolves references using the .aux written by the pass before; LaTeX says in the log when that has changed things
	auto stdFile = changeExtension(texFile, ".stdcout");
	for (int pass = 1; pass <= MaxPasses; ++pass)
	{
		LOG(DEBUG) << "xelatex pass " << pass << " for " << note.RelativePath;
		int code = Shell::Run(cmd, (note.SourceAbsolutePath).parent_path(), stdFile);

		/// CHECK IF IT WORKED
		if (code != 0 || !std::filesystem::exists(outFile))
		{
			LOG(WARN) << "Failed to compile " << note.RelativePath << " (xelatex exit code " << code << "); see " << logFile;
			return std::nullopt;
		}

		/// CHECK IF ANOTHER PASS IS NEEDED
		if (!NeedsRerun(JSL::IO::getFile(logFile)))
		{
			return outFile;
		}
	}

	LOG(WARN) << "References in " << note.RelativePath << " had not settled after " << MaxPasses << " passes; some may be wrong. See " << logFile;
	return outFile;
}
