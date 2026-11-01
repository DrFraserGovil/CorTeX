// A full build of the fixture: what DESIGN.md says the index and the build messages must hold
#include "support/Harness.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using Harness::Built;
using Harness::BuiltOutput;

namespace
{
	Harness::Index BuiltIndex() { return Harness::Index(Built().Root / "view"); }
	bool Contains(const nlohmann::json &list, const std::string &value) { return std::find(list.begin(), list.end(), value) != list.end(); }
	std::string BuiltMessages() { return Harness::Messages(BuiltOutput(), Built()); }
} // namespace

TEST_CASE("Which files are notes, and their status (§2, §6.3)", "[build]")
{
	REQUIRE_TEX();
	auto index = BuiltIndex();
	CHECK(index["alpha"]["status"] == "ok");
	CHECK(index["units"]["status"] == "ok");
	CHECK(index["broken"]["status"] == "failed");
	CHECK(index["broken"]["diagnostic"].get<std::string>().find("broken.tex:3") != std::string::npos);
	CHECK(index["stubs/old-name"]["status"] == "redirect");
	CHECK_FALSE(index.Notes.contains("fragment")); // no header, so not a note
}

TEST_CASE("Links resolve by name, nearest first, through redirects (§4.3, §5.3)", "[build]")
{
	REQUIRE_TEX();
	auto index = BuiltIndex();
	CHECK(index["alpha"]["links"]["gamma"] == "gamma");
	CHECK(index["gamma"]["links"]["shared"] == "beta"); // shared by beta and gamma: equally near, so the earlier id
	CHECK(index["gamma"]["links"]["old-name"] == "stubs/old-name");
	CHECK(index["stubs/old-name"]["redirect"] == "alpha");
	CHECK(index["stubs/dangling"]["redirect"].is_null());
	CHECK(Contains(index["alpha"]["backlinks"], "home"));
	CHECK(index["home"]["links"]["nowhere"].is_null()); // a wanted note
}

TEST_CASE("Markers set kind and strength (§4.2, §9.2)", "[build]")
{
	REQUIRE_TEX();
	auto index = BuiltIndex();
	auto &france = index["places/france"];
	CHECK(france["links"]["earth"] == "places/earth");
	CHECK(Contains(france["loose"], "earth"));	 // unmarked, into a graph: loose note
	CHECK(Contains(france["loose"], "germany")); // [[~Germany]]
	CHECK_FALSE(Contains(france["loose"], "units"));
	CHECK(Contains(france["transcludes"], "units"));

	CHECK_FALSE(index["places/germany"].contains("loose")); // [[+Earth]] is strong even into a loose note

	auto &spain = index["places/spain"];
	CHECK(Contains(spain["loose"], "earth"));
	CHECK_FALSE(Contains(spain["loose"], "units")); // [[!~Units]] is loose, but ![[Units]] is an ordinary (strong) link
	CHECK(Contains(spain["transcludes"], "units"));

	auto messages = BuiltMessages();
	CHECK(messages.find("is not a transclusion") != std::string::npos);
	CHECK(messages.find("contradictory markers") != std::string::npos);
}

TEST_CASE("cortex off leaves text alone (§7.4)", "[build]")
{
	REQUIRE_TEX();
	CHECK_FALSE(BuiltIndex()["home"]["links"].contains("not-a-link"));
}

TEST_CASE("Header fields reach the index (§3.1)", "[build]")
{
	REQUIRE_TEX();
	auto index = BuiltIndex();
	CHECK(index["hidden"]["graphHidden"] == true);
	CHECK(index["maths/euler"]["todo"] == "needs a proof");
	CHECK(index["maths/euler"]["names"].size() == 2); // title and alias
	CHECK(index["home"]["sidebar"][0]["target"] == "beta");
}

TEST_CASE("The global sidebar and viewer configuration (§8.6, §11)", "[build]")
{
	REQUIRE_TEX();
	auto index = BuiltIndex();
	REQUIRE(index.Sidebar.size() == 2);
	CHECK(index.Sidebar[0]["heading"] == "Start");
	CHECK(index.Sidebar[0]["items"][1]["label"] == "Euler (label)");
	CHECK(index.Sidebar[0]["items"][1]["target"] == "maths/euler");
	CHECK(index.Sidebar[1]["items"][0]["target"].is_null());
	CHECK(index.Config["name"] == "Fixture");
	CHECK(index.Config["home"] == "home");
	CHECK(index.Config["live"] == true);
}

TEST_CASE("Images are embedded in the note (§6.5)", "[build]")
{
	REQUIRE_TEX();
	CHECK(Built().Read("view/notes/art/gallery.js").find("data:image/png;base64,") != std::string::npos);
}

TEST_CASE("Problems with the link network are reported (§4.3, §5.2, §5.3, §8.6, §13.4)", "[build]")
{
	REQUIRE_TEX();
	auto messages = BuiltMessages();
	CHECK(messages.find("'shared' is the title or alias of several notes") != std::string::npos);
	CHECK(messages.find("would loop back on itself") != std::string::npos);
	CHECK(messages.find("redirects to 'Ghost', which matches no note") != std::string::npos);
	CHECK(messages.find("loop of redirects") != std::string::npos);
	CHECK(messages.find("No Such Note") != std::string::npos);
	CHECK(messages.find("1 note is flagged") != std::string::npos);
}

TEST_CASE("Two files with the same id stop the build (§2.3)", "[build]")
{
	REQUIRE_TEX();
	Harness::Project project("duplicate-id");
	project.Write("Dup Name.tex", "title: One\n-----\nx\n");
	project.Write("dup-name.tex", "title: Two\n-----\nx\n");
	auto output = project.Cortex("build");
	CHECK(output.Exit != 0);
	CHECK(output.Text.find("dup-name") != std::string::npos);
}
