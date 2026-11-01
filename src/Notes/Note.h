#pragma once
#include "Header.h"
#include "Reference.h"
#include <filesystem>
#include <string>
#include <vector>

//! A note, in the shape every format is read into (DESIGN.md §16): its identity, header, preamble, body, and what its body refers to.
//! A Note knows nothing about other notes; how they connect is the Network's business
namespace Notes
{
	struct Note
	{
		//! Relative to the project root
		std::filesystem::path Path;
		//! Absolute
		std::filesystem::path Source;
		//! The format it was read from: its extension, without the dot ("tex")
		std::string Format;
		//! From the path (see IdOf): how the viewer addresses the note
		std::string Id;
		//! A hash of the id: a name for the note made only of hex digits, safe anywhere in TeX (macro and file names) and in URLs
		std::string Key;

		Header Fields;

		//! LaTeX placed after the project preamble when compiling this note, and any note transcluding it
		std::string LocalPreamble;
		//! The body as written
		std::string Body;
		//! The body with CorTeX's syntax rewritten into LaTeX (the same number of lines): what gets compiled. Empty for a stub
		std::string Processed;
		//! The [[...]] in the body, in order; none for a stub
		std::vector<Reference> References;
		//! The \label{...}s written in the body; none for a stub
		std::vector<Label> Labels;

		//! The 1-based lines of the file at which LocalPreamble (0 if none) and Body start, for mapping TeX's messages back to the file
		size_t LocalPreambleLine = 0;
		size_t BodyLine = 1;

		bool IsStub() const { return !Fields.Redirect.empty(); }
		//! The note's names: its title, then its aliases
		std::vector<std::string> Names() const;
	};

	//! A note's id, from its path relative to the root (DESIGN.md §2.3): without the extension, lowercased, and every character other
	//! than letters, digits, '/', '-' and '_' replaced by '-'
	std::string IdOf(const std::filesystem::path &relativePath);
} // namespace Notes
