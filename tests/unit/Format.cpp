// Formatter::Indent: notes re-indented by their structure (DESIGN.md §13.6)
#include "Format/Indent.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
	std::string Format(const std::string &text, bool extended = true, Settings::Formatting options = {}) { return Formatter::Indent(text, options, extended); }
} // namespace

TEST_CASE("The header and separators are left alone; the body is re-indented", "[format]")
{
	CHECK(Format("title: X\n  alias: y\n-----\n   text  \n") == "title: X\n  alias: y\n-----\ntext\n");
	CHECK(Format("no header\n  at all") == "no header\nat all"); // no final newline: none added
	CHECK(Format("") == "");
}

TEST_CASE("Headings indent what follows, both forms alike", "[format]")
{
	auto text = "t: x\n-----\n# A\none\n## B\ntwo\n\\section{C}\nthree\n\\subsection*{D}\nfour\n";
	CHECK(Format(text) == "t: x\n-----\n# A\n\tone\n\t## B\n\t\ttwo\n\\section{C}\n\tthree\n\t\\subsection*{D}\n\t\tfour\n");
	// without extended syntax, # lines are text
	CHECK(Format("-----\n# A\none\n", false) == "-----\n# A\none\n");
	// only as deep as the setting
	Settings::Formatting shallow;
	shallow.Headings = 1;
	CHECK(Format("-----\n# A\n## B\nx\n", true, shallow) == "-----\n# A\n\t## B\n\tx\n");
}

TEST_CASE("Environments, braces and display maths", "[format]")
{
	CHECK(Format("-----\n\\begin{align}\na\n  \\end{align}\nafter\n") == "-----\n\\begin{align}\n\ta\n\\end{align}\nafter\n");
	CHECK(Format("-----\n\\newcommand{\\x}{\nbody\n}\n") == "-----\n\\newcommand{\\x}{\n\tbody\n}\n");
	CHECK(Format("-----\n\\[\nx\n\\]\n") == "-----\n\\[\n\tx\n\\]\n");
	CHECK(Format("-----\n\\{ not a brace\nx\n") == "-----\n\\{ not a brace\nx\n");
	CHECK(Format("-----\n{ % } in a comment\nx\n}\n") == "-----\n{ % } in a comment\n\tx\n}\n");
	Settings::Formatting flat;
	flat.Environments = false;
	CHECK(Format("-----\n\\begin{a}\nx\n\\end{a}\n", true, flat) == "-----\n\\begin{a}\nx\n\\end{a}\n");
}

TEST_CASE("Verbatim and cortex off are copied as written", "[format]")
{
	CHECK(Format("-----\n# H\n\\begin{verbatim}\n   keep  \n\\end{verbatim}\n") == "-----\n# H\n\t\\begin{verbatim}\n   keep  \n\t\\end{verbatim}\n");
	CHECK(Format("-----\n# H\n% cortex off\n   raw\n% cortex on\nx\n") == "-----\n# H\n\t% cortex off\n   raw\n\t% cortex on\n\tx\n");
}

TEST_CASE("Star lists", "[format]")
{
	// items by their stars; a continuation one deeper than its item, but only if the author indented it (else it ends the list)
	CHECK(Format("-----\n* a\n  more\n** b\n* c\nafter\n") == "-----\n* a\n\tmore\n\t** b\n* c\nafter\n");
	CHECK(Format("-----\n# H\n* a\n\tmore\nafter\n") == "-----\n# H\n\t* a\n\t\tmore\n\tafter\n");
	// a verbatim environment inside an item: its \end goes back to the \begin's depth, so the item still holds it
	CHECK(Format("-----\n* a\n  \\begin{lstlisting}\ncode\n\\end{lstlisting}\n* b\n") == "-----\n* a\n\t\\begin{lstlisting}\ncode\n\t\\end{lstlisting}\n* b\n");
}

TEST_CASE("Formatting is idempotent, and keeps line endings", "[format]")
{
	auto text = "t: x\n-----\n\\usepackage{y}\n-----\n# A\n\\begin{itemize}\n\\item x\n\\end{itemize}\n* a\n  b\n## B\n{\nc\n}\n";
	auto once = Format(text);
	CHECK(Format(once) == once);
	CHECK(Format("-----\r\n# A\r\nx\r\n") == "-----\r\n# A\r\n\tx\r\n");
	Settings::Formatting spaces;
	spaces.Indent = "  ";
	CHECK(Format("-----\n# A\nx\n", true, spaces) == "-----\n# A\n  x\n");
}
