#pragma once
#include <cstdint>
#include <string>
#include <string_view>

//! Small text utilities which JSL doesn't provide
namespace Util
{
	//! Encodes a string as a JSON string literal (which is also a valid JS string literal), quotes included
	std::string Json(std::string_view input);

	//! Encodes bytes as standard base64 (with '=' padding), e.g. for a data: URI
	std::string Base64(std::string_view bytes);

	//! FNV-1a (64-bit): tiny, and gives the same answer in every build, on every compiler (unlike std::hash), so it is safe to persist
	uint64_t Hash(std::string_view input);

	//! Hash(input) as 16 lowercase hex digits, for storing in text files and names
	std::string HashHex(std::string_view input);

	/*! Makes a string safe for use as an identifier (a file name, a socket name, a URL fragment)
		@details Letters and digits are kept; any character in allowedSymbols is kept; everything else (spaces, punctuation, non-ASCII bytes) becomes the replacement
		@returns A string of the same length as input, containing only allowed characters
	*/
	std::string Sanitise(std::string_view input, std::string_view allowedSymbols = "-_", bool lowercase = false, char replacement = '-');

	//! Undoes %XX escapes (as written by a browser's encodeURIComponent); anything malformed is left as it is
	std::string PercentDecode(std::string_view text);
} // namespace Util
