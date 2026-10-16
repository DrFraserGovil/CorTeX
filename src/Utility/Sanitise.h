#pragma once
#include <string>
#include <string_view>

namespace Utility
{
	/*! Makes a string safe for use as an identifier (a file name, a socket name, a URL fragment)
		@details Letters and digits are kept; any character in allowedSymbols is kept; everything else (spaces, punctuation, non-ASCII bytes) becomes the replacement
		@param input The string to sanitise
		@param allowedSymbols The non-alphanumeric characters which may pass through unchanged
		@param lowercase If true, letters are converted to lowercase
		@param replacement The character substituted for anything not allowed
		@returns A string of the same length as input, containing only allowed characters
	*/
	std::string Sanitise(std::string_view input, std::string_view allowedSymbols = "-_", bool lowercase = false, char replacement = '-');
} // namespace Utility
