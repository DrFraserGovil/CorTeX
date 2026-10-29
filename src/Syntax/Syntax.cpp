#include "Syntax.h"
#include "../Utility/Sanitise.h"
#include <JSL.h>
#include <cctype>
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

namespace
{
	//! Which kind of maths (if any) the scan is inside; tracked through delimiters and the standard maths environments (never a
	//! general environment stack), and carried from line to line
	enum class Maths
	{
		None,
		Inline,		 //!< $ ... $
		Display,	 //!< $$ ... $$
		Paren,		 //!< \( ... \)
		Bracket,	 //!< \[ ... \]
		Environment, //!< \begin{equation} ... \end{equation}, or another of MathsEnvironments
	};

	//! The environments whose contents are maths: \begin{X} opens maths like \[ does, and only the matching \end{X} closes it
	bool IsMathsEnvironment(std::string_view name)
	{
		static const std::string_view names[] = {"equation", "align", "gather", "multline", "flalign", "alignat", "eqnarray", "math", "displaymath"};
		if (!name.empty() && name.back() == '*') name.remove_suffix(1);
		for (auto n : names)
		{
			if (name == n) return true;
		}
		return false;
	}

	//! Inline formatting markers and the LaTeX they become (salvaged from the original CorTeX generator)
	struct Format
	{
		std::string_view Marker;
		std::string_view Open;
		std::string_view Close;
	};
	constexpr Format Formats[] = {
		{"***", "\\textbf{\\textit{", "}}"},
		{"**", "\\textbf{", "}"},
		{"*", "\\textit{", "}"},
		{"__", "\\textsc{", "}"},
		{"_", "\\texttt{", "}"},
	};

	const Format *FindFormat(std::string_view marker)
	{
		for (auto &format : Formats)
		{
			if (format.Marker == marker) return &format;
		}
		return nullptr;
	}

	//! A line which is nothing but a "cortex on" / "cortex off" comment
	bool IsMarker(const std::string &line, bool &turnsOn)
	{
		static const std::regex marker(R"(^\s*%+\s*cortex\s+(on|off)\s*$)", std::regex::icase);
		std::smatch match;
		if (!std::regex_match(line, match, marker)) return false;
		turnsOn = JSL::String::iEquals(match[1].str(), "on");
		return true;
	}

	//! The sectioning commands for heading levels 1, 2, ...: everything the standard classes provide
	constexpr std::string_view Headings[] = {"section", "subsection", "subsubsection", "paragraph", "subparagraph"};

	//! How a line starts, as far as star lists are concerned
	struct ListLine
	{
		enum Kind
		{
			Item,		  //!< "<ws>*** text": a list item at depth = number of stars
			ManualItem,	  //!< "<ws>\item...": the author's own item; left alone, lists stay open
			Neutral,	  //!< a comment-only line, or an indented continuation of an item: lists stay open
			Other,		  //!< anything else (including a blank line): ends any open lists
		} Type = Other;
		size_t Depth = 0;
		std::string ForcedType;	 //!< "itemize" or "enumerate" if the item used *ITEM / *ENUM
		std::string Indent;		 //!< the leading whitespace, preserved
		std::string Remainder;	 //!< the item's text
		size_t RemainderColumn = 0;
	};

	ListLine ClassifyListLine(const std::string &line, bool inList)
	{
		ListLine out;
		auto start = line.find_first_not_of(" \t");
		if (start == std::string::npos)
		{
			return out; // blank: ends lists
		}
		out.Indent = line.substr(0, start);
		if (line.compare(start, 5, "\\item") == 0)
		{
			out.Type = ListLine::ManualItem;
			return out;
		}
		if (line[start] == '%')
		{
			out.Type = ListLine::Neutral;
			return out;
		}

		size_t pos = start;
		while (pos < line.size() && line[pos] == '*')
		{
			++pos;
		}
		size_t stars = pos - start;
		if (stars > 0 && pos < line.size())
		{
			if (line[pos] == ' ')
			{
				out.Type = ListLine::Item;
				out.RemainderColumn = pos + 1;
			}
			else if (line.compare(pos, 5, "ENUM ") == 0 || line.compare(pos, 5, "ITEM ") == 0)
			{
				out.Type = ListLine::Item;
				out.ForcedType = (line[pos] == 'E') ? "enumerate" : "itemize";
				out.RemainderColumn = pos + 5;
			}
		}
		if (out.Type == ListLine::Item)
		{
			out.Depth = stars;
			out.Remainder = line.substr(out.RemainderColumn);
			return out;
		}

		// an indented line inside a list continues the current item
		out.Type = (inList && start > 0) ? ListLine::Neutral : ListLine::Other;
		return out;
	}

