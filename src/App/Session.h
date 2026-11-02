#pragma once
#include "Build/Pipeline.h"
#include "Build/Scan.h"
#include "Project/Lock.h"
#include "Report/Report.h"
#include <filesystem>
#include <functional>

//! What the commands which hold a project (build, watch, the language server) share: one build, serving other commands' requests,
//! and which file changes matter
namespace App
{
	//! Given the scan a build used and its full report, once the build is done
	using Built = std::function<void(Build::Scan &&scan, const Report::Log &report)>;

	//! One build (or rebuild), told in full to the build log and briefly to the terminal; progress, if given, is told of each note as it
	//! compiles, and then built, if given, is called
	//! @throws runtime_error If the build couldn't happen at all (missing programs, clashing ids, every note failing); the log is written first
	void BuildOnce(const std::filesystem::path &root, bool clean, const Built &built = {}, const Build::Progress &progress = {});

	//! Copies the built output into folder, as a copy to share: no path on this machine, no editor tools, no live refresh (DESIGN.md §13.3)
	void ExportTo(const std::filesystem::path &root, const std::filesystem::path &folder);

	//! Does what other commands asked of the lock's holder; returns false once one asked it to stop
	bool Serve(const std::filesystem::path &root, Project::Lock &lock, const std::function<void(bool)> &build);

	//! Whether a change to a file (absolute, or relative to root) can change the build: not the output, nor CorTeX's own working files
	//! in .cortex, nor other hidden files (editor and version control noise); the project's own .cortex files do
	bool AffectsBuild(const std::filesystem::path &root, const std::filesystem::path &output, const std::filesystem::path &changed);
} // namespace App
