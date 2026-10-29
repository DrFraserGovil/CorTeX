#pragma once
#include "../Syntax/Syntax.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

//! A single source file: its identity, header metadata, local preamble and LaTeX body. Knows nothing about other notes.
//! @details The file is split by separator lines (five or more dashes) into up to three sections:
//! 1 section: body only (no metadata, so no title: the project will refuse to build it);
//! 2 sections: metadata / body;
//! 3+ sections: metadata / local preamble / body, where any further separators are treated as body text.
class Note
{
  public:
	//! Path relative to the archive root
	std::filesystem::path RelativePath;
	//! Path of the source
	std::filesystem::path SourceAbsolutePath;

	//! The explicit `id:` header if present, else a slug of the relative path (sans extension)
	std::string Id;

	//! A hash of the id: a name for the note made only of hex digits, so it is safe anywhere in TeX (macro names, file names)
	//! and in URLs, whatever characters an explicit id contains
	std::string Key;

	//! The `title:` header. Empty if the note has none, which makes it unbuildable (it can't be referenced)
	std::string Title;

	//! The `alias:` header entries (comma-separated), as written. Together with the title, these are what [[links]] can match
	std::vector<std::string> Aliases;

	//! The `redirect:` header, as written: if set, the note is a stub which points at that note (a title or alias) instead of
	//! having content. Stubs are never compiled; the viewer forwards them, and links/transclusions in PDF mode resolve through them
	std::string Redirect;

	//! True if the header says `graph: hidden`: the note is left out of every graph except its own (for navigation pages
	//! such as indexes, which everything links to and which would otherwise pull every graph into a star)
	bool GraphHidden = false;

	//! The note's `show-title: yes|no` header, overriding the project's insert-title setting for this note; empty if absent
	std::optional<bool> ShowTitle;

	//! The note's `todo:` header: set if the note is flagged as needing work, holding what's left to do (empty if no reason was given;
	//! `todo: yes` and a bare `todo:` flag it without one, `todo: no` doesn't flag it). Display-only: never recompiles the note
	std::optional<std::string> Todo;

	//! The `sidebar:` header entries (comma-separated, each "target" or "target|Label"), as written: extra sidebar links shown with this note
	std::vector<std::string> Sidebar;

	//! Preamble material specific to this note, placed after the project preamble (empty if absent)
	std::string LocalPreamble;

	//! The LaTeX content of the note, as written
	std::string Body;

	//! Body with CorTeX's extended syntax rewritten into LaTeX (same number of lines): this is what gets compiled
	std::string ProcessedBody;

	//! The [[links]] found in the body
	std::vector<Syntax::Link> Links;

	//! The ![[transclusions]] found in the body
	std::vector<Syntax::Link> Transclusions;

	//! The 1-based line in the source file at which LocalPreamble starts (0 if there is none), for mapping TeX errors back to the source
	size_t LocalPreambleLine = 0;

	//! The 1-based line in the source file at which Body starts, for mapping TeX errors back to the source
	size_t BodyLine = 1;

	//! Reads and parses the file at notePath (an absolute path inside root), applying the extended syntax with the given options
	Note(const std::filesystem::path &root, const std::filesystem::path &notePath, const Syntax::Options &syntax = {});

	//! True if the file starts with a CorTeX header containing a title (i.e. it is a note, not a plain TeX fragment)
	static bool HasNoteHeader(const std::filesystem::path &file);

	//! Flags or unflags a note as needing work by rewriting only its header's `todo:` line(s): on adds a bare `todo:` (unless the note
	//! is already flagged, when its reason is kept); off removes every `todo:` line. Nothing else in the file changes; written atomically
	//! @returns true if the file was changed
	//! @throws runtime_error If the file has no header
	static bool SetTodo(const std::filesystem::path &file, bool on);

  private:
	void ParseHeader(const std::vector<std::string> &lines);
	static std::string Slug(const std::filesystem::path &relativePath);
};
