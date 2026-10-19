#include "Index.h"
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

Index::Index(const std::vector<Note> &notes, const BuildState &state)
{
	/// ONE ENTRY PER NOTE, WITH ITS BUILD STATUS AND NAMES
	for (auto &note : notes)
	{
		Entry entry{note.Id, note.Key, note.Title, note.RelativePath.generic_string(), Status::Failed, "not yet built", {}, {}, {}};

		// the status comes from the last time the note was compiled, whether that was this run or an earlier one
		auto record = state.Records.find(note.Id);
		if (record != state.Records.end())
		{
			auto &r = record->second;
			entry.State = r.Succeeded ? Status::Ok : (r.HasOutput ? Status::Stale : Status::Failed);
			entry.Diagnostic = r.Diagnostic;
		}

		std::set<std::string> names;
		names.insert(Syntax::Slug(note.Title));
		for (auto &alias : note.Aliases)
		{
			names.insert(Syntax::Slug(alias));
		}
		names.erase("");
		entry.Names.assign(names.begin(), names.end());
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

std::string Index::Serialise() const
{
	using Utility::JsonString;
	std::string js = "CortexIndex([\n";
	for (size_t i = 0; i < List.size(); ++i)
	{
		auto &e = List[i];
		js += (i > 0 ? ",\n" : "");
		js += "{\"id\":" + JsonString(e.Id) + ",\"key\":" + JsonString(e.Key) + ",\"title\":" + JsonString(e.Title) + ",\"path\":" + JsonString(e.Path);
		js += ",\"status\":" + JsonString(StatusName(e.State)) + ",\"diagnostic\":" + JsonString(e.Diagnostic);
		js += ",\"names\":" + JsonArray(e.Names);

		// links as {slug: target id, or null if unresolved}, so the viewer can follow a link with one lookup
		js += ",\"links\":{";
		for (size_t l = 0; l < e.Links.size(); ++l)
		{
			auto &link = e.Links[l];
			js += (l > 0 ? "," : "") + JsonString(link.Slug) + ":" + (link.Target.empty() ? "null" : JsonString(link.Target));
		}
		js += "},\"backlinks\":" + JsonArray(e.Backlinks) + "}";
	}
	return js + "\n]);\n";
}
