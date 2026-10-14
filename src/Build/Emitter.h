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

	//! Copies a compiled PDF to notes/<id>.pdf
	void WritePdf(const Note &note, const std::filesystem::path &pdf);

	//! Writes index.js (calls CortexIndex([...]) when loaded) and the stub index.html viewer
	//! @param pdf If true, the viewer opens notes/<id>.pdf rather than loading notes/<id>.js
	void WriteIndex(const std::vector<Note> &notes, bool pdf);

  private:
	std::filesystem::path OutputDir;
};
