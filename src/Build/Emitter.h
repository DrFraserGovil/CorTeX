#pragma once
#include "Note.h"
#include <filesystem>
#include <string>
#include <vector>

//! Writes the browser-facing output folder: one script per note, an index script, and the viewer page
class Emitter
{
  public:
	explicit Emitter(std::filesystem::path outputDir);

	//! Where a note's output lives: notes/<id>.js for SVG, or the source path with a .pdf extension for PDF
	//! @details The single source of truth for output locations: the writers and the post-build sweep both use it
	std::filesystem::path OutputFor(const Note &note, bool pdf) const;

	//! Writes notes/<id>.js, which calls CortexNote({...}) when loaded
	void WriteNote(const Note &note, const std::vector<std::string> &pages);

	//! Copies a compiled PDF into the output directory, mirroring the note's source path (sub/Note.tex -> sub/Note.pdf)
	void WritePdf(const Note &note, const std::filesystem::path &pdf);

	//! Writes index.js (the serialised Index) and the stub index.html viewer
	void WriteIndex(std::string_view indexJs);

	//! The files the viewer needs besides the notes (index.js, index.html), so the sweep knows to keep them
	std::vector<std::filesystem::path> ViewerFiles() const;

  private:
	std::filesystem::path OutputDir;
};
