// Export (§13.3) and the cortex:// link handler (§10). Link commands run with no PATH, so no desktop notification is ever shown
#include "support/Harness.h"
#include "Settings/Config.h"
#include <catch2/catch_test_macros.hpp>
#include <fstream>

namespace
{
	const std::string NoPath = "PATH=/nonexistent";
	std::string Url(const Harness::Project &p, const std::string &action, const std::string &rest)
	{
		return "-url 'cortex://" + action + "?root=" + p.Root.string() + "&" + rest + "'";
	}
} // namespace

TEST_CASE("Export makes a self-contained copy to share", "[export]")
{
	REQUIRE_TEX();
	Harness::Project project("export");
	auto target = project.Root.parent_path() / (project.Root.filename().string() + "-shared");
	std::filesystem::remove_all(target);
	auto output = project.Cortex("export -to " + target.string());
	INFO(output.Text);
	REQUIRE(output.Exit == 0);
	Harness::Index index(target);
	CHECK(index.Config["live"] == false);
	CHECK(index.Raw.find(project.Root.string()) == std::string::npos);
	CHECK(index.Config["editors"].empty());
	CHECK_FALSE(std::filesystem::exists(target / "version.js"));
	CHECK(std::filesystem::exists(target / "notes/alpha.js"));

	SECTION("an earlier export is replaced")
	{
		CHECK(project.Cortex("export -to " + target.string()).Exit == 0);
	}
	std::filesystem::remove_all(target);
}

TEST_CASE("Export refuses unsafe destinations", "[export]")
{
	REQUIRE_TEX();
	Harness::Project project("export-refused");
	auto occupied = project.Root.parent_path() / (project.Root.filename().string() + "-occupied");
	std::filesystem::create_directories(occupied);
	std::ofstream(occupied / "keep.txt") << "mine";
	CHECK(project.Cortex("export -to " + occupied.string()).Exit != 0);
	CHECK(std::filesystem::exists(occupied / "keep.txt"));
	std::filesystem::remove_all(occupied);

	CHECK(project.Cortex("export -to " + project.Root.string()).Exit != 0);
	CHECK(project.Cortex("export -to " + (project.Root / ".cortex/x").string()).Exit != 0);
}

TEST_CASE("The link handler refuses anything outside its rules", "[link]")
{
	Harness::Project project("link-refused");
	auto refused = [&](const std::string &arguments) {
		auto output = project.Cortex("link " + arguments, NoPath);
		INFO(arguments << "\n" << output.Text);
		CHECK(output.Exit != 0);
	};
	refused("-url 'cortex://edit?root=relative/path&note=alpha.tex'");
	refused(Url(project, "edit", "note=../outside.tex"));
	refused(Url(project, "edit", "note=fragment.tex")); // a .tex file, but not a note
	refused(Url(project, "edit", "note=alpha.tex&editor=missing"));
	refused(Url(project, "explode", "note=alpha.tex"));
	refused(Url(project, "todo", "note=alpha.tex&set=maybe"));
	refused(Url(project, "create", "dir=&slug=not-wanted"));
	refused(Url(project, "create", "dir=../&slug=nowhere"));
	CHECK(project.Read("alpha.tex") == "title: Alpha\n-----\nAlpha points at [[Gamma]].\n");
	CHECK_FALSE(project.Exists("not-wanted.ctx"));
}

TEST_CASE("The link handler flags notes and creates wanted ones", "[link]")
{
	REQUIRE_TEX();
	Harness::Project project("link-actions");
	project.Build(); // so the project knows which notes are wanted

	project.Cortex("link " + Url(project, "todo", "note=alpha.tex&set=on"), NoPath);
	CHECK(project.Read("alpha.tex") == "title: Alpha\ntodo:\n-----\nAlpha points at [[Gamma]].\n");
	project.Cortex("link " + Url(project, "todo", "note=alpha.tex&set=off"), NoPath);
	CHECK(project.Read("alpha.tex") == "title: Alpha\n-----\nAlpha points at [[Gamma]].\n");

	project.Cortex("link " + Url(project, "create", "dir=&slug=nowhere&editor=ed"), NoPath);
	REQUIRE(project.Exists("nowhere.ctx")); // new notes are .ctx files
	CHECK(project.Read("nowhere.ctx") == "title: nowhere\nalias: nowhere\n-----\n\n");
}

TEST_CASE("A graph link saves only valid graph settings", "[link]")
{
	Harness::Project project("link-graph");
	auto settings = [&] { return project.Exists(".cortex/settings.config") ? project.Read(".cortex/settings.config") : std::string(); };
	auto before = settings();
	auto refused = [&](const std::string &rest) {
		auto output = project.Cortex("link " + Url(project, "graph", rest), NoPath);
		INFO(rest << "\n" << output.Text);
		CHECK(output.Exit != 0);
		CHECK(settings() == before);
	};
	refused("layout=spiral");
	refused("colours=rainbow");
	refused("folder-pull=2");
	refused("folder-pull=0.5x");
	refused("folder-repulsion=-1");
	refused("folder-repulsion=nan");
	refused("zoom=3");
	refused("layout=rings&output=/tmp"); // one bad key refuses the whole link
	refused("");

	// a valid link saves (the rebuild after it fails here, with no PATH, but the settings are already written)
	project.Cortex("link " + Url(project, "graph", "layout=rings&colours=shades&folder-pull=0.25&folder-repulsion=1.5"), NoPath);
	Report::Log report;
	auto config = Settings::Config::Load(project.Root, report);
	CHECK(config.Viewer.GraphLayout == "rings");
	CHECK(config.Viewer.GraphColours == "shades");
	CHECK(config.Viewer.FolderPull == 0.25);
	CHECK(config.Viewer.FolderRepulsion == 1.5);
	CHECK(config.Viewer.Zoom == 1.4); // everything else as it was
}

TEST_CASE("Format re-indents a note in place, with the project's settings", "[format]")
{
	Harness::Project project("format");
	project.Append(".cortex/settings.config", "format-indent 2\n");
	project.Write("indent.ctx", "title: Indent\n-----\n# Heading\n      text\n");
	CHECK(project.Cortex("format -file indent.ctx").Exit == 0);
	CHECK(project.Read("indent.ctx") == "title: Indent\n-----\n# Heading\n  text\n");
	CHECK(project.Cortex("format -file indent.ctx").Text.find("already formatted") != std::string::npos);
	CHECK(project.Cortex("format -file missing.ctx").Exit != 0);
}
