// Notes::Syntax: CorTeX's syntax rewritten into LaTeX (DESIGN.md §4.2, §7)
#include "Notes/Syntax.h"
#include <catch2/catch_test_macros.hpp>

using namespace Notes;
using Kind = Reference::Kind;

namespace
{
	Syntax::Result Run(std::string_view body, bool extended = false) { return Syntax::Process(body, {extended, "itemize", "K"}); }
	std::string Text(std::string_view body, bool extended = false) { return Run(body, extended).Text; }
	bool Warned(const Syntax::Result &result, const std::string &part)
	{
		for (auto &w : result.Warnings)
			if (w.Message.find(part) != std::string::npos) return true;
		return false;
	}
} // namespace

TEST_CASE("Slugs match names loosely", "[syntax]")
{
	CHECK(Syntax::Slug("Euler's Identity") == "euler-s-identity");
	CHECK(Syntax::Slug("euler’s identity") == "euler-s-identity");
	CHECK(Syntax::Slug(" Euler s  identity") == "euler-s-identity");
	CHECK(Syntax::Slug("--a_b--") == "a_b");
	CHECK(Syntax::Slug("!!!") == "");
}

TEST_CASE("Links", "[syntax]")
{
	auto result = Run("See [[Euler's Identity]] and [[Gamma|the third]].");
	CHECK(result.Text == "See \\CortexLink{K}{euler-s-identity}{Euler's Identity} and \\CortexLink{K}{gamma}{the third}.");
	REQUIRE(result.References.size() == 2);
	CHECK(result.References[0].Type == Kind::Link);
	CHECK(result.References[0].Target == "Euler's Identity");
	CHECK(result.References[1].Line == 1);
	CHECK(result.Warnings.empty());
}

TEST_CASE("Markers set kind and strength, in any order", "[syntax]")
{
	auto result = Run("[[!a]] [[~b]] [[+c]] [[~!d]] [[!+e]]");
	REQUIRE(result.References.size() == 5);
	CHECK(result.References[0].Type == Kind::Transclusion);
	CHECK(result.References[0].Marked == Strength::Default);
	CHECK(result.References[1].Marked == Strength::Loose);
	CHECK(result.References[2].Marked == Strength::Strong);
	CHECK(result.References[3].Type == Kind::Transclusion);
	CHECK(result.References[3].Marked == Strength::Loose);
	CHECK(result.References[4].Marked == Strength::Strong);
	CHECK(result.Text == "\\CortexTransclude{K}{a} \\CortexLink{K}{b}{b} \\CortexLink{K}{c}{c} \\CortexTransclude{K}{d} \\CortexTransclude{K}{e}");
}

TEST_CASE("Bad links are left as written, with a warning", "[syntax]")
{
	for (auto bad : {"[[!!x]]", "[[~+x]]", "[[]]", "[[|text]]", "[[!]]", "[[!x#eq]]", "[[x#]]", "[[#]]"})
	{
		INFO(bad);
		auto result = Run(bad);
		CHECK(result.Text == bad);
		CHECK(result.References.empty());
		CHECK(result.Warnings.size() == 1);
	}
	auto unclosed = Run("[[x and more");
	CHECK(unclosed.Text == "[[x and more");
	CHECK(Warned(unclosed, "no closing"));
}

TEST_CASE("![[x]] is not a transclusion", "[syntax]")
{
	auto result = Run("![[Units]]");
	CHECK(result.Text == "!\\CortexLink{K}{units}{Units}");
	REQUIRE(result.References.size() == 1);
	CHECK(result.References[0].Type == Kind::Link);
	CHECK(Warned(result, "[[!...]]"));
}

TEST_CASE("Label references", "[syntax]")
{
	auto result = Run("[[#eq:euler]] [[Euler#eq:euler]] [[~#fig:Main|the figure]]");
	CHECK(result.Text == "\\CortexRef{K}{}{eq:euler}{} \\CortexRef{K}{euler}{eq:euler}{} \\CortexRef{K}{}{fig:Main}{the figure}");
	REQUIRE(result.References.size() == 3);
	CHECK(result.References[0].Type == Kind::Label);
	CHECK(result.References[0].Target.empty());
	CHECK(result.References[0].Label == "eq:euler");
	CHECK(result.References[1].Slug == "euler");
	CHECK(result.References[2].Label == "fig:Main"); // exact: labels are not slugs
	CHECK(result.References[2].Marked == Strength::Loose);
	CHECK(result.Warnings.empty());
}

