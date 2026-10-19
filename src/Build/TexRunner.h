#pragma once
#include "Note.h"
#include <filesystem>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>
#include <string_view>

//! Everything about a note's compilation which depends on other notes, worked out by the project before compiling
struct CompilePlan
{
	//! Notes transcluded into this one (directly or indirectly, cycles excluded), in first-visit order: their local preambles are
	//! hoisted after the note's own, and errors in their fragments are mapped back to them
	std::vector<const Note *> Hoisted;
	//! PDF mode: (origin key, slug, path of the target PDF relative to this note's PDF), for the links of this note and of every hoisted note
	std::vector<std::tuple<std::string, std::string, std::string>> LinkTargets;
	//! (origin key, slug, key of the transcluded note), for every transclusion in this note or a hoisted note which can be made
	std::vector<std::tuple<std::string, std::string, std::string>> Transclusions;
	//! (origin key, slug, message), for every transclusion which can't be made (no such note, or it would loop back on itself)
	std::vector<std::tuple<std::string, std::string, std::string>> TransclusionProblems;
};

//! Wraps a note body in the archive preamble and compiles it with xelatex, to either XDV (for SVG conversion) or PDF
//! @details Owns cortex.cls, which every note is compiled against: it is (re)written into its own directory (ClassDir, beside the build directory) on construction, and selected by the generated \documentclass line, so the output-mode drivers live in exactly one place.
class TexRunner
{
  public:
	//! @param buildDir The scratch directory in which .tex/.xdv/.log files are written
	//! @param preamble The archive preamble (packages and macros; the \documentclass line is generated)
	//! @param producePdf If true, xelatex runs to completion and produces a PDF; otherwise it stops at XDV
	//! @param widthCM The fixed width of every note's body (the cortexbody minipage)
	//! @param insertTitle If true, each note starts with \CortexTitle{<its title>}
	//! @throws runtime_error If cortex.cls cannot be written
	TexRunner(std::filesystem::path buildDir, std::string_view preamble, bool producePdf, double widthCM, bool insertTitle = false);

	//! Compiles the note. The working directory is the note's source directory, so relative inputs and images resolve as they would for the author.
	//! @details xelatex is rerun (up to MaxPasses times) while the log asks for it, so cross-references are correct regardless of what earlier builds left behind.
	//! The outcome of compiling one note
	struct Result
	{
		//! The resulting XDV (or PDF) file, or nullopt if compilation failed
		std::optional<std::filesystem::path> Output;
		//! A one-line explanation of the failure (empty on success); also logged
		std::string Error;
		//! Every existing file TeX read during a successful compilation (absolute, normalised, from the -recorder .fls file); empty on failure
		std::set<std::filesystem::path> Inputs;
	};

	//! @details The note's body is never inlined: the generated wrapper \input's the note's fragment (written by the project), exactly as
	//! a transclusion of the note would, so a note looks the same on its own page and wherever it is transcluded
	//! @param plan The note's transclusions, hoisted preambles and (PDF mode) link targets
	//! @returns The output file on success, or the reason for failure
	Result Compile(const Note &note, const CompilePlan &plan = {});

	//! The name (as given to \input, without .tex) of a note's fragment file: built from its key, so it can never match a path in the project
	static std::string FragmentName(const Note &note);

	//! The source of cortex.cls, as compiled into this binary (needed by anything hashing the build inputs)
	static std::string_view ClassSource();

	//! The maximum number of xelatex passes per note, before giving up on references settling
	static constexpr int MaxPasses = 3;

	//! The longest a single xelatex pass may run before it is killed (guards against infinite loops in a note)
	static constexpr int TimeoutSeconds = 60;

  private:
	std::filesystem::path BuildDir;
	//! Holds only cortex.cls: the one directory added to TeX's search path. The build directory must never be on it, because it
	//! mirrors note ids (i.e. project paths), so \input{sub/note} would find the generated wrapper instead of the user's file
	std::filesystem::path ClassDir;
	//! Holds the notes' fragments (their processed bodies), under key-based names; on TeX's search path after ClassDir
	std::filesystem::path FragmentDir;
	std::string Preamble;
	bool ProducePdf;
	double WidthCM;
	bool InsertTitle;

	//! Writes ClassSource() to ClassDir/cortex.cls, unless an identical copy is already there
	void WriteClass();

	//! Where one line of a generated .tex file came from, so TeX's line numbers can be mapped back to the files the user wrote
	struct LineSource
	{
		//! The file, relative to the project root; empty for lines the engine generated itself
		std::string File;
		size_t Line = 0;
		//! Extra context for the message, e.g. "local preamble"
		std::string Context;
	};

	//! Reads the files TeX opened from its -recorder output
	static std::set<std::filesystem::path> ReadInputs(const std::filesystem::path &flsFile);

	//! Explains a failed compilation: the first error in the log, located in the user's own file where possible
	std::string Diagnose(const Note &note, const CompilePlan &plan, const std::filesystem::path &texFile, const std::filesystem::path &logFile, const std::vector<LineSource> &sources) const;
};
