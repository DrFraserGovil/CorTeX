#include "Header.h"
#include "Util/Files.h"
#include <JSL.h>
#include <algorithm>
#include <cctype>

namespace
{
	std::string Join(std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end)
	{
		std::string out;
		for (auto it = begin; it != end; ++it)
		{
			out += *it + "\n";
		}
		return out;
	}

	bool IsOneOf(std::string_view value, std::initializer_list<const char *> words)
	{
		return std::any_of(words.begin(), words.end(), [&](const char *w) { return JSL::String::iEquals(value, w); });
	}

	// The key of a "key: value" line, or empty if it has no colon
	std::string KeyOf(const std::string &line)
	{
		auto delim = line.find(':');
		return delim == std::string::npos ? "" : JSL::String::trim(std::string_view(line).substr(0, delim));
	}

	std::vector<std::string> List(std::string_view value)
	{
		std::vector<std::string> out;
		for (auto &item : JSL::String::split(std::string(value), ","))
		{
			auto trimmed = JSL::String::trim(item);
			if (!trimmed.empty()) out.push_back(trimmed);
		}
		return out;
	}
} // namespace

std::string Notes::PlainText(std::string_view latex)
{
	std::string out;
	for (size_t i = 0; i < latex.size(); ++i)
	{
		char c = latex[i];
		if (c == '\\' && i + 1 < latex.size())
		{
			char next = latex[i + 1];
			if (std::string_view("&%$#_{}").find(next) != std::string_view::npos)
			{
				out += next;
				++i;
				continue;
			}
			if (next == ',' || next == ' ')
			{
				out += ' ';
				++i;
				continue;
			}
			// a command name: dropped if a group follows it (whose text is kept), else left as written
			size_t end = i + 1;
			while (end < latex.size() && std::isalpha(static_cast<unsigned char>(latex[end]))) ++end;
			if (end > i + 1 && end < latex.size() && latex[end] == '{')
			{
				i = end - 1; // the brace itself is dropped below
				continue;
			}
			out += latex.substr(i, end - i);
			i = end - 1;
			continue;
		}
		if (c == '{' || c == '}') continue;
		if (c == '~')
		{
			out += ' ';
			continue;
		}
		if (latex.compare(i, 3, "---") == 0)
		{
			out += "\xE2\x80\x94"; // —
			i += 2;
			continue;
		}
		if (latex.compare(i, 2, "--") == 0)
		{
			out += "\xE2\x80\x93"; // –
			i += 1;
			continue;
		}
		out += c;
	}
	return out;
}

bool Notes::IsSeparator(std::string_view line)
{
	auto trimmed = JSL::String::trim(line);
	if (trimmed.size() < 5) return false;
	return trimmed.find_first_not_of('-') == std::string::npos || trimmed.find_first_not_of('=') == std::string::npos;
}

Notes::Sections Notes::Split(std::string_view text)
{
	auto lines = JSL::String::split(std::string(text), "\n");
	if (!text.empty() && text.back() == '\n') lines.pop_back(); // the final newline ends the last line; it doesn't start another

	// only the first two separators are structural; any later ones are part of the body
	std::vector<size_t> separators;
	for (size_t i = 0; i < lines.size() && separators.size() < 2; ++i)
	{
		if (IsSeparator(lines[i])) separators.push_back(i);
	}

	Sections out;
	auto begin = lines.cbegin();
	switch (separators.size())
	{
		case 0:
			out.Body = Join(begin, lines.cend());
			break;
		case 1:
			out.HasHeader = true;
			out.Header = {begin, begin + separators[0]};
			out.Body = Join(begin + separators[0] + 1, lines.cend());
			out.BodyLine = separators[0] + 2; // separators are 0-based indices; the section starts on the line after
			break;
		default:
			out.HasHeader = true;
			out.Header = {begin, begin + separators[0]};
			out.LocalPreamble = Join(begin + separators[0] + 1, begin + separators[1]);
			out.Body = Join(begin + separators[1] + 1, lines.cend());
			out.LocalPreambleLine = separators[0] + 2;
			out.BodyLine = separators[1] + 2;
			break;
	}
	return out;
}

