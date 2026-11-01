// Behaviour DESIGN.md added in the refactor (what was §15, "Not yet built"): transcluded paths, labels, ids, the build log, prerequisites,
// the output setting, the viewer data's format, and the watch's handoff
#include "support/Harness.h"
#include "Platform/Process.h"
#include <catch2/catch_test_macros.hpp>
#include <regex>
#include <functional>
#include <thread>

using Harness::Built;
using Harness::BuiltOutput;

TEST_CASE("Transcluded text resolves its paths from its own folder (§5.2)", "[build]")
{
	REQUIRE_TEX();
	Harness::Index index(Built().Root / "view");
	CHECK(index["book/chapter"]["status"] == "ok");
	CHECK(Built().Read("view/notes/book/chapter.js").find("data:image/png;base64,") != std::string::npos);
}

TEST_CASE("Label references (§4.4)", "[build]")
{
	REQUIRE_TEX();
	Harness::Index index(Built().Root / "view");
	auto &calculus = index["maths/calculus"];
	CHECK(calculus["status"] == "ok");
	CHECK(calculus["links"]["euler"] == "maths/euler");
	auto messages = Harness::Messages(BuiltOutput(), Built());
	CHECK(messages.find("eq:missing") != std::string::npos);
}

TEST_CASE("The id header is not a feature (§2.3)", "[build]")
{
	REQUIRE_TEX();
	Harness::Index index(Built().Root / "view");
	CHECK(index.Notes.contains("explicit-id"));
	CHECK_FALSE(index.Notes.contains("customid"));
}

TEST_CASE("Build log, and a quiet terminal (§13.4)", "[build]")
{
	REQUIRE_TEX();
	REQUIRE(Built().Exists(".cortex/build.log"));
	auto log = Built().Read(".cortex/build.log");
	CHECK(log.find("fragment.tex") != std::string::npos);
	CHECK(log.find("would loop back") != std::string::npos);
	CHECK(BuiltOutput().Text.find("fragment.tex") == std::string::npos); // skipped candidates are only counted in the terminal
}

TEST_CASE("Missing prerequisites stop the build with a clear message (§13.1)", "[build]")
{
	Harness::Project project("prerequisites");
	auto output = project.Cortex("build", "PATH=/nonexistent");
	CHECK(output.Exit != 0);
	CHECK(output.Text.find("xelatex") != std::string::npos);
	CHECK(output.Text.find("failed to build") == std::string::npos);
}

TEST_CASE("The output folder is a setting (§11)", "[build]")
{
	REQUIRE_TEX();
	Harness::Project project("output");
	project.Append(".cortex/settings.config", "output site\n");
	project.Build();
	CHECK(project.Exists("site/index.html"));
	CHECK_FALSE(project.Exists("view"));
}

TEST_CASE("The viewer data says which format it is", "[build]")
{
	REQUIRE_TEX();
	CHECK(Harness::Index(Built().Root / "view").Config["format"] == 1);
}

namespace
{
	// The text a note's SVG shows as its label references, in order
	std::vector<std::string> ReferenceTexts(const std::string &js)
	{
		static const std::regex reference(R"(cortex-ref[^>]*>\\n<text[^>]*>([^<]*)</text>)");
		std::vector<std::string> out;
		for (std::sregex_iterator match(js.begin(), js.end(), reference), end; match != end; ++match) out.push_back((*match)[1]);
		return out;
	}

	bool WaitFor(const std::function<bool()> &condition, int seconds)
	{
		for (int i = 0; i < seconds * 10; ++i)
		{
			if (condition()) return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		return false;
	}
} // namespace

TEST_CASE("A reference shows the label's current number, after one build (§4.4)", "[build]")
{
	REQUIRE_TEX();
	Harness::Project project("label-numbers");
	project.Build();
	CHECK(ReferenceTexts(project.Read("view/notes/maths/calculus.js")) == std::vector<std::string>{"1", "1", "??"});

	// a new equation before it: Euler's equation becomes (2), and the notes showing its number follow in the same build
	auto euler = project.Read("maths/euler.tex");
	project.Write("maths/euler.tex", euler.replace(euler.find("\\begin{equation}"), 0, "\\begin{equation} x \\end{equation}\n"));
	auto compiled = Harness::Compiled(project.Build(), project);
	CHECK(compiled.contains("maths/calculus.tex"));
	CHECK(ReferenceTexts(project.Read("view/notes/maths/calculus.js")) == std::vector<std::string>{"2", "2", "??"});
}

TEST_CASE("A label reference on the same page is local (§4.4)", "[build]")
{
	REQUIRE_TEX();
	Harness::Project project("label-local");
	project.Append("maths/euler.tex", "As [[#eq:euler]] shows.\n");
	project.Build();
	auto js = project.Read("view/notes/maths/euler.js");
	CHECK(js.find("#/l/maths/euler/") != std::string::npos);
	CHECK(js.find("cortex-label") != std::string::npos);
}

TEST_CASE("A build while a watch runs is handed to the watch (§13.2)", "[build]")
{
	REQUIRE_TEX();
	Harness::Project project("watch");
	auto log = project.Root / "watch.out";
	REQUIRE(Platform::Launch({.Arguments = {CORTEX_BINARY, "watch"}, .WorkingDirectory = project.Root, .OutputFile = log}) == "");
	REQUIRE(WaitFor([&] { return project.Exists("view/index.html"); }, 120));
	auto before = project.Read("view/notes/alpha.js");

	auto handed = project.Cortex("build");
	CHECK(handed.Exit == 0);
	CHECK(handed.Text.find("has been asked to rebuild") != std::string::npos);

	project.Write("alpha.tex", "title: Alpha\n-----\nAlpha changed, still pointing at [[Gamma]].\n");
	CHECK(WaitFor([&] { return project.Read("view/notes/alpha.js") != before; }, 60));

	CHECK(project.Cortex("shutdown").Text.find("Asked the running CorTeX process to stop") != std::string::npos);
	CHECK(WaitFor([&] { return project.Cortex("shutdown").Text.find("No CorTeX process is running") != std::string::npos; }, 20));
}
