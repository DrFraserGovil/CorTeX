// Network::Resolve on the fixture's notes: resolution, redirects, strength, labels and sidebars (DESIGN.md §4.3–4.4, §5, §9.2)
#include "Network/Network.h"
#include "Notes/Reader.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace
{
	struct Fixture
	{
		Report::Log Report;
		std::vector<Notes::Note> Notes;
		Network::Network Net;

		Fixture(const std::vector<Notes::Note> &extra = {})
		{
			std::filesystem::path root = CORTEX_FIXTURE;
			Report::Log reading;
			for (auto &entry : std::filesystem::recursive_directory_iterator(root))
			{
				auto relative = entry.path().lexically_relative(root).generic_string();
				if (relative.starts_with(".") || entry.path().extension() != ".tex") continue;
				if (auto note = Notes::Read(root, entry.path(), {true, "itemize"}, reading)) Notes.push_back(*note);
			}
			Notes.insert(Notes.end(), extra.begin(), extra.end());
			std::sort(Notes.begin(), Notes.end(), [](auto &a, auto &b) { return a.Id < b.Id; });
			Net = Network::Resolve(Notes, Project::Registry::Read(root).Sidebar, Report);
		}

		const Network::Node &operator[](const std::string &id) const
		{
			auto node = Net.Find(id);
			INFO(id);
			REQUIRE(node);
			return *node;
		}
		const Network::Connection *Link(const std::string &from, const std::string &key) const
		{
			for (auto &link : (*this)[from].Links)
				if (link.Key == key) return &link;
			return nullptr;
		}
		bool Reported(Report::Kind kind, const std::string &part) const
		{
			for (auto &e : Report.Of(kind))
				if (e.Message.find(part) != std::string::npos) return true;
			return false;
		}
	};

	bool Has(const std::vector<std::string> &list, const std::string &value) { return std::find(list.begin(), list.end(), value) != list.end(); }
} // namespace

TEST_CASE("Links resolve to the nearest claimant", "[network]")
{
	Fixture f;
	CHECK(f.Link("alpha", "gamma")->Target == "gamma");
	CHECK(f.Link("gamma", "shared")->Target == "beta"); // equally near: the earlier id
	CHECK(f.Net.SharedNames.at("shared") == std::vector<std::string>{"beta", "gamma"});
	CHECK(f.Reported(Report::Kind::SharedName, "'shared' is the title or alias of several notes"));

	// a nearer folder wins over an earlier id
	Notes::Note local;
	local.Path = "places/shared.tex";
	local.Id = "places/shared";
	local.Fields.Title = "Shared";
	Fixture g({local});
	CHECK(g.Net.Resolve("shared", "places/x.tex") == "places/shared");
	CHECK(g.Net.Resolve("shared", "x.tex") == "beta");
}

TEST_CASE("Backlinks and wanted notes", "[network]")
{
	Fixture f;
	CHECK(Has(f["alpha"].Backlinks, "home"));
	CHECK(Has(f["units"].Backlinks, "places/france")); // a transclusion is a backlink too
	CHECK(f.Link("home", "nowhere")->Target.empty());
	CHECK(f.Net.IsWanted("nowhere"));
	CHECK_FALSE(f.Net.IsWanted("alpha"));
	CHECK(f.Net.Unresolved() >= 1);
	CHECK_FALSE(f.Link("home", "not-a-link")); // cortex off
}

TEST_CASE("Redirects", "[network]")
{
	Fixture f;
	CHECK(f["stubs/old-name"].Redirect == "alpha");
	CHECK(f.Net.Follow("stubs/old-name") == "alpha");
	CHECK(f.Net.Follow("alpha") == "alpha");
	CHECK_FALSE(f.Net.Follow("stubs/dangling"));
	CHECK_FALSE(f.Net.Follow("stubs/loop-one"));
	CHECK(f.Link("gamma", "old-name")->Target == "stubs/old-name");
	CHECK(f.Reported(Report::Kind::Redirect, "redirects to 'Ghost', which matches no note"));
	CHECK(f.Reported(Report::Kind::Redirect, "loop of redirects"));
}

TEST_CASE("Strength: loose only if every reference is", "[network]")
{
	Fixture f;
	CHECK(f.Link("places/france", "earth")->Loose);	   // unmarked into a graph: loose note
	CHECK(f.Link("places/france", "germany")->Loose);  // [[~Germany]]
	CHECK_FALSE(f.Link("places/france", "units")->Loose);
	CHECK_FALSE(f.Link("places/germany", "earth")->Loose); // [[+Earth]]
	CHECK_FALSE(f.Link("places/spain", "units")->Loose);   // [[!~Units]], but also ![[Units]] (an unmarked link)
	CHECK(f["places/france"].Transcludes == std::vector<std::string>{"units"});
}

