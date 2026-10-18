#pragma once
#include <string>
#include <string_view>
#include <vector>

//! CorTeX's extended syntax: the text-level additions to LaTeX which are rewritten into plain LaTeX before compiling
//! @details Currently only links: [[target]] and [[target|text]], which become \CortexLink{<slug>}{<text>} (defined in cortex.cls).
//! Rules, which every future syntax shares:
//!  - every rewrite stays on its own line, so line numbers in TeX's errors still map back to the source
//!  - nothing inside a % comment is touched (a % escaped as \% isn't a comment)
//!  - a line consisting only of a "cortex off" / "cortex on" comment (any number of %, any case and spacing) switches the syntax off/on for the rest of the note
//!  - a backslash and the character after it are always left alone, so \[[ is not a link (and \[[A,B]\] stays display maths), while \\[[x]] is a line break then a link
namespace Syntax
{
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
		std::vector<Warning> Warnings;
	};

	//! Rewrites the extended syntax in a note body
	Result Process(std::string_view body);

	//! The canonical form of a link target, title or alias, so that they can be matched loosely:
	//! lowercase, with every run of characters other than letters, digits, '-' and '_' collapsed to a single '-', and no leading or trailing '-'
	//! @details "Euler's Identity", "euler’s identity" and " Euler s  identity" all become "euler-s-identity"
	std::string Slug(std::string_view text);
} // namespace Syntax
