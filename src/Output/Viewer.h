#pragma once
#include "Build/BuildState.h"
#include "Network/Network.h"
#include "Report/Report.h"
#include "Settings/Config.h"
#include <filesystem>
#include <string>

/*! The viewer's files (DESIGN.md §8): index.html (the viewer itself, from src/Viewer), index.js (the project), notes/<id>.js (one per
	note) and version.js (polled for live refresh). They are scripts, not JSON, because a page opened from file:// can't fetch files,
	but can load scripts: each is a call to a function the viewer defines.

	index.js holds three calls:
	- CortexIndex([note, ...]), one object per note:
		id, key, title, path            strings: the note's id, the hex key its links are written with, its title, its source path
		status                          "ok", "stale" (the last compile failed; the output is the last good one), "failed" (no output),
		                                or "redirect" (a stub)
		diagnostic                      why the last compile failed; empty otherwise
		names                           the slugs it can be linked by
		links                           {key: id or null}: each thing it refers to, by the key its links are written with (a slug, or
		                                "#label" for a [[#label]] reference); null if unresolved (a wanted note)
		loose                           [key, ...]: those left off the graph (only present if there are any)
		backlinks, transcludes          [id, ...]
		sidebar                         [{heading, items: [{label, slug, target: id or null}, ...]}, ...]: its own sidebar (copied
		                                sidebars, then its own entries), by heading; "" for entries under none
		graphHidden: true, todo: text, redirect: id or null, redirectText: text   only present when they apply
	- CortexSidebar([{heading, items: [{label, slug, target}, ...]}, ...]): the global sidebar
	- CortexConfig({...}): format (this layout's version, 1), name, home (an id, or empty for the overview), zoom, background, logo (a
	  data: URI, or empty), graphLayout, folderPull, folderRepulsion, graphColours, root (the project's absolute path; empty where the viewer may not act
	  on the project), editors ([name, ...]), live (whether to poll version.js)

	notes/<id>.js holds CortexNote({id, title, pages: [svg, ...]}); version.js holds CortexVersion("<stamp>").
	A reference to a label is an SVG link to #/l/<id>/<hex label>; the label's position is an element with class cortex-label and
	data-label="<hex label>".
*/
namespace Output
{
	//! The version of the layout above: bumped whenever it changes incompatibly
	constexpr int Format = 1;

	//! What the viewer is told about the project
	struct ViewerOptions
	{
		std::string Name;
		//! A copy to share: no project path, no editors, no live refresh
		bool ForSharing = false;
	};

	//! The contents of index.js. The home note's setting is checked here, against the notes (a missing one is reported)
	std::string IndexJs(const Network::Network &network, const Build::BuildState &state, const Settings::Config &config, const ViewerOptions &options,
						Report::Log &report);

	//! Writes notes/<id>.js
	void WriteNote(const std::filesystem::path &outputDir, const Notes::Note &note, const std::string &svg);

	//! Writes index.js and index.html
	//! @returns true if index.js changed (so an open viewer should refresh)
	bool WriteIndex(const std::filesystem::path &outputDir, const std::string &indexJs);

	//! Writes version.js with a new stamp, which tells an open viewer to reload. Written last in a build, so everything it refers to is in place
	void WriteVersion(const std::filesystem::path &outputDir);
} // namespace Output
