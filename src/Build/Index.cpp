#include "Index.h"
#include <JSL.h>
#include "../Syntax/Syntax.h"
#include "../Utility/Files.h"
#include <algorithm>
#include <filesystem>
#include <set>

static std::string StatusName(Index::Status status)
{
	switch (status)
	{
		case Index::Status::Ok:
			return "ok";
		case Index::Status::Stale:
			return "stale";
		case Index::Status::Redirect:
			return "redirect";
		default:
			return "failed";
	}
}

// The number of leading folders two relative paths share: a/b/x.tex and a/c/y.tex share 1
static size_t SharedFolders(const std::string &a, const std::string &b)
{
	auto da = std::filesystem::path(a).parent_path();
	auto db = std::filesystem::path(b).parent_path();
	size_t shared = 0;
	for (auto ia = da.begin(), ib = db.begin(); ia != da.end() && ib != db.end() && *ia == *ib; ++ia, ++ib)
	{
		++shared;
	}
	return shared;
}

static size_t Depth(const std::string &path)
{
	return std::distance(std::filesystem::path(path).begin(), std::filesystem::path(path).end());
}

std::string Index::Nearest(const std::string &fromPath, const std::vector<const Entry *> &candidates)
{
	auto better = [&](const Entry *a, const Entry *b) {
		auto sa = SharedFolders(fromPath, a->Path), sb = SharedFolders(fromPath, b->Path);
		if (sa != sb) return sa > sb;
		auto da = Depth(a->Path), db = Depth(b->Path);
		if (da != db) return da < db;
		return a->Id < b->Id;
	};
	return (*std::min_element(candidates.begin(), candidates.end(), better))->Id;
}

Index::SidebarItem Index::SidebarItem::Parse(std::string_view entry)
{
	SidebarItem item;
	auto bar = entry.find('|');
	item.Text = JSL::String::trim(entry.substr(0, bar));
	item.Label = (bar == std::string_view::npos) ? "" : JSL::String::trim(entry.substr(bar + 1));
	item.Slug = Syntax::Slug(item.Text);
	return item;
}

std::string Index::SidebarItem::Shown(const std::map<std::string, std::string> &titles) const
{
	if (!Label.empty()) return Label;
	if (auto title = titles.find(Target); !Target.empty() && title != titles.end()) return title->second;
	return Text;
}

Index::Index(const std::vector<Note> &notes, const BuildState &state, std::vector<SidebarGroup> sidebar) : GlobalSidebar(std::move(sidebar))
{
	/// ONE ENTRY PER NOTE, WITH ITS BUILD STATUS AND NAMES
	for (auto &note : notes)
	{
		Entry entry{note.Id, note.Key, note.Title, note.RelativePath.generic_string(), Status::Failed, "not yet built", {}, {}, {}, {}, "", "", false, {}};

		// the status comes from the last time the note was compiled, whether that was this run or an earlier one
		auto record = state.Records.find(note.Id);
		if (record != state.Records.end())
		{
			auto &r = record->second;
			entry.State = r.Succeeded ? Status::Ok : (r.HasOutput ? Status::Stale : Status::Failed);
			entry.Diagnostic = r.Diagnostic;
		}
		entry.GraphHidden = note.GraphHidden;
		if (!note.Redirect.empty())
		{
			entry.State = Status::Redirect;
			entry.Diagnostic = "";
			entry.RedirectText = note.Redirect;
		}

		std::set<std::string> names;
		names.insert(Syntax::Slug(note.Title));
		for (auto &alias : note.Aliases)
		{
			names.insert(Syntax::Slug(alias));
		}
		names.erase("");
		entry.Names.assign(names.begin(), names.end());
		for (auto &item : note.Sidebar)
		{
			entry.Sidebar.push_back(SidebarItem::Parse(item));
		}
		List.push_back(entry);
	}

	/// WHICH NOTES CLAIM EACH NAME
	std::map<std::string, std::vector<const Entry *>> claims;
	for (auto &entry : List)
	{
		for (auto &name : entry.Names)
		{
			claims[name].push_back(&entry);
		}
	}
	for (auto &[name, claimants] : claims)
	{
		if (claimants.size() > 1)
		{
			for (auto *c : claimants) ClashList[name].push_back(c->Id);
		}
	}

	/// RESOLVE EACH NOTE'S LINKS, AND RECORD THE BACKLINKS
	std::map<std::string, std::set<std::string>> backlinks;
	for (size_t n = 0; n < notes.size(); ++n)
	{
		auto &entry = List[n];
		std::set<std::string> seen;
		// a transclusion is a kind of link: it resolves the same way, counts as a backlink, and if unresolved it's a wanted note
		auto references = notes[n].Links;
		references.insert(references.end(), notes[n].Transclusions.begin(), notes[n].Transclusions.end());
		for (auto &link : references)
		{
			if (!seen.insert(link.Slug).second) continue;

			ResolvedLink resolved{link.Slug, ""};
			auto claim = claims.find(link.Slug);
			if (claim != claims.end())
			{
				resolved.Target = Nearest(entry.Path, claim->second);
				if (resolved.Target != entry.Id) backlinks[resolved.Target].insert(entry.Id);
			}
			entry.Links.push_back(resolved);
		}
	}
	for (auto &entry : List)
	{
		auto &from = backlinks[entry.Id];
		entry.Backlinks.assign(from.begin(), from.end());
	}

	/// WHICH OF THOSE ARE TRANSCLUSIONS (THE GRAPH DRAWS THEM DIFFERENTLY)
	for (size_t n = 0; n < notes.size(); ++n)
	{
		std::set<std::string> seen;
		for (auto &t : notes[n].Transclusions)
		{
			auto claim = claims.find(t.Slug);
			if (claim == claims.end()) continue;
			auto target = Nearest(List[n].Path, claim->second);
			if (seen.insert(target).second) List[n].Transcludes.push_back(target);
		}
	}

	/// RESOLVE REDIRECTS, FROM THE STUB (LIKE A LINK WRITTEN IN IT)
	for (auto &entry : List)
	{
		if (entry.RedirectText.empty()) continue;
		auto claim = claims.find(Syntax::Slug(entry.RedirectText));
		if (claim != claims.end()) entry.RedirectTarget = Nearest(entry.Path, claim->second);
	}

	/// RESOLVE THE SIDEBARS: A NOTE'S OWN FROM THE NOTE (LIKE ITS LINKS), THE GLOBAL ONE FROM NOWHERE (THE SHALLOWEST MATCH)
	auto resolve = [&](SidebarItem &item, const std::string &fromPath) {
		auto claim = claims.find(item.Slug);
		item.Target = (claim == claims.end()) ? "" : Nearest(fromPath, claim->second);
	};
	for (auto &entry : List)
	{
		for (auto &item : entry.Sidebar) resolve(item, entry.Path);
	}
	for (auto &group : GlobalSidebar)
	{
		for (auto &item : group.Items) resolve(item, "");
	}
}

