#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Build
{
	//! What the last build did with one note
	struct Record
	{
		std::string Id;
		//! Hash of everything the note was compiled from (see Plan::Hash), when it was last compiled
		std::string Hash;
		//! True if the last compilation succeeded
		bool Succeeded = false;
		//! True if an output exists for the note: after a failure, this is the last good version
		bool HasOutput = false;
		//! A one-line explanation of the last failure (empty after a success)
		std::string Diagnostic;
		//! Project files TeX read while compiling the note (\input files, local packages, images), as (path relative to the root, content hash)
		std::vector<std::pair<std::string, std::string>> Dependencies;
		//! The labels the note owns, with what \ref printed for each when it last compiled: what references from elsewhere show
		std::map<std::string, std::string> Labels;
	};

	//! The persistent memory of a project's builds, used to decide which notes need recompiling (DESIGN.md §6.6)
	//! @details Stored as tab-separated text: a header line ("cortex-buildstate", format version, global hash), then one line per note:
	//! id, hash, ok|failed, has-output (0|1), dependency count, a (path, hash) pair per dependency, label count, a (label, text) pair per
	//! label, and finally the diagnostic (last, because it is the only field which may be empty)
	class BuildState
	{
	  public:
		//! Hash of everything which affects every note (preamble, class, compile settings, engine version), as of the last build
		std::string GlobalHash;
		//! One record per note, keyed by note id
		std::map<std::string, Record> Records;

		//! Reads a saved state. A missing, unreadable or out-of-date file gives an empty state (which makes every note dirty), never an error
		static BuildState Load(const std::filesystem::path &file);
		//! Writes the state atomically
		void Save(const std::filesystem::path &file) const;
		//! Drops the records of notes which no longer exist
		void RetainOnly(const std::set<std::string> &ids);

		//! Bumped whenever the file layout changes, so old files are discarded rather than misread
		static constexpr int FormatVersion = 3;
	};
} // namespace Build
