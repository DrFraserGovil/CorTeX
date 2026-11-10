#pragma once
#include "Notes/Note.h"
#include "Settings/Config.h"
#include "Warm.h"
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

//! Compiling a note's page with XeLaTeX (DESIGN.md §6.1): the note's fragment, wrapped in the project preamble, the hoisted local
//! preambles and the engine's declarations, to XDV (for SVG) or PDF
namespace Tex
{
	//! Everything about compiling one note which depends on other notes, worked out beforehand (see Build::Plan)
	struct Job
	{
		const Notes::Note *Note = nullptr;
		//! Notes transcluded into this one (directly or not, cycles excluded), in first-visit order: their local preambles are hoisted
		//! after the note's own, and errors in their fragments are mapped back to them
		std::vector<const Notes::Note *> Hoisted;
		//! TeX lines placed before \begin{document}: where each link, transclusion and label reference on the page goes
		std::vector<std::string> Declarations;
		bool ShowTitle = true;
	};

	struct Result
	{
		//! The XDV (or PDF), or nullopt if compilation failed
		std::optional<std::filesystem::path> Output;
		//! A one-line explanation of the failure; empty on success
		std::string Error;
		//! Every existing file TeX read (absolute, normalised, from the -recorder .fls file); empty on failure
		std::set<std::filesystem::path> Inputs;
		//! The labels the page defined, with what \ref prints for each (from the .aux)
		std::map<std::string, std::string> Labels;
		//! LaTeX's warnings about labels (multiply defined, or referenced but never defined)
		std::vector<std::string> LabelWarnings;
		//! If a warm worker failed to compile the page, so it was compiled afresh: where to see why (the warm run's log)
		std::string WarmFailure;
	};

	class Compiler
	{
	  public:
		//! Writes cortex.cls into classDir (unless it's already there)
		//! @param buildDir Where each note's .tex, .aux, .log and output go, at its id
		//! @param fragmentDir Where the notes' fragments are (see FragmentName)
		//! @param warm Warm workers to compile with (see WarmPool), kept by whoever builds the project repeatedly; prepared here, with
		//! warmDir as their folder (and root, where they wait until a note is compiled). Without, every pass starts XeLaTeX afresh
		Compiler(std::filesystem::path buildDir, std::filesystem::path classDir, std::filesystem::path fragmentDir, std::string preamble,
				 Settings::Compile settings, WarmPool *warm = nullptr, const std::filesystem::path &warmDir = {}, const std::filesystem::path &root = {});

		//! Compiles a note's page, running XeLaTeX until its references settle (at most MaxPasses times). TeX runs in the note's own
		//! folder, so its relative inputs and images resolve as they would for the author. With warm workers (and warm), each pass is
		//! given to one, waiting in that folder; if any warm pass fails, the note is compiled again without them, so warm workers can
		//! never make a note fail which would otherwise compile, and its errors are always reported as they would be
		Result Compile(const Job &job, bool warm = true) const;

		//! The name (as given to \input, without .tex) of a note's fragment: from its key, so it can never match a path in the project
		static std::string FragmentName(const Notes::Note &note);

		static constexpr int MaxPasses = 3;
		//! The longest a single pass may run before it is stopped (guards against a note which loops forever)
		static constexpr std::chrono::seconds Timeout{60};

	  private:
		std::filesystem::path BuildDir;
		//! Holds only cortex.cls: the one folder added to TeX's search path ahead of the fragments. The build folder must never be on it,
		//! because it mirrors note ids (so project paths): \input{sub/note} would find the generated wrapper instead of the author's file
		std::filesystem::path ClassDir;
		std::filesystem::path FragmentDir;
		std::string Preamble;
		Settings::Compile Settings;
		WarmPool *Warm = nullptr;

		//! The XeLaTeX command line every pass shares (interaction mode, job name and file aside), and the environment it runs in
		std::vector<std::string> Arguments() const;
		std::vector<std::pair<std::string, std::string>> Environment() const;
		//! The first line of every page: the class, with the settings as its options
		std::string ClassLine() const;

		struct Page;
		//! The page compiled by warm workers; nothing if a pass failed (or no worker could be had), and it should be compiled afresh
		std::optional<Result> CompileWarm(const Job &job, const Page &page) const;

		//! Where one line of a generated .tex file came from, so TeX's line numbers can be mapped back to the files the author wrote
		struct LineSource
		{
			//! Relative to the root; empty for lines the engine generated
			std::string File;
			size_t Line = 0;
			std::string Context;
		};
		std::string Diagnose(const Job &job, const std::filesystem::path &texFile, const std::filesystem::path &logFile, const std::vector<LineSource> &sources) const;
	};

	//! The files TeX opened, from its -recorder output
	std::set<std::filesystem::path> ReadInputs(const std::filesystem::path &flsFile);

	//! The labels in a .aux file, with what \ref prints for each (the first group of each \newlabel)
	std::map<std::string, std::string> ReadLabels(const std::filesystem::path &auxFile);

	//! Encodes text as hex digits, so no file or label name can upset TeX or the output (a PDF hex string, an SVG attribute)
	std::string Hex(std::string_view text);
} // namespace Tex
