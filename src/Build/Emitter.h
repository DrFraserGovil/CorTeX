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

	//! Writes notes/<id>.js, which calls CortexNote({...}) when loaded
	void WriteNote(const Note &note, const std::vector<std::string> &pages);

	//! Copies a compiled PDF into the output directory, mirroring the note's source path (sub/Note.tex -> sub/Note.pdf)
	void WritePdf(const Note &note, const std::filesystem::path &pdf);

	//! Writes index.js (calls CortexIndex([...]) when loaded) and the stub index.html viewer
	void WriteIndex(const std::vector<Note> &notes);

  private:
	std::filesystem::path OutputDir;
};
