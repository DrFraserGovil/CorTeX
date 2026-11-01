#include "Text.h"
#include <cctype>
#include <cstdio>
#include <format>

std::string Util::Json(std::string_view input)
{
	std::string out = "\"";
	out.reserve(input.size() + 2);
	for (char c : input)
	{
		switch (c)
		{
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(c) < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					out += buf;
				}
				else
				{
					out += c;
				}
		}
	}
	return out + "\"";
}

std::string Util::Base64(std::string_view bytes)
{
	static constexpr char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve((bytes.size() + 2) / 3 * 4);
	for (size_t i = 0; i < bytes.size(); i += 3)
	{
		// three bytes (zero-padded at the end) become four 6-bit digits; '=' stands in for the digits a short final group lacks
		uint32_t group = static_cast<unsigned char>(bytes[i]) << 16;
		if (i + 1 < bytes.size()) group |= static_cast<unsigned char>(bytes[i + 1]) << 8;
		if (i + 2 < bytes.size()) group |= static_cast<unsigned char>(bytes[i + 2]);
		out += digits[(group >> 18) & 63];
		out += digits[(group >> 12) & 63];
		out += i + 1 < bytes.size() ? digits[(group >> 6) & 63] : '=';
		out += i + 2 < bytes.size() ? digits[group & 63] : '=';
	}
	return out;
}

uint64_t Util::Hash(std::string_view input)
{
	uint64_t hash = 14695981039346656037ull;
	for (unsigned char c : input)
	{
		hash ^= c;
		hash *= 1099511628211ull;
	}
	return hash;
}

std::string Util::HashHex(std::string_view input)
{
	return std::format("{:016x}", Hash(input));
}

std::string Util::Sanitise(std::string_view input, std::string_view allowedSymbols, bool lowercase, char replacement)
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

std::string Util::PercentDecode(std::string_view text)
{
	std::string out;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) && std::isxdigit(static_cast<unsigned char>(text[i + 2])))
		{
			out += static_cast<char>(std::stoi(std::string(text.substr(i + 1, 2)), nullptr, 16));
			i += 2;
		}
		else
		{
			out += text[i];
		}
	}
	return out;
}
