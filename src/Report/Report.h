#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

//! What a command found worth telling the author (DESIGN.md §13.4), recorded as data: the build log gets every entry in full, and the
//! terminal a line per failure and a count of everything else by kind
namespace Report
{
	enum class Kind
	{
		Failure,	//!< a note that didn't compile, with its diagnosis
		Setting,	//!< an invalid setting, replaced by its default
		Skipped,	//!< a candidate without a title (not a note); only counted in the terminal
		SharedName, //!< a name several notes share
		Redirect,	//!< a broken or looping redirect
		Sidebar,	//!< a sidebar entry which matches no note
		Syntax,		//!< CorTeX syntax used wrongly (contradictory markers, ![[x]], a transclusion of a section, ...)
		Transclusion, //!< a transclusion which can't be made (a cycle, a missing note)
		Label,		//!< a label reference which is missing or ambiguous, or one of LaTeX's label warnings
		Note		//!< anything else
	};

	//! The kind as the terminal's summary counts it, e.g. "shared name"
	std::string Describe(Kind kind);

	struct Entry;
	//! An entry as one line: "file:line: message", or as much of that as it has
	std::string Format(const Entry &entry);

	struct Entry
	{
		Kind Type;
		std::string Message;
		//! The author's file it belongs to (relative to the root), and the line within it; empty and 0 when it belongs to neither
		std::filesystem::path File = {};
		int Line = 0;
	};

	class Log
	{
	  public:
		void Add(Kind kind, std::string message, std::filesystem::path file = {}, int line = 0);
		const std::vector<Entry> &Entries() const { return All; }
		size_t Count(Kind kind) const;
		//! The entries of one kind
		std::vector<Entry> Of(Kind kind) const;
		//! Moves another log's entries (and account) to the end of this one
		void Absorb(Log &&other);

		//! Records a line of the command's account, for the build log only (what was compiled, and the like)
		void Account(std::string line) { Lines.push_back(std::move(line)); }
		const std::vector<std::string> &AccountLines() const { return Lines; }

	  private:
		std::vector<Entry> All;
		std::vector<std::string> Lines;
	};
} // namespace Report
