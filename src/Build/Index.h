#pragma once
#include "BuildState.h"
#include "Note.h"
#include <map>
#include <string>
#include <vector>

//! The project-wide view of every note: what exists, what state its output is in, and how the notes link together. Pure data: it never touches the filesystem
//! @details Built from the full scan (so it lists every note, not just those compiled this run) merged with the BuildState.
//! Links are resolved here, not at compile time: a note's compiled output only contains the slug it links to, so renaming
//! or re-aliasing a note elsewhere changes the index, never the compiled notes.
class Index
{
  public:
	enum class Status
	{
		Ok,		//!< the output is up to date
		Stale,	//!< the latest version failed to compile; the output is the last good version
		Failed, //!< the note has never compiled; there is no output
	};

	//! One distinct link target written in a note, and what it resolved to
	struct ResolvedLink
	{
		std::string Slug;
		//! The id of the note it points at, or empty if no note has that title or alias
		std::string Target;
	};

	struct Entry
	{
		std::string Id;
		std::string Title;
		std::string Path;
		Status State;
		std::string Diagnostic;
		//! The slugs this note can be linked by: its title and aliases
		std::vector<std::string> Names;
		//! Each distinct target linked to from this note, in order of first appearance
		std::vector<ResolvedLink> Links;
		//! The ids of notes which link to this one (excluding itself), sorted
		std::vector<std::string> Backlinks;
	};

	Index(const std::vector<Note> &notes, const BuildState &state);

	const std::vector<Entry> &Entries() const;

	//! Slugs claimed by more than one note, with the ids claiming them (links to these resolve to the nearest claimant)
	const std::map<std::string, std::vector<std::string>> &Clashes() const;

	//! The number of links (counting each distinct target once per note) which match no note
	size_t UnresolvedCount() const;

	//! The contents of index.js: a call to CortexIndex([...]) with one object per note
	std::string Serialise() const;

	//! Chooses between notes claiming the same slug: the one whose folder shares the most leading folders with the linking note's,
	//! then the shallower one, then by id (so the choice is always deterministic)
	static std::string Nearest(const std::string &fromPath, const std::vector<const Entry *> &candidates);

  private:
	std::vector<Entry> List;
	std::map<std::string, std::vector<std::string>> ClashList;
};
