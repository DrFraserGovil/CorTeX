#include "Note.h"
#include "../Settings/RuntimeSettings.h"
#include "../Utility/Sanitise.h"
#include <JSL.h>
#include <algorithm>
// A line consisting solely of five or more dashes (surrounding whitespace allowed) delineates sections.
// Five, so that TeX's en-/em-dashes (-- and ---) on a line of their own are never mistaken for one.
static bool IsSeparator(std::string_view line)
{
	auto trimmed = JSL::String::trim(line);
	if (trimmed.size() < 5) return false;

	std::vector<std::string> allowedSeparators = {"-", "="};
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

Note::Note(const std::filesystem::path &root, const std::filesystem::path &notePath, const Syntax::Options &syntax) : SourceAbsolutePath(notePath)
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

	/// APPLY THE EXTENDED SYNTAX
	// purely local to this note: what the links point at is resolved later, by the Index
	auto processed = Syntax::Process(Body, syntax);
	ProcessedBody = std::move(processed.Text);
	Links = std::move(processed.Links);
	for (auto &warning : processed.Warnings)
	{
		LOG(WARN) << RelativePath.generic_string() << ":" << BodyLine + warning.Line - 1 << ": " << warning.Message;
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
		else if (JSL::String::iEquals(key, "alias") || JSL::String::iEquals(key, "aliases"))
		{
			for (auto &alias : JSL::String::split(value, ","))
			{
				auto trimmed = JSL::String::trim(alias);
				if (!trimmed.empty()) Aliases.push_back(trimmed);
			}
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

bool Note::HasNoteHeader(const std::filesystem::path &file)
{
	auto lines = JSL::IO::getFileLines(file);
	for (auto &line : lines)
	{
		if (IsSeparator(line))
		{
			return false; // reached the end of the would-be header without finding a title
		}
		auto delim = line.find(':');
		if (delim != std::string::npos && JSL::String::iEquals(JSL::String::trim(std::string_view(line).substr(0, delim)), "title"))
		{
			// a title line only counts if a separator follows it somewhere
			return std::any_of(lines.begin(), lines.end(), [](auto &l) { return IsSeparator(l); });
		}
	}
	return false;
}

std::string Note::Slug(const std::filesystem::path &relativePath)
{
	auto stem = relativePath;
	stem.replace_extension();
	// '/' survives so that ids mirror the folder structure; lowercase so ids don't depend on how a file name was capitalised
	return Utility::Sanitise(stem.generic_string(), "/-_", true);
}
