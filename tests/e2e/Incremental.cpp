// Which notes a build recompiles (§6.6). Notes that failed last time are always retried, so they are allowed in every set
#include "support/Harness.h"
#include <catch2/catch_test_macros.hpp>
#include <functional>

namespace
{
	// Builds once, applies a change, builds again, and checks the second build compiled exactly `expected` (plus failed notes)
	void ExpectRecompiled(const std::string &name, const std::function<void(const Harness::Project &)> &change, std::set<std::string> expected)
	{
		Harness::Project project(name);
		project.Build();
		auto failed = Harness::Index(project.Root / "view").Failed();
		change(project);
		auto compiled = Harness::Compiled(project.Build(), project);
		expected.insert(failed.begin(), failed.end());
		for (auto &f : failed) compiled.insert(f); // a failed note may or may not be retried before its dependents
		CHECK(compiled == expected);
	}
} // namespace

TEST_CASE("An unchanged project recompiles nothing", "[incremental]")
{
	REQUIRE_TEX();
	ExpectRecompiled("unchanged", [](auto &) {}, {});
}

TEST_CASE("Header-only changes recompile nothing", "[incremental]")
{
	REQUIRE_TEX();
	ExpectRecompiled("header", [](auto &p) { p.Write("maths/euler.tex", p.Read("maths/euler.tex").replace(0, 0, "alias: Another Name\n")); }, {});
}

TEST_CASE("A body change recompiles the note and everything transcluding it", "[incremental]")
{
	REQUIRE_TEX();
	ExpectRecompiled("body", [](auto &p) { p.Append("units.tex", "More.\n"); }, {"units.tex", "places/france.tex", "places/spain.tex"});
}

TEST_CASE("A changed image recompiles the notes that used it", "[incremental]")
{
	REQUIRE_TEX();
	ExpectRecompiled("image", [](auto &p) { p.Append("art/pic.png", std::string(1, '\0')); }, {"art/gallery.tex", "book/chapter.tex"});
}

TEST_CASE("A changed project preamble recompiles every note", "[incremental]")
{
	REQUIRE_TEX();
	Harness::Project probe("probe");
	std::set<std::string> all;
	for (auto &entry : std::filesystem::recursive_directory_iterator(probe.Root))
	{
		auto relative = entry.path().lexically_relative(probe.Root).generic_string();
		if (entry.path().extension() == ".tex" && !relative.starts_with(".") && relative != "fragment.tex" && !relative.starts_with("stubs/")) all.insert(relative);
	}
	ExpectRecompiled("preamble", [](auto &p) { p.Append(".cortex/preamble.tex", "% changed\n"); }, all);
}