Notes::Header Notes::Header::Parse(const std::vector<std::string> &lines, std::vector<HeaderProblem> &problems)
{
	Header header;
	std::vector<std::string> seen;
	for (size_t number = 1; number <= lines.size(); ++number)
	{
		auto &line = lines[number - 1];
		auto key = JSL::String::getLower(KeyOf(line));
		if (key.empty()) continue;
		if (key == "aliases") key = "alias";
		// the first occurrence wins, except for the fields which take lists: every line of those adds to the list
		bool list = key == "alias" || key == "sidebar" || key == "copy-sidebar" || key == "graph";
		if (!list && std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
		seen.push_back(key);
		auto value = JSL::String::trim(std::string_view(line).substr(line.find(':') + 1));

		if (key == "title")
		{
			header.Title = value;
		}
		else if (key == "redirect")
		{
			header.Redirect = value;
		}
		else if (key == "alias")
		{
			for (auto &alias : List(value)) header.Aliases.push_back(alias);
		}
		else if (key == "sidebar")
		{
			for (auto &item : List(value)) header.Sidebar.push_back(item);
		}
		else if (key == "copy-sidebar")
		{
			for (auto &name : List(value)) header.CopySidebar.push_back(name);
		}
		else if (key == "graph")
		{
			for (auto &setting : List(value))
			{
				if (JSL::String::iEquals(setting, "hidden")) header.GraphHidden = true;
				else if (JSL::String::iEquals(setting, "loose")) header.GraphLoose = true;
				else problems.push_back({number, "unknown graph setting '" + setting + "' (the options are 'hidden' and 'loose'); ignored"});
			}
		}
		else if (key == "show-title")
		{
			if (IsOneOf(value, {"yes", "true", "on"})) header.ShowTitle = true;
			else if (IsOneOf(value, {"no", "false", "off"})) header.ShowTitle = false;
			else problems.push_back({number, "unknown show-title value '" + value + "' (use yes or no); ignored"});
		}
		else if (key == "todo")
		{
			if (!IsOneOf(value, {"no", "false", "off"})) header.Todo = IsOneOf(value, {"yes", "true", "on"}) ? std::string() : value;
		}
	}
	return header;
}

bool Notes::SetTodo(const std::filesystem::path &file, bool on)
{
	/// SPLIT INTO LINES, KEEPING WHETHER THE FILE ENDED WITH A NEWLINE
	auto text = JSL::IO::getFile(file);
	std::vector<std::string> lines;
	for (size_t start = 0; start < text.size();)
	{
		auto end = text.find('\n', start);
		if (end == std::string::npos) end = text.size();
		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	bool finalNewline = !text.empty() && text.back() == '\n';

	/// FIND THE HEADER AND ITS TODO LINES
	auto separator = std::find_if(lines.begin(), lines.end(), [](auto &l) { return IsSeparator(l); });
	if (separator == lines.end())
	{
		throw std::runtime_error(file.string() + " has no header (no separator line)");
	}
	auto isTodo = [](const std::string &line) { return JSL::String::iEquals(KeyOf(line), "todo"); };
	auto todoValue = [](const std::string &line) { return JSL::String::trim(std::string_view(line).substr(line.find(':') + 1)); };
	size_t headerEnd = separator - lines.begin();

	/// REWRITE THEM
	bool changed = false;
	if (on)
	{
		// the first todo line is the one that counts (as in Parse); a 'no' there is replaced, any other value is already flagged
		auto first = std::find_if(lines.begin(), lines.begin() + headerEnd, isTodo);
		if (first == lines.begin() + headerEnd)
		{
			lines.insert(lines.begin() + headerEnd, "todo:");
			changed = true;
		}
		else if (IsOneOf(todoValue(*first), {"no", "false", "off"}))
		{
			*first = "todo:";
			changed = true;
		}
	}
	else
	{
		auto end = std::remove_if(lines.begin(), lines.begin() + headerEnd, isTodo);
		changed = end != lines.begin() + headerEnd;
		lines.erase(end, lines.begin() + headerEnd);
	}
	if (!changed) return false;

	std::string out;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		out += lines[i];
		if (i + 1 < lines.size() || finalNewline) out += "\n";
	}
	Util::WriteAtomic(file, out);
	return true;
}
