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
		Failed,	  //!< the note has never compiled; there is no output
		Redirect, //!< a stub which forwards to another note (never compiled)
	};

	//! One distinct link target written in a note, and what it resolved to
	struct ResolvedLink
	{
		std::string Slug;
		//! The id of the note it points at, or empty if no note has that title or alias
		std::string Target;
	};

	//! One sidebar link: written like a link target, optionally with a label ("target|Label")
	struct SidebarItem
	{
		//! The target as written
		std::string Text;
		//! The explicit label, or empty to use the target note's title
		std::string Label;
		std::string Slug;
		//! The id of the note it resolves to; empty if none
		std::string Target;

		//! Parses "target" or "target|Label"
		static SidebarItem Parse(std::string_view entry);
		//! What to show: the explicit label, else the target's title, else the text as written
		std::string Shown(const std::map<std::string, std::string> &titles) const;
	};

	//! A run of sidebar items under an optional heading
	struct SidebarGroup
	{
		std::string Heading;
		std::vector<SidebarItem> Items;
	};

	struct Entry
	{
		std::string Id;
		//! The note's key (a hex hash of its id), as written into its links and transclusions
		std::string Key;
		std::string Title;
		std::string Path;
		Status State;
		std::string Diagnostic;
		//! The slugs this note can be linked by: its title and aliases
		std::vector<std::string> Names;
		//! Each distinct target linked to or transcluded by this note, in order of first appearance (resolved from this note's position)
		std::vector<ResolvedLink> Links;
		//! The ids of notes which link to this one (excluding itself), sorted
		std::vector<std::string> Backlinks;
		//! The ids of the notes this one transcludes (resolved from this note, not following redirects), in order; a subset of Links' targets
		std::vector<std::string> Transcludes;
		//! For a redirect stub: its redirect as written, and the id it resolves to (empty if no such note). Empty text for ordinary notes
		std::string RedirectText;
		std::string RedirectTarget;
		//! Left out of every graph except its own (the note's `graph: hidden` header)
		bool GraphHidden = false;
		//! Flagged as needing work (the note's `todo:` header), and what's left to do (may be empty)
		bool Todo = false;
		std::string TodoText;
		//! The note's own sidebar entries (its `sidebar:` header), resolved from the note, shown beneath the global sidebar when it is open
		std::vector<SidebarItem> Sidebar;
	};

	//! @param sidebar The project's global sidebar (from registry.ctx); its entries are resolved here, with no origin note
	Index(const std::vector<Note> &notes, const BuildState &state, std::vector<SidebarGroup> sidebar = {});

	//! The global sidebar, resolved
	const std::vector<SidebarGroup> &Sidebar() const;

	const std::vector<Entry> &Entries() const;

	//! Slugs claimed by more than one note, with the ids claiming them (links to these resolve to the nearest claimant)
	const std::map<std::string, std::vector<std::string>> &Clashes() const;

	//! The number of links (counting each distinct target once per note) which match no note
	size_t UnresolvedCount() const;

	//! Follows redirect stubs from a note id to the note they finally point at
	//! @returns The final id (the id itself if it isn't a stub); empty if a redirect is unresolved or the chain loops
	std::string FollowRedirects(const std::string &id) const;

	//! The contents of index.js: a call to CortexIndex([...]) with one object per note
	std::string Serialise() const;

	//! Chooses between notes claiming the same slug: the one whose folder shares the most leading folders with the linking note's,
	//! then the shallower one, then by id (so the choice is always deterministic)
	static std::string Nearest(const std::string &fromPath, const std::vector<const Entry *> &candidates);

  private:
	std::vector<Entry> List;
	std::vector<SidebarGroup> GlobalSidebar;
	std::map<std::string, std::vector<std::string>> ClashList;
};
