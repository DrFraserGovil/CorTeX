#include "Syntax.h"
#include "../Utility/Sanitise.h"
#include <JSL.h>
#include <regex>

std::string Syntax::Slug(std::string_view text)
{
	auto sanitised = Utility::Sanitise(JSL::String::trim(text), "-_", true);
	std::string slug;
	for (char c : sanitised)
	{
		if (c == '-' && (slug.empty() || slug.back() == '-')) continue; // collapse runs, and drop leading dashes
		slug += c;
	}
	while (!slug.empty() && slug.back() == '-')
	{
		slug.pop_back();
	}
	return slug;
}

// A line which is nothing but a "cortex on" / "cortex off" comment
static bool IsMarker(const std::string &line, bool &turnsOn)
{
	static const std::regex marker(R"(^\s*%+\s*cortex\s+(on|off)\s*$)", std::regex::icase);
	std::smatch match;
	if (!std::regex_match(line, match, marker)) return false;
	turnsOn = JSL::String::iEquals(match[1].str(), "on");
	return true;
}

// Rewrites the links on a single line (which is known to be outside any "cortex off" region)
static std::string ProcessLine(const std::string &line, size_t lineNumber, Syntax::Result &result)
{
	auto warn = [&](std::string message) { result.Warnings.push_back({lineNumber, std::move(message)}); };

	std::string out;
	size_t i = 0;
	while (i < line.size())
	{
		char c = line[i];

		/// CONTROL SEQUENCES ARE LEFT ALONE
		// a backslash and the character after it are one unit: \[ \% \\ etc. never start anything
		if (c == '\\')
		{
			out += line.substr(i, 2);
			i += 2;
			continue;
		}

		/// COMMENTS ARE LEFT ALONE
		if (c == '%')
		{
			out += line.substr(i);
			break;
		}

		/// RESERVED SYNTAX: TRANSCLUSION
		if (line.compare(i, 3, "![[") == 0)
		{
			auto close = line.find("]]", i + 3);
			size_t end = (close == std::string::npos) ? line.size() : close + 2;
			warn("transclusion (![[...]]) isn't supported yet; left as written");
			out += line.substr(i, end - i);
			i = end;
			continue;
		}

		/// LINKS
		if (line.compare(i, 2, "[[") == 0)
		{
			auto close = line.find("]]", i + 2);
			if (close == std::string::npos)
			{
				warn("'[[' with no closing ']]' on the same line; left as written (to write a literal [[, use \\[[ or [ [)");
				out += "[[";
				i += 2;
				continue;
			}

			auto inner = line.substr(i + 2, close - i - 2);
			auto bar = inner.find('|');
			std::string target = JSL::String::trim(inner.substr(0, bar));
			std::string text = (bar == std::string::npos) ? target : inner.substr(bar + 1);

			// sections aren't supported yet: link to the note itself, so the link still works (a bare # would be a TeX error)
			auto hash = target.find('#');
			if (hash != std::string::npos)
			{
				warn("section links ([[note#section]]) aren't supported yet; linking to '" + JSL::String::trim(target.substr(0, hash)) + "' instead");
				target = JSL::String::trim(target.substr(0, hash));
				if (bar == std::string::npos) text = target;
			}

			if (target.empty() || Syntax::Slug(target).empty())
			{
				warn("link [[" + inner + "]] has no target; left as written");
				out += line.substr(i, close + 2 - i);
			}
			else
			{
				auto slug = Syntax::Slug(target);
				result.Links.push_back({target, slug, lineNumber});
				out += "\\CortexLink{" + slug + "}{" + text + "}";
			}
			i = close + 2;
			continue;
		}

		out += c;
		++i;
	}
	return out;
}

Syntax::Result Syntax::Process(std::string_view body)
{
	Result result;
	bool enabled = true;
	size_t lineNumber = 0;
	size_t start = 0;
	while (start <= body.size())
	{
		/// SPLIT INTO LINES (KEEPING THE EXACT LINE STRUCTURE)
		auto end = body.find('\n', start);
		bool lastLine = (end == std::string_view::npos);
		std::string line(body.substr(start, lastLine ? std::string_view::npos : end - start));
		++lineNumber;

		/// MARKERS SWITCH THE SYNTAX OFF AND ON; OTHERWISE PROCESS THE LINE IF ENABLED
		bool turnsOn;
		if (IsMarker(line, turnsOn))
		{
			enabled = turnsOn;
			result.Text += line;
		}
		else
		{
			result.Text += enabled ? ProcessLine(line, lineNumber, result) : line;
		}

		if (lastLine) break;
		result.Text += '\n';
		start = end + 1;
	}
	return result;
}
