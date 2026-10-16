#include "Sanitise.h"
#include <cctype>

std::string Utility::Sanitise(std::string_view input, std::string_view allowedSymbols, bool lowercase, char replacement)
{
	std::string out;
	out.reserve(input.size());
	for (char c : input)
	{
		// the unsigned cast matters: std::isalnum on a negative char (any non-ASCII byte) is undefined behaviour
		auto uc = static_cast<unsigned char>(c);
		if (std::isalnum(uc) || allowedSymbols.find(c) != std::string_view::npos)
		{
			out += lowercase ? static_cast<char>(std::tolower(uc)) : c;
		}
		else
		{
			out += replacement;
		}
	}
	return out;
}
