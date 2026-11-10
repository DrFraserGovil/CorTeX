#pragma once
#include "Notes/Note.h"
#include "Project/Registry.h"
#include "Report/Report.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

//! How the notes connect (DESIGN.md §4.3–4.4, §5): what each reference resolves to, backlinks, redirects, wanted notes, strength and
//! label owners. Pure: built from the notes alone (never the filesystem, never build results), so a renamed note changes only this
namespace Network
{
	//! One distinct thing a note refers to, and what it resolved to (from the note: DESIGN.md §4.3)
	struct Connection
	{
		//! How the viewer looks it up: the target's slug, or "#label" for a [[#label]] reference, which has no target name
		std::string Key;
		//! The id it resolves to (not following redirects); empty if it matches nothing
		std::string Target;
		//! Left off the graph: every reference to it is marked loose, or unmarked into a `graph: loose` note
		bool Loose = false;
	};

	//! A label reference ([[#label]] or [[target#label]]), resolved
	struct LabelReference
	{
		//! The Connection key it belongs to: the target's slug, or "#label"
		std::string Key;
		std::string Label;
		//! The note which owns the label (after redirects, for [[target#label]]); empty if the label wasn't found there (or anywhere)
		std::string Owner;
	};

	struct SidebarItem
	{
		std::string Text;
		std::string Label;
		std::string Slug;
		//! The id it resolves to; empty if none
		std::string Target;
	};

	struct SidebarGroup
	{
		std::string Heading;
		std::vector<SidebarItem> Items;
	};

	struct Node
	{
		const Notes::Note *Source;
		std::string Id;
		//! The slugs it can be linked by: its title's and its aliases', sorted
		std::vector<std::string> Names;
		//! Each distinct thing it refers to (links, transclusions, label references), in order of first appearance
		std::vector<Connection> Links;
		//! The ids of the notes it transcludes (not following redirects), in order
		std::vector<std::string> Transcludes;
		//! Its label references, in the order written
		std::vector<LabelReference> Labels;
		//! The ids of the notes which refer to it (excluding itself), sorted
		std::vector<std::string> Backlinks;
		//! For a stub: the id its redirect resolves to (empty if nothing)
		std::string Redirect;
		//! Its own sidebar: the sidebars it copies (copy-sidebar:, each in turn, as if written in its header), then its sidebar:
		//! entries, grouped by their [Heading]s (entries before any heading in an unnamed first group), all resolved from it; a
		//! target listed twice shows once
		std::vector<SidebarGroup> Sidebar;
	};

	class Network
	{
	  public:
		// it holds pointers into its own Nodes, which a move keeps valid and a copy wouldn't
		Network() = default;
		Network(Network &&) = default;
		Network &operator=(Network &&) = default;
		Network(const Network &) = delete;
		Network &operator=(const Network &) = delete;

		std::vector<Node> Nodes;
		//! The global sidebar, resolved from nowhere (so the shallowest match)
		std::vector<SidebarGroup> Sidebar;
		//! Slugs claimed by more than one note, with the ids claiming them
		std::map<std::string, std::vector<std::string>> SharedNames;

		const Node *Find(const std::string &id) const;
		//! Follows stubs from an id to the note they finally point at: the id itself for an ordinary note; nullopt if a redirect matches
		//! nothing or the chain loops
		std::optional<std::string> Follow(const std::string &id) const;
		//! What a slug resolves to from a note at fromPath (relative): the nearest claimant, or empty if none
		std::string Resolve(const std::string &slug, const std::filesystem::path &fromPath) const;
		//! The number of connections (each distinct target once per note) which match no note
		size_t Unresolved() const;
		//! True if some note refers to slug without finding it: a wanted note
		bool IsWanted(const std::string &slug) const;
		//! The note a [[#label]] written at fromPath (relative) refers to: the nearest of the notes with that label; empty if none has it
		std::string LabelOwner(const std::string &label, const std::filesystem::path &fromPath) const;

	  private:
		friend Network Resolve(const std::vector<Notes::Note> &, const std::vector<Project::SidebarGroup> &, Report::Log &);
		std::map<std::string, size_t> ById;
		std::map<std::string, std::vector<const Node *>> Claims;
		std::map<std::string, std::vector<const Node *>> LabelOwners;
	};

	//! Connects the notes; problems (shared names, broken redirects and sidebar entries, missing or ambiguous labels) go to report.
	//! The notes must outlive the Network
	Network Resolve(const std::vector<Notes::Note> &notes, const std::vector<Project::SidebarGroup> &sidebar, Report::Log &report);

	//! Chooses between notes claiming the same name: the one whose folder shares the most leading folders with fromPath's, then the
	//! shallower one, then the earlier id (so the choice is always deterministic)
	const Node *Nearest(const std::filesystem::path &fromPath, const std::vector<const Node *> &candidates);
} // namespace Network