std::string Index::FollowRedirects(const std::string &id) const
{
	std::set<std::string> visited;
	std::string current = id;
	while (true)
	{
		auto entry = std::find_if(List.begin(), List.end(), [&](auto &e) { return e.Id == current; });
		if (entry == List.end()) return "";
		if (entry->RedirectText.empty()) return current; // an ordinary note: the end of the chain
		if (entry->RedirectTarget.empty() || !visited.insert(current).second) return ""; // unresolved, or a loop
		current = entry->RedirectTarget;
	}
}

const std::vector<Index::SidebarGroup> &Index::Sidebar() const
{
	return GlobalSidebar;
}

const std::vector<Index::Entry> &Index::Entries() const
{
	return List;
}

const std::map<std::string, std::vector<std::string>> &Index::Clashes() const
{
	return ClashList;
}

size_t Index::UnresolvedCount() const
{
	size_t count = 0;
	for (auto &entry : List)
	{
		count += std::count_if(entry.Links.begin(), entry.Links.end(), [](auto &l) { return l.Target.empty(); });
	}
	return count;
}

static std::string JsonArray(const std::vector<std::string> &items)
{
	std::string out = "[";
	for (size_t i = 0; i < items.size(); ++i)
	{
		out += (i > 0 ? "," : "") + Utility::JsonString(items[i]);
	}
	return out + "]";
}

// Sidebar items as [{"label":...,"slug":...,"target":id or null}]
static std::string SidebarJson(const std::vector<Index::SidebarItem> &items, const std::map<std::string, std::string> &titles)
{
	using Utility::JsonString;
	std::string out = "[";
	for (size_t i = 0; i < items.size(); ++i)
	{
		auto &item = items[i];
		out += (i > 0 ? "," : "") + std::string("{\"label\":") + JsonString(item.Shown(titles)) + ",\"slug\":" + JsonString(item.Slug);
		out += ",\"target\":" + (item.Target.empty() ? std::string("null") : JsonString(item.Target)) + "}";
	}
	return out + "]";
}

std::string Index::Serialise() const
{
	using Utility::JsonString;
	std::map<std::string, std::string> titles;
	for (auto &e : List)
	{
		titles[e.Id] = e.Title;
	}
	std::string js = "CortexIndex([\n";
	for (size_t i = 0; i < List.size(); ++i)
	{
		auto &e = List[i];
		js += (i > 0 ? ",\n" : "");
		js += "{\"id\":" + JsonString(e.Id) + ",\"key\":" + JsonString(e.Key) + ",\"title\":" + JsonString(e.Title) + ",\"path\":" + JsonString(e.Path);
		js += ",\"status\":" + JsonString(StatusName(e.State)) + ",\"diagnostic\":" + JsonString(e.Diagnostic);
		if (e.GraphHidden)
		{
			js += ",\"graphHidden\":true";
		}
		if (!e.RedirectText.empty())
		{
			js += ",\"redirect\":" + (e.RedirectTarget.empty() ? std::string("null") : JsonString(e.RedirectTarget)) + ",\"redirectText\":" + JsonString(e.RedirectText);
		}
		js += ",\"names\":" + JsonArray(e.Names);

		// links as {slug: target id, or null if unresolved}, so the viewer can follow a link with one lookup
		js += ",\"links\":{";
		for (size_t l = 0; l < e.Links.size(); ++l)
		{
			auto &link = e.Links[l];
			js += (l > 0 ? "," : "") + JsonString(link.Slug) + ":" + (link.Target.empty() ? "null" : JsonString(link.Target));
		}
		js += "},\"backlinks\":" + JsonArray(e.Backlinks) + ",\"transcludes\":" + JsonArray(e.Transcludes);
		js += ",\"sidebar\":" + SidebarJson(e.Sidebar, titles) + "}";
	}
	js += "\n]);\n";

	// the global sidebar: groups of resolved items
	js += "CortexSidebar([";
	for (size_t g = 0; g < GlobalSidebar.size(); ++g)
	{
		js += (g > 0 ? "," : "") + std::string("{\"heading\":") + JsonString(GlobalSidebar[g].Heading) + ",\"items\":" + SidebarJson(GlobalSidebar[g].Items, titles) + "}";
	}
	return js + "]);\n";
}
