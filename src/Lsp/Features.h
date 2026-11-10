#pragma once
#include "Build/Scan.h"
#include "Protocol.h"
#include "Report/Report.h"
#include <filesystem>
#include <map>
#include <string>

//! What the language server offers (DESIGN.md §13.7), each a pure function of the project as last read and the document the editor has
//! open, answering with the LSP's own JSON. Positions are the LSP's: 0-based lines, characters counted in the negotiated encoding
namespace Lsp
{
	//! A document open in the editor: its file (absolute) and its text as the editor has it, saved or not
	struct Document
	{
		std::filesystem::path File;
		std::string Text;
	};

	struct Position
	{
		size_t Line = 0;
		size_t Character = 0;
	};

	//! Inside [[: note names (titles and aliases); after a #, labels; on redirect:, sidebar: and copy-sidebar: header lines, note names
	Json Complete(const Build::Scan &scan, const Document &document, Position position, Encoding encoding);
	//! The file (and line, for a label) a [[...]] at position points to; null if none
	Json Definition(const Build::Scan &scan, const Document &document, Position position, Encoding encoding);
	//! What a [[...]] at position points to, as Markdown; null if the position isn't on one
	Json Hover(const Build::Scan &scan, const Document &document, Position position, Encoding encoding);
	//! Every [[...]] in the project referring to the note the [[...]] at position points to or, elsewhere, to the document's own note
	Json References(const Build::Scan &scan, const Document &document, Position position, Encoding encoding);
	//! The document re-indented (Formatter::Indent) with the project's settings, as edits; an empty list if it's already formatted
	Json Formatting(const Build::Scan &scan, const Document &document, Encoding encoding);

	//! The semantic token kinds the server uses (LSP's "legend"): one type, link, for every [[...]], and modifiers for what it is and
	//! whether it reaches anything. An editor colours them as it likes (Neovim: @lsp.typemod.link.resolved.cortex, ...)
	Json TokenLegend();
	//! The target (and label) of every [[...]] in the document's body, as semantic tokens: resolved if it reaches a note (or label),
	//! unresolved if not, or if it is malformed. Not in the header, the local preamble, or `% cortex off` regions
	Json SemanticTokens(const Build::Scan &scan, const Document &document, Encoding encoding);

	//! The report's entries which belong to a file, as LSP diagnostics by file URI (failures are errors, the rest warnings; entries
	//! only counted in the terminal, like skipped files, are left out)
	std::map<std::string, Json> Diagnostics(const Report::Log &report, const std::filesystem::path &root);
} // namespace Lsp