TEST_CASE("Label references", "[network]")
{
	Fixture f;
	auto &calculus = f["maths/calculus"];
	REQUIRE(calculus.Labels.size() == 3);
	CHECK(calculus.Labels[0].Key == "#eq:euler");
	CHECK(calculus.Labels[0].Owner == "maths/euler");
	CHECK(calculus.Labels[1].Key == "euler");
	CHECK(calculus.Labels[1].Owner == "maths/euler");
	CHECK(calculus.Labels[2].Owner.empty());
	CHECK(f.Link("maths/calculus", "#eq:euler")->Target == "maths/euler"); // on the graph, a link to the owner
	CHECK(f.Link("maths/calculus", "euler")->Target == "maths/euler");	   // a missing label still opens the note
	CHECK(f.Reported(Report::Kind::Label, "has no label 'eq:missing'"));
	CHECK(Has(f["maths/euler"].Backlinks, "maths/calculus"));
}

TEST_CASE("A label in several notes goes to the nearest, with a warning", "[network]")
{
	Notes::Note copy;
	copy.Path = "far/away/other.tex";
	copy.Id = "far/away/other";
	copy.Fields.Title = "Other";
	copy.Labels = {{"eq:euler", 1}};
	Fixture f({copy});
	CHECK(f["maths/calculus"].Labels[0].Owner == "maths/euler");
	CHECK(f.Reported(Report::Kind::Label, "is in several notes"));
}

TEST_CASE("Sidebars", "[network]")
{
	Fixture f;
	REQUIRE(f.Net.Sidebar.size() == 2);
	CHECK(f.Net.Sidebar[0].Items[1].Target == "maths/euler");
	CHECK(f.Net.Sidebar[0].Items[1].Label == "Euler (label)");
	CHECK(f.Net.Sidebar[1].Items[0].Target.empty());
	CHECK(f.Reported(Report::Kind::Sidebar, "No Such Note"));
	REQUIRE(f["home"].Sidebar.size() == 1);
	CHECK(f["home"].Sidebar[0].Heading.empty());
	CHECK(f["home"].Sidebar[0].Items[0].Target == "beta");
}

namespace
{
	Notes::Note Sidebarred(const std::string &id, const std::string &title, std::vector<std::string> copy, std::vector<std::string> own, const std::string &redirect = "")
	{
		Notes::Note note;
		note.Path = id + ".tex";
		note.Id = id;
		note.Fields.Title = title;
		note.Fields.CopySidebar = std::move(copy);
		note.Fields.Sidebar = std::move(own);
		note.Fields.Redirect = redirect;
		return note;
	}

	// each group as "Heading: target, target", the unnamed group's heading as "-"
	std::vector<std::string> Shape(const Network::Node &node)
	{
		std::vector<std::string> out;
		for (auto &group : node.Sidebar)
		{
			std::string line = (group.Heading.empty() ? "-" : group.Heading) + ":";
			for (size_t i = 0; i < group.Items.size(); ++i) line += (i ? ", " : " ") + (group.Items[i].Target.empty() ? "?" + group.Items[i].Text : group.Items[i].Target);
			out.push_back(line);
		}
		return out;
	}
} // namespace

TEST_CASE("A note's sidebar groups under [Heading]s, after the sidebars it copies", "[network]")
{
	Fixture f({Sidebarred("fam/parent", "Parent", {}, {"Alpha", "[Maths]", "Euler|E"}),
			   Sidebarred("fam/child", "Child", {"Parent"}, {"Beta", "[Maths]", "Alpha", "[Other]", "Gamma"}),
			   Sidebarred("fam/grand", "Grand", {"Child"}, {"[Other]", "Nowhere"})});
	// the copied entries come first; each note's own list starts outside any heading; a target already shown is left out
	CHECK(Shape(f["fam/child"]) == std::vector<std::string>{"-: alpha, beta", "Maths: maths/euler", "Other: gamma"});
	CHECK(f["fam/child"].Sidebar[1].Items[0].Label == "E");
	// transitive: a copy of a note copies what it copies
	CHECK(Shape(f["fam/grand"]) == std::vector<std::string>{"-: alpha, beta", "Maths: maths/euler", "Other: gamma, ?Nowhere"});
	CHECK(f.Reported(Report::Kind::Sidebar, "'Nowhere' matches no note"));
}

TEST_CASE("copy-sidebar follows redirects, and reports loops and names matching nothing", "[network]")
{
	Fixture f({Sidebarred("fam/parent", "Parent", {}, {"Alpha"}), Sidebarred("fam/old", "Old Parent", {}, {}, "Parent"),
			   Sidebarred("fam/via", "Via", {"Old Parent", "No Such Parent"}, {"Beta"}),
			   Sidebarred("loop/a", "Loop A", {"Loop B"}, {"Alpha"}), Sidebarred("loop/b", "Loop B", {"Loop A"}, {"Beta"})});
	CHECK(Shape(f["fam/via"]) == std::vector<std::string>{"-: alpha, beta"});
	CHECK(f.Reported(Report::Kind::Sidebar, "copy-sidebar 'No Such Parent' matches no note"));
	CHECK(Shape(f["loop/a"]) == std::vector<std::string>{"-: beta, alpha"});
	CHECK(Shape(f["loop/b"]) == std::vector<std::string>{"-: alpha, beta"});
	CHECK(f.Reported(Report::Kind::Sidebar, "copy-sidebar loops (loop/a -> loop/b -> loop/a)"));
}
