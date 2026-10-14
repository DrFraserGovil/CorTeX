#pragma once
#include "Note.h"
#include <filesystem>
#include <optional>
#include <string_view>

//! Wraps a note body in the archive preamble and compiles it with xelatex, to either XDV (for SVG conversion) or PDF
//! @details Owns cortex.cls, which every note is compiled against: it is (re)written into the build directory on construction, and selected by the generated \documentclass line, so the output-mode drivers live in exactly one place.
class TexRunner
{
  public:
	//! @param buildDir The scratch directory in which .tex/.xdv/.log files are written
	//! @param preamble The archive preamble (packages and macros; the \documentclass line is generated)
	//! @param producePdf If true, xelatex runs to completion and produces a PDF; otherwise it stops at XDV
	//! @throws runtime_error If cortex.cls cannot be written
	TexRunner(std::filesystem::path buildDir, std::string_view preamble, bool producePdf);

	//! Compiles the note. The working directory is the note's source directory, so relative inputs and images resolve as they would for the author.
	//! @returns The path to the resulting XDV (or PDF) file, or nullopt if compilation failed (the reason is logged)
	std::optional<std::filesystem::path> Compile(const Note &note);

	//! The source of cortex.cls, as compiled into this binary (needed by anything hashing the build inputs)
	static std::string_view ClassSource();

  private:
	std::filesystem::path BuildDir;
	std::string Preamble;
	bool ProducePdf;

	//! Writes ClassSource() to BuildDir/cortex.cls, unless an identical copy is already there
	void WriteClass();
};
