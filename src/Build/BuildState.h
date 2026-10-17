#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

//! What the last build did with one note
struct NoteRecord
{
	std::string Id;
	//! Relative path of the source, at the time it was last built
	std::string Path;
	//! Hash of the note's TeX content (local preamble + body), when it was last compiled
	std::string Hash;
	//! True if the last compilation succeeded
	bool Succeeded = false;
	//! True if an output exists for the note: after a failure, this is the last good version
	bool HasOutput = false;
	//! A one-line explanation of the last failure (empty after a success)
	std::string Diagnostic;
	//! Project files TeX read while compiling the note (\input files, local packages, images), as (path relative to the root, content hash)
	std::vector<std::pair<std::string, std::string>> Dependencies;
};

//! The persistent memory of a project's builds, used to decide which notes need recompiling
//! @details Stored as tab-separated text: a header line ("cortex-buildstate", format version, global hash), then one line per note:
//! id, path, hash, ok|failed, has-output (0|1), dependency count, then a (path, hash) pair per dependency, and finally the diagnostic.
//! The diagnostic is last because it is the only field which may be empty.
class BuildState
{
  public:
	//! Hash of everything which affects every note (preamble, class, width, mode, engine version), as of the last build
	std::string GlobalHash;

	//! One record per note, keyed by note id
	std::map<std::string, NoteRecord> Records;

	//! Reads a saved state. A missing, unreadable or out-of-date file gives an empty state (which makes every note dirty), never an error
	static BuildState Load(const std::filesystem::path &file);

	//! Writes the state atomically
	void Save(const std::filesystem::path &file) const;

	//! Drops the records of notes which no longer exist
	void RetainOnly(const std::set<std::string> &ids);

	//! Bumped whenever the file layout changes, so old files are discarded rather than misread
	static constexpr int FormatVersion = 2;
};
