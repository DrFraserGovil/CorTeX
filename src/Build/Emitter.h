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

	//! Display settings for the viewer: they ride along in index.js, so changing them never recompiles a note
	struct ViewerConfig
	{
		//! The default display scale for notes (1 = true printed size)
		double Zoom = 1;
		//! The project's name, shown on the overview page and in the window title
		std::string Name;
		//! The id of the note the viewer opens on; empty means the constructed overview
		std::string Home;
		//! The page colour behind the notes, as any CSS colour; empty for the default
		std::string Background;
		//! The graph's pull between linked notes in neighbouring folders, relative to the same folder (0 to 1)
		double FolderPull = 0.5;
		//! The sidebar logo as a data: URI; empty for none
		std::string Logo;
		//! The names of the editors the Edit button offers (the commands stay in the settings: links carry only a name)
		std::vector<std::string> Editors;
		//! The project's absolute root, which the viewer's cortex:// links name
		std::string Root;
	};

	//! Writes index.js (the serialised Index, followed by the viewer's settings) and the stub index.html viewer
	//! @returns true if index.js changed (so an open viewer should refresh)
	bool WriteIndex(std::string_view indexJs, const ViewerConfig &config);

	//! Writes version.js, which an open viewer polls: a new stamp tells it to reload the index and the note on show.
	//! Written last in a build, so by the time the stamp changes, everything it refers to is in place
	void WriteVersion();

	//! The files the viewer needs besides the notes (index.js, index.html), so the sweep knows to keep them
	std::vector<std::filesystem::path> ViewerFiles() const;

  private:
	std::filesystem::path OutputDir;
};
