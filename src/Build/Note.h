#pragma once
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

	//! Preamble material specific to this note, placed after the project preamble (empty if absent)
	std::string LocalPreamble;

	//! The LaTeX content of the note
	std::string Body;

	//! The 1-based line in the source file at which LocalPreamble starts (0 if there is none), for mapping TeX errors back to the source
	size_t LocalPreambleLine = 0;

	//! The 1-based line in the source file at which Body starts, for mapping TeX errors back to the source
	size_t BodyLine = 1;

	//! Reads and parses the file at root/relativePath
	Note(const std::filesystem::path &root, const std::filesystem::path &notePath);

  private:
	void ParseHeader(const std::vector<std::string> &lines);
	static std::string Slug(const std::filesystem::path &relativePath);
};
