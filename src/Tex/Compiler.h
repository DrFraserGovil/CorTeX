#pragma once
#include "Notes/Note.h"
#include "Settings/Config.h"
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
	};

	class Compiler
	{
	  public:
		//! Writes cortex.cls into classDir (unless it's already there)
		//! @param buildDir Where each note's .tex, .aux, .log and output go, at its id
		//! @param fragmentDir Where the notes' fragments are (see FragmentName)
		Compiler(std::filesystem::path buildDir, std::filesystem::path classDir, std::filesystem::path fragmentDir, std::string preamble,
				 Settings::Compile settings);

		//! Compiles a note's page, running XeLaTeX until its references settle (at most MaxPasses times). TeX runs in the note's own
		//! folder, so its relative inputs and images resolve as they would for the author
		Result Compile(const Job &job) const;

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