	//! The scan of one note body: state that carries from line to line, plus the output
	class Processor
	{
	  public:
		Processor(const Syntax::Options &options) : Options(options)
		{
		}

		Syntax::Result Run(std::string_view body);

	  private:
		const Syntax::Options &Options;
		Syntax::Result Result;
		Maths State = Maths::None;
		size_t MathsOpenedOn = 0;
		//! With State == Maths::Environment: the environment's name (so only its own \end closes it)
		std::string MathsEnvironment;
		std::vector<std::string> OpenLists;
		size_t LineNumber = 0;

		void Warn(std::string message)
		{
			Result.Warnings.push_back({LineNumber, std::move(message)});
		}

		std::string ProcessLine(const std::string &line);
		bool Heading(const std::string &line, std::string &output);
		std::string Link(const std::string &line, size_t &i);
		std::string CloseLists(size_t toDepth);
		std::string ListLineOutput(const std::string &line);
	};

	// Handles "[[" or "![[" at line[i], advancing i past what it consumed
	std::string Processor::Link(const std::string &line, size_t &i)
	{
		/// TRANSCLUSION
		if (line.compare(i, 3, "![[") == 0)
		{
			auto close = line.find("]]", i + 3);
			if (close == std::string::npos)
			{
				Warn("'![[' with no closing ']]' on the same line; left as written");
				i += 3;
				return "![[";
			}
			auto inner = line.substr(i + 3, close - i - 3);
			std::string target = JSL::String::trim(inner.substr(0, inner.find('|'))); // any |text is meaningless here, and ignored
			auto hash = target.find('#');
			if (hash != std::string::npos)
			{
				Warn("section transclusion (![[note#section]]) isn't supported yet; transcluding the whole of '" + JSL::String::trim(target.substr(0, hash)) + "'");
				target = JSL::String::trim(target.substr(0, hash));
			}
			std::string out;
			if (Syntax::Slug(target).empty())
			{
				Warn("transclusion ![[" + inner + "]] has no target; left as written");
				out = line.substr(i, close + 2 - i);
			}
			else
			{
				auto slug = Syntax::Slug(target);
				Result.Transclusions.push_back({target, slug, LineNumber});
				out = "\\CortexTransclude{" + Options.Origin + "}{" + slug + "}";
			}
			i = close + 2;
			return out;
		}

		/// LINKS
		auto close = line.find("]]", i + 2);
		if (close == std::string::npos)
		{
			Warn("'[[' with no closing ']]' on the same line; left as written (to write a literal [[, use \\[[ or [ [)");
			i += 2;
			return "[[";
		}

		auto inner = line.substr(i + 2, close - i - 2);
		auto bar = inner.find('|');
		std::string target = JSL::String::trim(inner.substr(0, bar));
		std::string text = (bar == std::string::npos) ? target : inner.substr(bar + 1);

		// sections aren't supported yet: link to the note itself, so the link still works (a bare # would be a TeX error)
		auto hash = target.find('#');
		if (hash != std::string::npos)
		{
			Warn("section links ([[note#section]]) aren't supported yet; linking to '" + JSL::String::trim(target.substr(0, hash)) + "' instead");
			target = JSL::String::trim(target.substr(0, hash));
			if (bar == std::string::npos) text = target;
		}

		std::string out;
		if (target.empty() || Syntax::Slug(target).empty())
		{
			Warn("link [[" + inner + "]] has no target; left as written");
			out = line.substr(i, close + 2 - i);
		}
		else
		{
			auto slug = Syntax::Slug(target);
			Result.Links.push_back({target, slug, LineNumber});
			// the link text is ordinary text, so it gets the same treatment (formatting, maths) as the rest of the line
			out = "\\CortexLink{" + Options.Origin + "}{" + slug + "}{" + ProcessLine(text) + "}";
		}
		i = close + 2;
		return out;
	}

