#include "Files.h"
#include <JSL.h>
#include <cstdio>

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
