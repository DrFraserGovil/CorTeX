#pragma once
#include "Note.h"
#include "Report/Report.h"
#include <filesystem>
#include <optional>
#include <string>

//! Reading files into Notes (DESIGN.md §16). Each format has a Reader, chosen by the file's extension; only LaTeX exists yet. A new
//! format is a new Reader which fills in the same Note: everything after reading works on Notes, whatever they were written in
namespace Notes
{
	//! What reading a note depends on from the project's settings
	struct ReadOptions
	{
		bool Extended = false;
		std::string ListType = "itemize";
	};

	class Reader
	{
	  public:
		virtual ~Reader() = default;
		//! Reads the file at source (absolute; inside root) into a Note, adding any problems to report
		//! @returns nullopt if the file isn't a note (no title), which is reported as skipped
		virtual std::optional<Note> Read(const std::filesystem::path &root, const std::filesystem::path &source, const ReadOptions &options,
										 Report::Log &report) const = 0;
	};

	//! The Reader for a file's format, or nullptr if CorTeX can't read it
	const Reader *ReaderFor(const std::filesystem::path &file);

	//! Reads any note file, through its format's Reader; nullopt if it isn't a note, or isn't a format CorTeX reads
	std::optional<Note> Read(const std::filesystem::path &root, const std::filesystem::path &source, const ReadOptions &options, Report::Log &report);
} // namespace Notes
