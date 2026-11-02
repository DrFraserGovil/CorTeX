#include "Network.h"
#include "Notes/Syntax.h"
#include <JSL.h>
#include <algorithm>
#include <set>

namespace
{
	// The number of leading folders two relative paths share: a/b/x.tex and a/c/y.tex share 1
	size_t SharedFolders(const std::filesystem::path &a, const std::filesystem::path &b)
	{
		auto da = a.parent_path(), db = b.parent_path();
		size_t shared = 0;
		for (auto ia = da.begin(), ib = db.begin(); ia != da.end() && ib != db.end() && *ia == *ib; ++ia, ++ib) ++shared;
		return shared;
	}

	size_t Depth(const std::filesystem::path &path)
	{
		return std::distance(path.begin(), path.end());
	}

	std::string Joined(const std::vector<std::string> &ids)
	{
		return JSL::String::stitch(ids, 0, ids.size(), ", ");
	}
} // namespace

const Network::Node *Network::Nearest(const std::filesystem::path &fromPath, const std::vector<const Node *> &candidates)
{
	auto better = [&](const Node *a, const Node *b) {
		auto sa = SharedFolders(fromPath, a->Source->Path), sb = SharedFolders(fromPath, b->Source->Path);
		if (sa != sb) return sa > sb;
		auto da = Depth(a->Source->Path), db = Depth(b->Source->Path);
		if (da != db) return da < db;
		return a->Id < b->Id;
	};
	return *std::min_element(candidates.begin(), candidates.end(), better);
}

const Network::Node *Network::Network::Find(const std::string &id) const
{
	auto found = ById.find(id);
	return found == ById.end() ? nullptr : &Nodes[found->second];
}

std::optional<std::string> Network::Network::Follow(const std::string &id) const
{
	std::set<std::string> visited;
	std::string current = id;
	while (true)
	{
		auto node = Find(current);
		if (!node) return std::nullopt;
		if (!node->Source->IsStub()) return current; // an ordinary note: the end of the chain
		if (node->Redirect.empty() || !visited.insert(current).second) return std::nullopt; // unresolved, or a loop
		current = node->Redirect;
	}
}

std::string Network::Network::Resolve(const std::string &slug, const std::filesystem::path &fromPath) const
{
	auto claim = Claims.find(slug);
	return claim == Claims.end() ? "" : Nearest(fromPath, claim->second)->Id;
}

size_t Network::Network::Unresolved() const
{
	size_t count = 0;
	for (auto &node : Nodes)
	{
		// a [[#label]] whose label is nowhere is a missing label, not a wanted note
		count += std::count_if(node.Links.begin(), node.Links.end(), [](auto &l) { return l.Target.empty() && !l.Key.starts_with("#"); });
	}
	return count;
}

bool Network::Network::IsWanted(const std::string &slug) const
{
	for (auto &node : Nodes)
	{
		for (auto &link : node.Links)
		{
			if (link.Key == slug && link.Target.empty()) return true;
		}
	}
	return false;
}

