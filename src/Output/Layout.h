#pragma once
#include "Notes/Note.h"
#include <filesystem>
#include <vector>

//! Where everything goes in the output folder: the single source of truth, used by the writers and the sweep alike, so they can never
//! disagree about what belongs there
namespace Output
{
	//! A note's output: notes/<id>.js for the viewer, or (with pdf) the source path with a .pdf extension
	std::filesystem::path NoteFile(const std::filesystem::path &outputDir, const Notes::Note &note, bool pdf);

	//! The viewer's own files: index.html, index.js and version.js
	std::vector<std::filesystem::path> ViewerFiles(const std::filesystem::path &outputDir);

	//! Copies a compiled PDF into place, atomically (never a half-copied PDF in place of the old one)
	void WritePdf(const std::filesystem::path &outputDir, const Notes::Note &note, const std::filesystem::path &pdf);
} // namespace Output
