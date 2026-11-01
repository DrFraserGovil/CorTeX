// Notes::Read: fixture files read into Notes
#include "Notes/Reader.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
	const std::filesystem::path Root = CORTEX_FIXTURE;
	std::optional<Notes::Note> Read(const std::string &file, Report::Log &report) { return Notes::Read(Root, Root / file, {true, "itemize"}, report); }
} // namespace

TEST_CASE("A note is read with its header, references and labels", "[reader]")
{
	Report::Log report;
	auto note = Read("maths/euler.tex", report);
	REQUIRE(note);
	CHECK(note->Id == "maths/euler");
	CHECK(note->Format == "tex");
	CHECK(note->Key.size() == 16);
	CHECK(note->Fields.Todo == "needs a proof");
	REQUIRE(note->Labels.size() == 1);
	CHECK(note->Labels[0].Name == "eq:euler");
	CHECK(report.Entries().empty());
}

TEST_CASE("Files that aren't notes are skipped and counted", "[reader]")
{
	Report::Log report;
	CHECK_FALSE(Read("fragment.tex", report));
	CHECK(report.Count(Report::Kind::Skipped) == 1);
	CHECK_FALSE(Notes::Read(Root, Root / "art/pic.png", {}, report)); // no reader for the format
}

TEST_CASE("A stub has no content", "[reader]")
{
	Report::Log report;
	auto note = Read("stubs/old-name.tex", report);
	REQUIRE(note);
	CHECK(note->IsStub());
	CHECK(note->References.empty());
	CHECK(note->Processed.empty());
}

TEST_CASE("Syntax problems are reported at their line in the file", "[reader]")
{
	Report::Log report;
	REQUIRE(Read("places/spain.tex", report));
	auto syntax = report.Of(Report::Kind::Syntax);
	REQUIRE(syntax.size() == 3);
	CHECK(syntax[0].File == "places/spain.tex");
	CHECK(syntax[0].Line > 2);
}
