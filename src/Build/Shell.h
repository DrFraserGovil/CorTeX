#pragma once
#include <filesystem>
#include <string>

namespace Shell
{
	//! Wraps a path in single quotes, escaping any embedded quotes, for safe use in a POSIX shell command
	std::string Quote(const std::filesystem::path &path);

	//! Runs a shell command from the given working directory, sending all output to logFile
	//! @returns The exit code of the command
	int Run(const std::string &command, const std::filesystem::path &workingDir, const std::filesystem::path &logFile);
} // namespace Shell