Network::Network Network::Resolve(const std::vector<Notes::Note> &notes, const std::vector<Project::SidebarGroup> &sidebar, Report::Log &report)
{
	Network network;
	using Notes::Reference;

	/// ONE NODE PER NOTE, WITH ITS NAMES
	network.Nodes.reserve(notes.size());
	for (auto &note : notes)
	{
		Node node{&note, note.Id, {}, {}, {}, {}, {}, "", {}};
		std::set<std::string> names;
		for (auto &name : note.Names()) names.insert(Notes::Syntax::Slug(name));
		names.erase("");
		node.Names.assign(names.begin(), names.end());
		network.ById[node.Id] = network.Nodes.size();
		network.Nodes.push_back(std::move(node));
	}

	/// WHICH NOTES CLAIM EACH NAME, AND OWN EACH LABEL
	auto &owners = network.LabelOwners;
	for (auto &node : network.Nodes)
	{
		for (auto &name : node.Names) network.Claims[name].push_back(&node);
		std::set<std::string> labels;
		for (auto &label : node.Source->Labels) labels.insert(label.Name);
		for (auto &label : labels) owners[label].push_back(&node);
	}
	for (auto &[name, claimants] : network.Claims)
	{
		if (claimants.size() < 2) continue;
		auto &ids = network.SharedNames[name];
		for (auto *c : claimants) ids.push_back(c->Id);
		report.Add(Report::Kind::SharedName, "'" + name + "' is the title or alias of several notes (" + Joined(ids) + "); links to it go to whichever is nearest the linking note");
	}

	/// RESOLVE REDIRECTS, FROM THE STUB (LIKE A LINK WRITTEN IN IT)
	for (auto &node : network.Nodes)
	{
		if (node.Source->IsStub()) node.Redirect = network.Resolve(Notes::Syntax::Slug(node.Source->Fields.Redirect), node.Source->Path);
	}
	for (auto &node : network.Nodes)
	{
		if (node.Source->IsStub() && !network.Follow(node.Id))
		{
			report.Add(Report::Kind::Redirect, "redirects to '" + node.Source->Fields.Redirect + "', which " + (node.Redirect.empty() ? "matches no note" : "leads into a loop of redirects"),
					   node.Source->Path);
		}
	}
	auto ownerOf = [&](const std::string &id) { return network.Follow(id).value_or(""); };
	auto owns = [&](const std::string &id, const std::string &label) {
		auto node = network.Find(id);
		return node && std::any_of(node->Source->Labels.begin(), node->Source->Labels.end(), [&](auto &l) { return l.Name == label; });
	};

	/// RESOLVE EACH NOTE'S REFERENCES
	std::map<std::string, std::set<std::string>> backlinks;
	for (auto &node : network.Nodes)
	{
		auto &note = *node.Source;
		auto file = note.Path;
		// how this note's references to each key were marked: (any marked strong, any unmarked); settled once all are seen
		std::map<std::string, std::pair<bool, bool>> marks;
		std::set<std::string> transcluded;
		for (auto &reference : note.References)
		{
			bool byLabel = reference.Type == Reference::Kind::Label && reference.Slug.empty();
			std::string key = byLabel ? "#" + reference.Label : reference.Slug;
			std::string target;

			if (byLabel)
			{
				// the label's owner, anywhere: the nearest if several notes own it, which is warned about for this reference only
				auto found = owners.find(reference.Label);
				if (found == owners.end())
				{
					report.Add(Report::Kind::Label, "no note has the label '" + reference.Label + "'; shown as ??", file, note.BodyLine + reference.Line - 1);
				}
				else
				{
					target = Nearest(note.Path, found->second)->Id;
					if (found->second.size() > 1)
					{
						std::vector<std::string> ids;
						for (auto *o : found->second) ids.push_back(o->Id);
						report.Add(Report::Kind::Label, "the label '" + reference.Label + "' is in several notes (" + Joined(ids) + "); using " + target + ", the nearest", file,
								   note.BodyLine + reference.Line - 1);
					}
				}
			}
			else
			{
				target = network.Resolve(reference.Slug, note.Path);
			}

			if (reference.Type == Reference::Kind::Label)
			{
				std::string owner = byLabel ? target : ownerOf(target);
				if (!byLabel && !target.empty() && !owns(owner, reference.Label))
				{
					report.Add(Report::Kind::Label, "'" + reference.Target + "' has no label '" + reference.Label + "'; shown as ??", file, note.BodyLine + reference.Line - 1);
					owner = "";
				}
				node.Labels.push_back({key, reference.Label, owner});
			}
			if (reference.Type == Reference::Kind::Transclusion && !target.empty() && transcluded.insert(target).second) node.Transcludes.push_back(target);

			bool seen = marks.contains(key);
			auto &[strong, unmarked] = marks[key];
			strong = strong || reference.Marked == Notes::Strength::Strong;
			unmarked = unmarked || reference.Marked == Notes::Strength::Default;
			if (seen) continue;
			node.Links.push_back({key, target, false});
			if (!target.empty() && target != node.Id) backlinks[target].insert(node.Id);
		}

		/// HOW STRONG EACH CONNECTION IS: LOOSE ONLY IF EVERY REFERENCE IS (MARKED LOOSE, OR UNMARKED INTO A `graph: loose` NOTE)
		for (auto &link : node.Links)
		{
			auto [strong, unmarked] = marks[link.Key];
			auto destination = link.Target.empty() ? nullptr : network.Find(ownerOf(link.Target));
			link.Loose = !strong && (!unmarked || (destination && destination->Source->Fields.GraphLoose));
		}
	}
	for (auto &node : network.Nodes)
	{
		auto &from = backlinks[node.Id];
		node.Backlinks.assign(from.begin(), from.end());
	}

	/// RESOLVE THE SIDEBARS: A NOTE'S OWN FROM THE NOTE (LIKE ITS LINKS), THE GLOBAL ONE FROM NOWHERE (THE SHALLOWEST MATCH)
	auto item = [&](const Project::SidebarEntry &entry, const std::filesystem::path &from) {
		auto slug = Notes::Syntax::Slug(entry.Text);
		return SidebarItem{entry.Text, entry.Label, slug, network.Resolve(slug, from)};
	};
	for (auto &node : network.Nodes)
	{
		for (auto &written : node.Source->Fields.Sidebar)
		{
			node.Sidebar.push_back(item(Project::SidebarEntry::Parse(written), node.Source->Path));
			if (node.Sidebar.back().Target.empty())
			{
				report.Add(Report::Kind::Sidebar, "sidebar entry '" + node.Sidebar.back().Text + "' matches no note", node.Source->Path);
			}
		}
	}
	for (auto &group : sidebar)
	{
		network.Sidebar.push_back({group.Heading, {}});
		for (auto &entry : group.Entries)
		{
			network.Sidebar.back().Items.push_back(item(entry, ""));
			if (network.Sidebar.back().Items.back().Target.empty())
			{
				report.Add(Report::Kind::Sidebar, "sidebar entry '" + entry.Text + "' matches no note", ".cortex/registry.config");
			}
		}
	}
	return network;
}

std::string Network::Network::LabelOwner(const std::string &label, const std::filesystem::path &fromPath) const
{
	auto found = LabelOwners.find(label);
	return found == LabelOwners.end() ? "" : Nearest(fromPath, found->second)->Id;
}
