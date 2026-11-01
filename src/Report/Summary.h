#pragma once
#include "Report.h"
#include <filesystem>
#include <string>

//! A command's report, told twice (DESIGN.md §13.4): in full in the build log, and briefly in the terminal
namespace Report
{
	//! Writes everything to the build log (replacing the last one): the account, then every entry in full, by kind
	void WriteLog(const std::filesystem::path &file, const Log &log, const std::string &title);

	//! The terminal's version: one line per failed note, then a count of everything else by kind, pointing at the build log
	std::string Brief(const Log &log, const std::filesystem::path &logFile);
} // namespace Report
