#pragma once
#include "BuildState.h"
#include "Scan.h"
#include "Tex/Warm.h"
#include <functional>

namespace Build
{
	//! Bumped whenever a change to CorTeX changes what notes compile to, so every note is recompiled
	constexpr int EngineVersion = 15;

	//! Checks the programs a build needs can be found (DESIGN.md §13.1)
	//! @throws runtime_error Naming those which can't
	void CheckPrerequisites(bool pdf);

	//! Told as notes compile: the note about to compile (its path in the project), and its place (from 1) among the notes this round
	//! compiles. If compiling changes label numbers, a second round recompiles the notes showing them, counting from 1 again
	using Progress = std::function<void(const std::filesystem::path &note, size_t number, size_t of)>;

	//! What a build may be given besides the project, by whoever runs it
	struct Options
	{
		//! Told of each note as it compiles
		Progress Told;
		//! Warm workers to compile with, kept between builds by a process which builds the project repeatedly (a watch, a language
		//! server); without, every pass starts XeLaTeX afresh
		Tex::WarmPool *Warm = nullptr;
	};

	//! Brings the output up to date (DESIGN.md §6.6), compiling only the notes whose inputs have changed, straight into the output:
	//! each output is replaced atomically, and a failure leaves the last good version in place. Everything worth telling the author goes
	//! to report
	void Update(const Scan &scan, Report::Log &report, const Options &options = {});

	//! Recompiles every note from scratch into a staging folder, which replaces the output only once it is complete (and only if not
	//! every note failed)
	//! @throws runtime_error If every note failed
	void Rebuild(const Scan &scan, Report::Log &report, const Options &options = {});

	//! The index.js for the project as last built
	std::string IndexJs(const Scan &scan, const BuildState &state, bool forSharing, Report::Log &report);
} // namespace Build