	// Rewrites one line (or the text of a list item), tracking maths as it goes
	std::string Processor::ProcessLine(const std::string &line)
	{
		std::string out;

		// an inline format opened on this line and not yet closed; formats never span lines
		bool formatOpen = false;
		size_t formatAt = 0;
		std::string formatMarker;

		size_t i = 0;
		while (i < line.size())
		{
			char c = line[i];

			/// CONTROL SEQUENCES: LEFT ALONE, BUT SOME OPEN OR CLOSE MATHS
			if (c == '\\')
			{
				// \begin{X} / \end{X} of a maths environment: copied whole, switching maths on or off
				if (State == Maths::None && line.compare(i, 7, "\\begin{") == 0)
				{
					if (auto close = line.find('}', i + 7); close != std::string::npos && IsMathsEnvironment(std::string_view(line).substr(i + 7, close - i - 7)))
					{
						State = Maths::Environment;
						MathsEnvironment = line.substr(i + 7, close - i - 7);
						MathsOpenedOn = LineNumber;
						out += line.substr(i, close + 1 - i);
						i = close + 1;
						continue;
					}
				}
				else if (State == Maths::Environment && line.compare(i, 6 + MathsEnvironment.size() + 1, "\\end{" + MathsEnvironment + "}") == 0)
				{
					State = Maths::None;
					size_t length = 6 + MathsEnvironment.size();
					out += line.substr(i, length);
					i += length;
					continue;
				}
				auto pair = line.substr(i, 2);
				if (State == Maths::None && (pair == "\\(" || pair == "\\["))
				{
					State = (pair == "\\(") ? Maths::Paren : Maths::Bracket;
					MathsOpenedOn = LineNumber;
				}
				else if ((State == Maths::Paren && pair == "\\)") || (State == Maths::Bracket && pair == "\\]"))
				{
					State = Maths::None;
				}
				out += pair;
				i += 2;
				continue;
			}

			/// COMMENTS: LEFT ALONE
			if (c == '%')
			{
				out += line.substr(i);
				break;
			}

			/// DOLLAR MATHS ($$ is checked first, so it isn't read as two $)
			if (c == '$')
			{
				bool dbl = line.compare(i, 2, "$$") == 0;
				if (dbl && (State == Maths::None || State == Maths::Display))
				{
					MathsOpenedOn = (State == Maths::None) ? LineNumber : MathsOpenedOn;
					State = (State == Maths::None) ? Maths::Display : Maths::None;
					out += "$$";
					i += 2;
					continue;
				}
				if (State == Maths::None || State == Maths::Inline)
				{
					MathsOpenedOn = (State == Maths::None) ? LineNumber : MathsOpenedOn;
					State = (State == Maths::None) ? Maths::Inline : Maths::None;
				}
				out += '$';
				++i;
				continue;
			}

			/// LINKS: EVERYWHERE, EVEN IN MATHS
			if (line.compare(i, 2, "[[") == 0 || line.compare(i, 3, "![[") == 0)
			{
				out += Link(line, i);
				continue;
			}

			/// INLINE FORMATTING: EXTENDED SYNTAX, TEXT ONLY
			if (Options.Extended && State == Maths::None && (c == '*' || c == '_'))
			{
				size_t run = line.find_first_not_of(c, i);
				if (run == std::string::npos) run = line.size();
				std::string marker = line.substr(i, run - i);
				char before = (i > 0) ? line[i - 1] : ' ';
				char after = (run < line.size()) ? line[run] : ' ';

				if (formatOpen && marker == formatMarker && !std::isspace(static_cast<unsigned char>(before)))
				{
					// a closer: replace the literal opening marker with the LaTeX, and close it here
					auto format = FindFormat(marker);
					out.replace(formatAt, marker.size(), format->Open);
					out += format->Close;
					formatOpen = false;
				}
				// an opener can't directly follow a letter, digit, backslash or closing brace: that keeps starred commands (\section*, \\*),
				// arithmetic (2*3, \x*\x in TikZ) and subscripts in maths the scan doesn't recognise (\vec{b}_1) from ever opening a format
				else if (FindFormat(marker) && !std::isspace(static_cast<unsigned char>(after)) && after != '}' && !std::isalnum(static_cast<unsigned char>(before)) && before != '\\' && before != '}')
				{
					if (formatOpen)
					{
						Warn("'" + formatMarker + "' was never closed; left as written");
					}
					formatOpen = true;
					formatAt = out.size();
					formatMarker = marker;
					out += marker; // written literally until a matching closer turns up
				}
				else
				{
					out += marker;
				}
				i = run;
				continue;
			}

			out += c;
			++i;
		}

		if (formatOpen)
		{
			Warn("'" + formatMarker + "' was never closed on this line; left as written");
		}
		return out;
	}

