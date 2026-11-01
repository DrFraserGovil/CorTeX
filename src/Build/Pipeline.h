#pragma once
#include "BuildState.h"
#include "Scan.h"

namespace Build
{
	//! Bumped whenever a change to CorTeX changes what notes compile to, so every note is recompiled
	constexpr int EngineVersion = 13;

	//! Checks the programs a build needs can be found (DESIGN.md §13.1)
	//! @throws runtime_error Naming those which can't
	void CheckPrerequisites(bool pdf);

	//! Brings the output up to date (DESIGN.md §6.6), compiling only the notes whose inputs have changed, straight into the output:
	//! each output is replaced atomically, and a failure leaves the last good version in place. Everything worth telling the author goes
	//! to report
	void Update(const Scan &scan, Report::Log &report);

	//! Recompiles every note from scratch into a staging folder, which replaces the output only once it is complete (and only if not
	//! every note failed)
	//! @throws runtime_error If every note failed
	void Rebuild(const Scan &scan, Report::Log &report);

	//! The index.js for the project as last built
	std::string IndexJs(const Scan &scan, const BuildState &state, bool forSharing, Report::Log &report);
} // namespace Build
