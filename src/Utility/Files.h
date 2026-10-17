#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace Utility
{
	//! Writes via a temporary file and a rename, so the target is always either the complete old version or the complete new one:
	//! a crash (or a reader mid-write) never sees a half-written file
	void WriteAtomic(const std::filesystem::path &target, std::string_view content);

	//! Encodes a string as a JSON string literal (which is also a valid JS string literal), quotes included
	std::string JsonString(std::string_view input);
} // namespace Utility
