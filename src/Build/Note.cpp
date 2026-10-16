#include "Note.h"
#include "../Settings/RuntimeSettings.h"
#include <JSL.h>
// A line consisting solely of five or more dashes (surrounding whitespace allowed) delineates sections.
// Five, so that TeX's en-/em-dashes (-- and ---) on a line of their own are never mistaken for one.
static bool IsSeparator(std::string_view line)
{
	auto trimmed = JSL::String::trim(line);
	return trimmed.size() >= 5 && trimmed.find_first_not_of('-') == std::string::npos;
}

static std::string Join(std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end)
{
	std::string out;
	for (auto it = begin; it != end; ++it)
	{
		out += *it + "\n";
	}
	return out;
}

Note::Note(const std::filesystem::path &root, const std::filesystem::path &notePath) : SourceAbsolutePath(notePath)
{
	RelativePath = notePath.lexically_relative(root);

	auto lines = JSL::IO::getFileLines(notePath);

	// only the first two separators are structural; any later ones are part of the body
	std::vector<size_t> separators;
	for (size_t i = 0; i < lines.size() && separators.size() < 2; ++i)
	{
		if (IsSeparator(lines[i]))
		{
			separators.push_back(i);
		}
	}

	auto begin = lines.cbegin();
	switch (separators.size())
	{
		case 0:
			Body = Join(begin, lines.cend());
			break;
		case 1:
			ParseHeader({begin, begin + separators[0]});
			Body = Join(begin + separators[0] + 1, lines.cend());
			BodyLine = separators[0] + 2; // separators are 0-based indices; the section starts on the line after
			break;
		default:
			ParseHeader({begin, begin + separators[0]});
			LocalPreamble = Join(begin + separators[0] + 1, begin + separators[1]);
			Body = Join(begin + separators[1] + 1, lines.cend());
			LocalPreambleLine = separators[0] + 2;
			BodyLine = separators[1] + 2;
			break;
	}

	if (Id.empty())
	{
		Id = Slug(RelativePath);
	}
}

void Note::ParseHeader(const std::vector<std::string> &lines)
{
	// traverse backwards so that the first declaration takes priority
	for (auto it = lines.rbegin(); it != lines.rend(); ++it)
	{
		auto delim = it->find(':');
		if (delim == std::string::npos) continue;

		auto key = JSL::String::trim(std::string_view(*it).substr(0, delim));
		auto value = JSL::String::trim(std::string_view(*it).substr(delim + 1));
		if (JSL::String::iEquals(key, "title"))
		{
			Title = value;
		}
		else if (JSL::String::iEquals(key, "id"))
		{
			if (value.find('/') != std::string::npos)
			{
				LOG(ERROR) << "Explicit id '" << value << "' in " << RelativePath << " contains '/', which is reserved for path-derived ids";
				throw std::runtime_error("Bad note id");
			}
			Id = value;
		}
	}
}

std::string Note::Slug(const std::filesystem::path &relativePath)
{
	auto stem = relativePath;
	stem.replace_extension();
	std::string slug;
	for (char c : stem.generic_string())
	{
		if (std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '-' || c == '_')
		{
			slug += std::tolower(static_cast<unsigned char>(c));
		}
		else
		{
			slug += '-';
		}
	}
	return slug;
}