TEST_CASE("Labels are recorded, except in comments", "[syntax]")
{
	auto result = Run("\\begin{equation}\\label{eq:a} x \\end{equation}\n% \\label{eq:commented}\n\\section{S}\\label{sec:s} 50\\% \\label{eq:b}\n% cortex off\n\\label{eq:off} % \\label{no}\n");
	REQUIRE(result.Labels.size() == 4);
	CHECK(result.Labels[0].Name == "eq:a");
	CHECK(result.Labels[1].Name == "sec:s");
	CHECK(result.Labels[1].Line == 3);
	CHECK(result.Labels[2].Name == "eq:b");
	CHECK(result.Labels[3].Name == "eq:off");
}

TEST_CASE("Links are found everywhere but comments, escapes and cortex off", "[syntax]")
{
	CHECK(Run("$x = [[a]]$").References.size() == 1);
	CHECK(Run("% [[a]]").References.empty());
	CHECK(Text("\\[[a,b]\\]") == "\\[[a,b]\\]");
	CHECK(Run("[ [a] ]").References.empty());
	auto off = Run("% cortex off\n[[a]]\n%% Cortex ON\n[[b]]");
	REQUIRE(off.References.size() == 1);
	CHECK(off.References[0].Slug == "b");
}

TEST_CASE("The line count never changes", "[syntax]")
{
	std::string body = "# Head\n* one\n** two\n\nText *it* [[x]]\n* last";
	auto text = Text(body, true);
	CHECK(std::count(text.begin(), text.end(), '\n') == std::count(body.begin(), body.end(), '\n'));
}

TEST_CASE("Extended syntax: inline formatting", "[syntax]")
{
	CHECK(Text("*a* **b** ***c*** _d_ __e__", true) == "\\textit{a} \\textbf{b} \\textbf{\\textit{c}} \\texttt{d} \\textsc{e}");
	CHECK(Text("*a*") == "*a*"); // only when switched on
	CHECK(Text("\\section*{x} 2*3 \\x*\\x", true) == "\\section*{x} 2*3 \\x*\\x");
	CHECK(Text("$a_1 * b_2$", true) == "$a_1 * b_2$"); // never in maths
	CHECK(Text("[[x|*it*]]", true) == "\\CortexLink{K}{x}{\\textit{it}}");
	auto unclosed = Run("*open", true);
	CHECK(unclosed.Text == "*open");
	CHECK(Warned(unclosed, "never closed"));
}

TEST_CASE("Extended syntax: headings", "[syntax]")
{
	CHECK(Text("# One\n## Two\n#* Star", true) == "\\section{One}\n\\subsection{Two}\n\\section*{Star}");
	CHECK(Text("#hashtag", true) == "#hashtag");
	auto deep = Run("###### Six", true);
	CHECK(deep.Text == "\\subparagraph{Six}");
	CHECK(Warned(deep, "levels deep"));
}

TEST_CASE("Extended syntax: star lists", "[syntax]")
{
	CHECK(Text("* a\n** b\n* c\n\nafter", true) == "\\begin{itemize}\\item{} a\n\\begin{itemize}\\item{} b\n\\end{itemize}\\item{} c\n\\end{itemize}\nafter");
	CHECK(Text("*ENUM a\n* b", true) == "\\begin{enumerate}\\item{} a\n\\item{} b\\end{enumerate}");
	CHECK(Text("* a\n  continued\n\\item manual", true) == "\\begin{itemize}\\item{} a\n  continued\n\\item manual\\end{itemize}");
}

TEST_CASE("Maths left open is reported", "[syntax]")
{
	CHECK(Warned(Run("$x", true), "never closed"));
	CHECK(Run("\\begin{align*} a \\end{align*} *b*", true).Text == "\\begin{align*} a \\end{align*} \\textit{b}");
}
