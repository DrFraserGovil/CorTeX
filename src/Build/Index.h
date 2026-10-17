#pragma once
#include "BuildState.h"
#include "Note.h"
#include <string>
#include <vector>

//! The project-wide view of every note: what exists and what state its output is in. Pure data: it never touches the filesystem
//! @details Built from the full scan (so it lists every note, not just those compiled this run) merged with the BuildState.
//! Links, aliases and backlinks will live here too (M4).
class Index
{
  public:
	enum class Status
	{
		Ok,		//!< the output is up to date
		Stale,	//!< the latest version failed to compile; the output is the last good version
		Failed, //!< the note has never compiled; there is no output
	};

	struct Entry
	{
		std::string Id;
		std::string Title;
		std::string Path;
		Status State;
		std::string Diagnostic;
	};

	Index(const std::vector<Note> &notes, const BuildState &state);

	const std::vector<Entry> &Entries() const;

	//! The contents of index.js: a call to CortexIndex([...]) with one object per note
	std::string Serialise() const;

  private:
	std::vector<Entry> List;
};
