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
	auto cmd = "TEXINPUTS=" + Shell::Quote(BuildDir) + ": xelatex " + mode + "-interaction=nonstopmode -halt-on-error -output-directory=" + Shell::Quote(outDir) + " " + Shell::Quote(texFile);

	/// RUN COMPILER
	auto stdFile = changeExtension(texFile, ".stdcout");
	int code = Shell::Run(cmd, (note.SourceAbsolutePath).parent_path(), stdFile);

	/// CHECK IF IT WORKED
	if (code != 0 || !std::filesystem::exists(outFile))
	{
		LOG(WARN) << "Failed to compile " << note.RelativePath << " (xelatex exit code " << code << "); see " << logFile;
		return std::nullopt;
	}
	return outFile;
}
