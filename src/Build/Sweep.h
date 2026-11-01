#pragma once
#include "Notes/Note.h"
#include <filesystem>
#include <vector>

namespace Build
{
	//! Removes everything in the output folder which the build didn't write (DESIGN.md §11: CorTeX owns it): deleted or renamed notes,
	//! the other mode's files after a mode switch, temporary files from an interrupted write, anything else put there
	void SweepOutput(const std::filesystem::path &outputDir, const std::vector<Notes::Note> &notes, bool pdf);

	//! Removes LaTeX's working files for notes which no longer exist, and fragments of notes which no longer exist
	void SweepScratch(const std::filesystem::path &buildDir, const std::filesystem::path &fragmentDir, const std::vector<Notes::Note> &notes);

	//! Removes now-empty folders under (but not including) root, deepest first
	void RemoveEmptyFolders(const std::filesystem::path &root);
} // namespace Build
