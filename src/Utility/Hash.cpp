#include "Hash.h"
#include <format>

uint64_t Utility::Hash(std::string_view input)
{
	uint64_t hash = 14695981039346656037ull;
	for (unsigned char c : input)
	{
		hash ^= c;
		hash *= 1099511628211ull;
	}
	return hash;
}

std::string Utility::HashHex(std::string_view input)
{
	return std::format("{:016x}", Hash(input));
}
