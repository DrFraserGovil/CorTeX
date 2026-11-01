// Notes::Header and Notes::Split: the note file's layout and header fields (DESIGN.md §3)
#include "Notes/Header.h"
#include "Notes/Note.h"
#include <JSL.h>
#include <catch2/catch_test_macros.hpp>

using namespace Notes;

TEST_CASE("Separators split the file", "[header]")
{
	CHECK(IsSeparator("-----"));
	CHECK(IsSeparator("  ======== "));
	CHECK_FALSE(IsSeparator("---"));
	CHECK_FALSE(IsSeparator("--=--"));

	auto none = Split("just text\n");
	CHECK_FALSE(none.HasHeader);
	CHECK(none.Body == "just text\n");

	auto one = Split("title: A\n-----\nbody\n");
	CHECK(one.HasHeader);
	CHECK(one.Header == std::vector<std::string>{"title: A"});
	CHECK(one.Body == "body\n");
	CHECK(one.BodyLine == 3);

	auto three = Split("title: A\n-----\n\\usepackage{x}\n=====\nbody\n-----\nmore\n");
	CHECK(three.LocalPreamble == "\\usepackage{x}\n");
	CHECK(three.LocalPreambleLine == 3);
	CHECK(three.Body == "body\n-----\nmore\n");
	CHECK(three.BodyLine == 5);
}

TEST_CASE("Header fields", "[header]")
{
	std::vector<HeaderProblem> problems;
	auto header = Header::Parse({"Title: Euler", "aliases: a, , b ", "title: Ignored", "graph: hidden, loose", "show-title: no", "todo: needs a proof",
								 "sidebar: X, Y|Why", "id: legacy", "no colon here"},
								problems);
	CHECK(problems.empty());
	CHECK(header.Title == "Euler");
	CHECK(header.Aliases == std::vector<std::string>{"a", "b"});
	CHECK(header.GraphHidden);
	CHECK(header.GraphLoose);
	CHECK(header.ShowTitle == false);
	CHECK(header.Todo == "needs a proof");
	CHECK(header.Sidebar == std::vector<std::string>{"X", "Y|Why"});
}

TEST_CASE("List fields gather every line", "[header]")
{
	std::vector<HeaderProblem> problems;
	auto header = Header::Parse({"alias: a", "title: T", "aliases: b, c", "graph: hidden", "graph: loose", "sidebar: X", "sidebar: Y"}, problems);
	CHECK(header.Aliases == std::vector<std::string>{"a", "b", "c"});
	CHECK(header.GraphHidden);
	CHECK(header.GraphLoose);
	CHECK(header.Sidebar == std::vector<std::string>{"X", "Y"});
}

TEST_CASE("Todo values", "[header]")
{
	std::vector<HeaderProblem> problems;
	CHECK(Header::Parse({"todo:"}, problems).Todo == "");
	CHECK(Header::Parse({"todo: YES"}, problems).Todo == "");
	CHECK_FALSE(Header::Parse({"todo: off"}, problems).Todo);
	CHECK_FALSE(Header::Parse({"title: x"}, problems).Todo);
}

TEST_CASE("Unknown values are problems", "[header]")
{
	std::vector<HeaderProblem> problems;
	auto header = Header::Parse({"title: x", "graph: hidden, sideways", "show-title: maybe"}, problems);
	CHECK(header.GraphHidden);
	REQUIRE(problems.size() == 2);
	CHECK(problems[0].Line == 2);
	CHECK(problems[1].Message.find("show-title") != std::string::npos);
}

TEST_CASE("SetTodo rewrites only the todo lines", "[header]")
{
	auto file = std::filesystem::path(CORTEX_TEST_SCRATCH) / "header" / "todo.tex";
	std::filesystem::create_directories(file.parent_path());
	auto write = [&](const std::string &text) { JSL::IO::writeString(file, text); };
	auto read = [&]() { return JSL::IO::getFile(file); };

	write("title: A\n-----\nbody todo: x\n");
	CHECK(SetTodo(file, true));
	CHECK(read() == "title: A\ntodo:\n-----\nbody todo: x\n");
	CHECK_FALSE(SetTodo(file, true));
	CHECK(SetTodo(file, false));
	CHECK(read() == "title: A\n-----\nbody todo: x\n");

	write("title: A\nTODO: keep this reason\n-----\nbody");
	CHECK_FALSE(SetTodo(file, true));
	write("title: A\ntodo: no\n-----\nbody");
	CHECK(SetTodo(file, true));
	CHECK(read() == "title: A\ntodo:\n-----\nbody"); // no final newline added

	write("no header");
	CHECK_THROWS(SetTodo(file, true));
}

TEST_CASE("Ids come from the path", "[header]")
{
	CHECK(IdOf("Physics/Euler's Identity.tex") == "physics/euler-s-identity");
	CHECK(IdOf("Dup Name.tex") == IdOf("dup-name.tex"));
}
