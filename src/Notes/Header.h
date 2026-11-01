#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

//! The layout of a note file and its header (DESIGN.md §3): read here, and rewritten here, so the two can never disagree
namespace Notes
{
	//! A line of five or more '-' (or five or more '='), ignoring surrounding spaces. Five, so that TeX's en- and em-dashes (-- and
	//! ---) on a line of their own are never mistaken for one
	bool IsSeparator(std::string_view line);

	//! A file split at its first two separators
	struct Sections
	{
		//! False if the file has no separator, so no header (and so isn't a note)
		bool HasHeader = false;
		std::vector<std::string> Header;
		std::string LocalPreamble;
		std::string Body;
		//! The 1-based lines of the file at which LocalPreamble (0 if there is none) and Body start, for mapping messages back to the file
		size_t LocalPreambleLine = 0;
		size_t BodyLine = 1;
	};

	Sections Split(std::string_view text);

	struct HeaderProblem
	{
		//! 1-based line within the header (and so within the file)
		size_t Line;
		std::string Message;
	};

	//! The header fields (DESIGN.md §3.1)
	struct Header
	{
		//! Empty if the header has none, which means the file isn't a note
		std::string Title;
		std::vector<std::string> Aliases;
		//! The target as written, if the note is a stub; else empty
		std::string Redirect;
		bool GraphHidden = false;
		bool GraphLoose = false;
		//! Overrides the project's insert-title setting; empty if not given
		std::optional<bool> ShowTitle;
		//! Set if the note is flagged as needing work, holding the reason (empty if none was given)
		std::optional<std::string> Todo;
		//! Extra sidebar entries, each "target" or "target|Label", as written
		std::vector<std::string> Sidebar;

		//! Reads the header's lines: the first occurrence of a key wins, except that every line of a list field (alias, sidebar, graph)
		//! adds to it; unknown keys are ignored; unknown values are problems
		static Header Parse(const std::vector<std::string> &lines, std::vector<HeaderProblem> &problems);
	};

	//! Flags or unflags a note as needing work by rewriting only its header's `todo:` line(s): on adds a bare `todo:` (unless the note
	//! is already flagged, when its reason is kept); off removes every `todo:` line. Nothing else in the file changes; written atomically
	//! @returns true if the file was changed
	//! @throws runtime_error If the file has no header
	bool SetTodo(const std::filesystem::path &file, bool on);
} // namespace Notes
