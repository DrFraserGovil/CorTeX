#include "Files.h"
#include <JSL.h>
#include <cstdint>
#include <cstdio>
#include <map>

void Utility::WriteAtomic(const std::filesystem::path &target, std::string_view content)
{
	auto temporary = target;
	temporary += ".tmp";
	JSL::IO::writeString(temporary, content);
	std::filesystem::rename(temporary, target);
}

std::string Utility::JsonString(std::string_view input)
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

std::string Utility::Base64(std::string_view bytes)
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

std::optional<std::string> Utility::ImageMediaType(const std::filesystem::path &file)
{
	static const std::map<std::string, std::string> types = {
		{".png", "image/png"},
		{".jpg", "image/jpeg"},
		{".jpeg", "image/jpeg"},
		{".gif", "image/gif"},
		{".webp", "image/webp"},
		{".svg", "image/svg+xml"},
	};
	auto extension = file.extension().string();
	for (auto &c : extension) c = std::tolower(static_cast<unsigned char>(c));
	auto type = types.find(extension);
	if (type == types.end()) return std::nullopt;
	return type->second;
}

std::optional<std::string> Utility::ImageDataUri(const std::filesystem::path &file)
{
	auto type = ImageMediaType(file);
	if (!type) return std::nullopt;
	return "data:" + *type + ";base64," + Base64(JSL::IO::getFile(file));
}
