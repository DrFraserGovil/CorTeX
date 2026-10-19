#pragma once
#include "../Syntax/Syntax.h"
#include <filesystem>
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

	//! The `title:` header. Empty if the note has none, which makes it unbuildable (it can't be referenced)
	std::string Title;

	//! The `alias:` header entries (comma-separated), as written. Together with the title, these are what [[links]] can match
	std::vector<std::string> Aliases;

	//! Preamble material specific to this note, placed after the project preamble (empty if absent)
	std::string LocalPreamble;

	//! The LaTeX content of the note, as written
	std::string Body;

	//! Body with CorTeX's extended syntax rewritten into LaTeX (same number of lines): this is what gets compiled
	std::string ProcessedBody;

	//! The [[links]] found in the body
	std::vector<Syntax::Link> Links;

	//! The 1-based line in the source file at which LocalPreamble starts (0 if there is none), for mapping TeX errors back to the source
	size_t LocalPreambleLine = 0;

	//! The 1-based line in the source file at which Body starts, for mapping TeX errors back to the source
	size_t BodyLine = 1;

	//! Reads and parses the file at notePath (an absolute path inside root), applying the extended syntax with the given options
	Note(const std::filesystem::path &root, const std::filesystem::path &notePath, const Syntax::Options &syntax = {});

	//! True if the file starts with a CorTeX header containing a title (i.e. it is a note, not a plain TeX fragment)
	static bool HasNoteHeader(const std::filesystem::path &file);

  private:
	void ParseHeader(const std::vector<std::string> &lines);
	static std::string Slug(const std::filesystem::path &relativePath);
};
