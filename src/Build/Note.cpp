#include "Note.h"
#include "../Settings/RuntimeSettings.h"
#include "../Utility/Hash.h"
#include "../Utility/Sanitise.h"
#include <JSL.h>
#include <algorithm>
// A line consisting solely of five or more dashes (surrounding whitespace allowed) delineates sections.
// Five, so that TeX's en-/em-dashes (-- and ---) on a line of their own are never mistaken for one.
static bool IsSeparator(std::string_view line)
{
	auto trimmed = JSL::String::trim(line);
	if (trimmed.size() < 5) return false;

	std::vector<char> allowedSeparators = {'-', '='};
	for (auto &sep : allowedSeparators)
	{
		if (trimmed.find_first_not_of(sep) == std::string::npos)
		{
			return true;
		}
	}
	return false;
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
	Key = Utility::HashHex(Id);
	auto options = syntax;
	options.Origin = Key;
	auto processed = Syntax::Process(Body, options);
	ProcessedBody = std::move(processed.Text);
	Links = std::move(processed.Links);
	Transclusions = std::move(processed.Transclusions);

	/// A REDIRECT STUB HAS NO CONTENT OF ITS OWN
	// anything written in its body is ignored (and said so, so it isn't silently lost); its links and transclusions don't count
	if (!Redirect.empty())
	{
		bool hasContent = false;
		for (auto &line : JSL::String::split(Body, "\n"))
		{
			auto trimmed = JSL::String::trim(line);
			if (!trimmed.empty() && !trimmed.starts_with("%")) hasContent = true;
		}
		if (hasContent)
		{
			LOG(WARN) << RelativePath.generic_string() << " redirects to '" << Redirect << "', so the text in its body is ignored";
		}
		ProcessedBody.clear();
		Links.clear();
		Transclusions.clear();
	}
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
		else if (JSL::String::iEquals(key, "redirect"))
		{
			Redirect = value;
		}
		else if (JSL::String::iEquals(key, "graph"))
		{
			if (JSL::String::iEquals(value, "hidden"))
			{
				GraphHidden = true;
			}
			else
			{
				LOG(WARN) << RelativePath.generic_string() << ": unknown graph setting '" << value << "' (the only option is 'graph: hidden'); ignored";
			}
		}
		else if (JSL::String::iEquals(key, "sidebar"))
		{
			for (auto &item : JSL::String::split(value, ","))
			{
				auto trimmed = JSL::String::trim(item);
				if (!trimmed.empty()) Sidebar.push_back(trimmed);
			}
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
