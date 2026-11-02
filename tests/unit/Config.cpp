// Settings::Config: settings are read, checked, and invalid ones replaced by their defaults with a report
#include "Settings/Config.h"
#include <JSL.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
	std::filesystem::path Project(const std::string &name, const std::string &settings)
	{
		auto root = std::filesystem::path(CORTEX_TEST_SCRATCH) / "config" / name;
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root / ".cortex");
		JSL::IO::writeString(root / ".cortex/settings.config", settings);
		return std::filesystem::canonical(root);
	}
} // namespace

TEST_CASE("Defaults", "[config]")
{
	Report::Log report;
	auto root = Project("defaults", "");
	auto config = Settings::Config::Load(root, report);
	CHECK(report.Entries().empty());
	CHECK(config.Output == root / "view");
	CHECK(config.Allowed == std::vector<std::string>{"*.ctx", "*.tex"});
	CHECK(config.Format.Indent == "\t");
	CHECK(config.Format.Headings == 3);
	CHECK(config.Compilation.ListType == "itemize");
	CHECK(config.Viewer.GraphLayout == "centre");
	CHECK(config.Viewer.GraphColours == "top");
	CHECK(config.Editors.empty());
}

TEST_CASE("Values are read", "[config]")
{
	Report::Log report;
	auto root = Project("values", "output site\nextended true\nwidth 12\ngraph-layout rings\ngraph-colours shades\neditors [a: x {file}, b: y]\nhome home.tex\n");
	auto config = Settings::Config::Load(root, report);
	CHECK(config.Output == root / "site");
	CHECK(config.Compilation.Extended);
	CHECK(config.Compilation.WidthCM == 12);
	CHECK(config.Viewer.GraphLayout == "rings");
	CHECK(config.Viewer.GraphColours == "shades");
	CHECK(config.Viewer.Home == "home.tex");
	REQUIRE(config.Editors.size() == 2);
	CHECK(config.Editors[1].Name == "b");
	CHECK(report.Count(Report::Kind::Setting) == 1); // b doesn't say where {file} goes
}

TEST_CASE("Invalid values become defaults, with a report", "[config]")
{
	Report::Log report;
	auto root = Project("invalid", "zoom -1\nfolder-pull 3\ngraph-layout spiral\ngraph-colours rainbow\nfolder-repulsion -2\nlist-type bullets\nwidth 0\nlogo missing.png\neditors [nocolon, a: x {file}, a: y {file}]\n");
	auto config = Settings::Config::Load(root, report);
	CHECK(config.Viewer.Zoom == 1.4);
	CHECK(config.Viewer.FolderPull == 0.5);
	CHECK(config.Viewer.GraphLayout == "centre");
	CHECK(config.Viewer.GraphColours == "top");
	CHECK(config.Viewer.FolderRepulsion == 0);
	CHECK(config.Compilation.ListType == "itemize");
	CHECK(config.Compilation.WidthCM == 25);
	CHECK(config.Viewer.Logo.empty());
	CHECK(config.Editors.size() == 1);
	CHECK(report.Count(Report::Kind::Setting) == 10);
}

TEST_CASE("The output folder can't be the project itself", "[config]")
{
	for (auto bad : {".", "..", ".cortex", ".cortex/out"})
	{
		Report::Log report;
		auto root = Project("output", std::string("output ") + bad + "\n");
		CHECK(Settings::Config::Load(root, report).Output == root / "view");
		CHECK(report.Count(Report::Kind::Setting) == 1);
	}
	Report::Log report;
	auto root = Project("absolute", "output /tmp/cortex-elsewhere\n");
	CHECK(Settings::Config::Load(root, report).Output == "/tmp/cortex-elsewhere");
}

TEST_CASE("The compile key changes with any compile setting", "[config]")
{
	Settings::Compile a, b;
	CHECK(a.Key() == b.Key());
	b.ListType = "enumerate";
	CHECK(a.Key() != b.Key());
}

TEST_CASE("Format settings", "[config]")
{
	Report::Log report;
	auto config = Settings::Config::Load(Project("format", "format-indent 4\nformat-headings 1\nformat-braces false\n"), report);
	CHECK(report.Entries().empty());
	CHECK(config.Format.Indent == "    ");
	CHECK(config.Format.Headings == 1);
	CHECK_FALSE(config.Format.Braces);

	Report::Log bad;
	auto fallback = Settings::Config::Load(Project("format-bad", "format-indent wide\nformat-headings -2\n"), bad);
	CHECK(bad.Count(Report::Kind::Setting) == 2);
	CHECK(fallback.Format.Indent == "\t");
	CHECK(fallback.Format.Headings == 3);
}
