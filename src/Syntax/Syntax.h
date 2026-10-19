#pragma once
#include <string>
#include <string_view>
#include <vector>

//! CorTeX's extended syntax: the text-level additions to LaTeX which are rewritten into plain LaTeX before compiling
//! @details Always on:
//!  - links: [[target]] and [[target|text]] become \CortexLink{<origin>}{<slug>}{<text>} (defined in cortex.cls), where origin is the
//!    key of the note the link was written in: a link resolves from where it was written, even when its note is transcluded elsewhere.
//!    [[note#section]] links to the note, ignoring the section (sections aren't supported yet).
//!  - transclusion: ![[target]] becomes \CortexTransclude{<origin>}{<slug>}, which inputs the target note's body (resolved by the engine)
//! Only with Options::Extended (the ExtendedSyntax setting), and only in text (never in maths):
//!  - inline formatting: *italic*, **bold**, ***bold italic***, _monospace_, __small caps__
//!    (an opening marker must not follow a letter, digit or backslash, and must be followed by a non-space other than '}';
//!    a closing one must follow a non-space; formats don't span lines. So \section*, 2*3 and \x*\x are never formatting)
//!  - headings: a line starting with "# " is \section{...}, "## " \subsection, "### " \subsubsection, "#### " \paragraph, "##### " \subparagraph
//!    (deeper levels warn and use \subparagraph); "#* ", "##* "... give the starred (unnumbered) forms. The title is processed like any text.
//!  - star lists: a line starting with "* " is a list item, "** " a nested one, and so on; "*ENUM " / "*ITEM " force the list type.
//!    A list ends at a blank line or any unindented non-list line; indented lines continue the current item; lines starting \item are left alone.
//! Rules every syntax shares:
//!  - every rewrite stays on its own line (list \begin/\end share a line with an item), so TeX's line numbers still map back to the source
//!  - nothing inside a % comment is touched (a % escaped as \% isn't a comment)
//!  - a line consisting only of a "cortex off" / "cortex on" comment (any number of %, any case and spacing) switches the syntax off/on for the rest of the note
//!  - a backslash and the character after it are always left alone, so \[[ is not a link (and \[[A,B]\] stays display maths), and \* \_ are never formatting
//!  - maths is tracked only through its delimiters ($...$, $$...$$, \(...\), \[...\]); environments are never tracked. Problems produce warnings, never errors
namespace Syntax
{
	struct Options
	{
		//! Enables inline formatting and star lists
		bool Extended = false;
		//! The environment opened by star lists which don't force a type
		std::string ListType = "itemize";
		//! The key of the note being processed, written into every link and transclusion so they resolve from their origin
		std::string Origin;
	};

	struct Link
	{
		//! The target as written, e.g. "Euler's Identity"
		std::string Target;
		//! The target in the form used for matching (see Slug)
		std::string Slug;
		//! 1-based line within the processed text
		size_t Line;
	};

	struct Warning
	{
		//! 1-based line within the processed text
		size_t Line;
		std::string Message;
	};

	struct Result
	{
		//! The text with all extended syntax rewritten into LaTeX; always has the same number of lines as the input
		std::string Text;
		std::vector<Link> Links;
		//! The ![[transclusions]] found, in order (Line and Slug as for links)
		std::vector<Link> Transclusions;
		std::vector<Warning> Warnings;
	};

	//! Rewrites the extended syntax in a note body
	Result Process(std::string_view body, const Options &options = {});

	//! The canonical form of a link target, title or alias, so that they can be matched loosely:
	//! lowercase, with every run of characters other than letters, digits, '-' and '_' collapsed to a single '-', and no leading or trailing '-'
	//! @details "Euler's Identity", "euler’s identity" and " Euler s  identity" all become "euler-s-identity"
	std::string Slug(std::string_view text);
} // namespace Syntax
