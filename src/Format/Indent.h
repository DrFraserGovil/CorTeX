#pragma once
#include "Settings/Config.h"
#include <string>
#include <string_view>

//! `cortex format`: re-indents a note by its structure (DESIGN.md §13.6), using the same rules for headings, lists and the
//! `cortex off` switch as the compiler (Notes::Syntax), so the two never disagree about what a line is
namespace Formatter
{
	/*! The note's text, re-indented
		@details The header and the separators are left exactly as written; the local preamble and the body are re-indented line by line:
		existing indentation and trailing whitespace are discarded, and each line gets one level per open heading (up to
		Formatting::Headings, both sectioning commands and, with extended syntax, # headings), open environment, open brace and open
		\[ display maths. A line starting with closers (}, \end{...}, \]) sits at the level they close to. Star list items are indented one
		level per star beyond the first, and the lines continuing an item one level deeper than it; a line continues an item only if the
		author indented it further than the list, as the compiler requires. Verbatim environments and `% cortex off` regions are copied
		as written. Blank lines stay blank, and line endings (\n or \r\n) are kept. Formatting its own output changes nothing
		@param extended The project's extended syntax setting: without it, # lines and star lines are ordinary text
	*/
	std::string Indent(std::string_view text, const Settings::Formatting &options, bool extended);
} // namespace Formatter