	// "<indent>#...# Title" or "<indent>#...#* Title": a heading. Returns false (output untouched) for any other line
	bool Processor::Heading(const std::string &line, std::string &output)
	{
		auto start = line.find_first_not_of(" \t");
		if (start == std::string::npos || line[start] != '#') return false;
		size_t pos = start;
		while (pos < line.size() && line[pos] == '#')
		{
			++pos;
		}
		size_t level = pos - start;
		bool starred = (pos < line.size() && line[pos] == '*');
		if (starred) ++pos;
		if (pos >= line.size() || line[pos] != ' ') return false; // "#hashtag", "##" alone: not headings

		constexpr size_t deepest = std::size(Headings);
		if (level > deepest)
		{
			Warn("headings go " + std::to_string(deepest) + " levels deep (#####, \\subparagraph); this one is treated as that level");
			level = deepest;
		}
		std::string title = JSL::String::trim(line.substr(pos + 1));
		// a heading ends any open lists, like any other unindented non-list line
		output = CloseLists(0) + line.substr(0, start) + "\\" + std::string(Headings[level - 1]) + (starred ? "*" : "") + "{" + ProcessLine(title) + "}";
		return true;
	}

	std::string Processor::CloseLists(size_t toDepth)
	{
		std::string out;
		while (OpenLists.size() > toDepth)
		{
			out += "\\end{" + OpenLists.back() + "}";
			OpenLists.pop_back();
		}
		return out;
	}

	// A line seen while the extended syntax is on and outside maths: may open, continue or close star lists
	std::string Processor::ListLineOutput(const std::string &line)
	{
		auto list = ClassifyListLine(line, !OpenLists.empty());
		switch (list.Type)
		{
			case ListLine::ManualItem:
			case ListLine::Neutral:
				return ProcessLine(line);

			case ListLine::Other:
				return CloseLists(0) + ProcessLine(line);

			case ListLine::Item:
			default:
			{
				// close any deeper lists first; then *ENUM / *ITEM at this depth swaps the innermost list's type
				std::string prefix = CloseLists(list.Depth);
				if (!list.ForcedType.empty() && list.Depth == OpenLists.size() && OpenLists.back() != list.ForcedType)
				{
					prefix += CloseLists(list.Depth - 1);
				}
				while (OpenLists.size() < list.Depth)
				{
					std::string type = list.ForcedType.empty() ? Options.ListType : list.ForcedType;
					prefix += "\\begin{" + type + "}";
					OpenLists.push_back(type);
				}
				// \begin/\end share the item's line, so the line count never changes
				return list.Indent + prefix + "\\item{} " + ProcessLine(list.Remainder);
			}
		}
	}

	Syntax::Result Processor::Run(std::string_view body)
	{
		bool enabled = true;
		size_t start = 0;
		while (start <= body.size())
		{
			/// SPLIT INTO LINES (KEEPING THE EXACT LINE STRUCTURE)
			auto end = body.find('\n', start);
			bool lastLine = (end == std::string_view::npos);
			std::string line(body.substr(start, lastLine ? std::string_view::npos : end - start));
			++LineNumber;

			/// MARKERS SWITCH THE SYNTAX OFF AND ON (an OFF ends any open lists first)
			bool turnsOn;
			if (IsMarker(line, turnsOn))
			{
				Result.Text += (!turnsOn ? CloseLists(0) : "") + line;
				enabled = turnsOn;
			}
			else if (!enabled)
			{
				Result.Text += line;
			}
			else if (Options.Extended && State == Maths::None)
			{
				std::string heading;
				Result.Text += Heading(line, heading) ? heading : ListLineOutput(line);
			}
			else
			{
				Result.Text += ProcessLine(line);
			}

			if (lastLine)
			{
				break;
			}
			Result.Text += '\n';
			start = end + 1;
		}

		/// TIDY UP AT THE END OF THE NOTE
		Result.Text += CloseLists(0); // on the last line, so the line count is unchanged
		if (Options.Extended && State != Maths::None)
		{
			LineNumber = MathsOpenedOn;
			Warn("maths opened here is never closed; nothing after it was formatted");
		}
		return Result;
	}
} // namespace

Syntax::Result Syntax::Process(std::string_view body, const Options &options)
{
	return Processor(options).Run(body);
}
