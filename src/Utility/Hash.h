#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace Utility
{
	//! FNV-1a (64-bit): tiny, and gives the same answer in every build, on every compiler (unlike std::hash), so it is safe to persist
	uint64_t Hash(std::string_view input);

	//! Hash(input) as 16 lowercase hex digits, for storing in text files and names
	std::string HashHex(std::string_view input);
} // namespace Utility
