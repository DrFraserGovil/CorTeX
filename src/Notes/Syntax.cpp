#include "Syntax.h"
#include "Util/Text.h"
#include <JSL.h>
#include <cctype>
#include <optional>
#include <regex>

std::string Notes::Syntax::Slug(std::string_view text)
{
	auto sanitised = Util::Sanitise(JSL::String::trim(text), "-_", true);
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
	//! The verbatim environment (one of names) a line leaves open: one begun outside a comment and not ended later on the line; empty if none
	std::string VerbatimOpenedIn(std::string_view line, const std::vector<std::string> &names)
	{
		std::string open;
		for (size_t i = 0; i < line.size(); ++i)
		{
			if (open.empty() && line[i] == '%') break; // a comment (inside the environment, % is just text)
			if (line[i] != '\\') continue;
			if (open.empty())
			{
				for (auto &name : names)
				{
					auto begin = "\\begin{" + name + "}";
					if (line.compare(i, begin.size(), begin) == 0)
					{
						open = name;
						i += begin.size() - 1;
						break;
					}
				}
				if (!open.empty()) continue;
			}
			else if (auto end = "\\end{" + open + "}"; line.compare(i, end.size(), end) == 0)
			{
				open.clear();
				i += end.size() - 1;
				continue;
			}
			++i; // a backslash and the character after it go together
		}
		return open;
	}

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

	//! The name in a \label{...} starting at line[i], or nullopt if there isn't one there (an unclosed one, or an empty name)
	std::optional<std::string> LabelAt(const std::string &line, size_t i)
	{
		if (line.compare(i, 7, "\\label{") != 0) return std::nullopt;
		auto close = line.find('}', i + 7);
		if (close == std::string::npos || close == i + 7) return std::nullopt;
		return line.substr(i + 7, close - i - 7);
	}

	//! The labels on a line the syntax is switched off for: every \label outside a comment
	std::vector<std::string> LabelsIn(const std::string &line)
	{
		std::vector<std::string> labels;
		for (size_t i = 0; i < line.size(); ++i)
		{
			if (line[i] == '%') break;
			if (line[i] != '\\') continue;
			if (auto label = LabelAt(line, i)) labels.push_back(*label);
			++i; // a backslash and the character after it go together, so \% isn't a comment
		}
		return labels;
	}

	//! The sectioning commands for heading levels 1, 2, ...: everything the standard classes provide
	constexpr std::string_view Headings[] = {"section", "subsection", "subsubsection", "paragraph", "subparagraph"};

	//! The scan of one note body: state that carries from line to line, plus the output
	class Processor
	{
	  public:
		Processor(const Notes::Syntax::Options &options) : Options(options)
		{
		}

		Notes::Syntax::Result Run(std::string_view body);

	  private:
		const Notes::Syntax::Options &Options;
		Notes::Syntax::Result Result;
		Maths State = Maths::None;
		size_t MathsOpenedOn = 0;
		//! With State == Maths::Environment: the environment's name (so only its own \end closes it)
		std::string MathsEnvironment;
		std::vector<std::string> OpenLists;
		//! The indentation (in characters) of the line which opened the outermost open list: only lines indented further continue an item
		size_t ListIndent = 0;
		//! The verbatim environment being copied, if any
		std::string Verbatim;
		//! The line being processed as the author wrote it, and where in it to look for the next reference (for its column)
		std::string CurrentLine;
		size_t SearchFrom = 0;
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

	// Handles "[[" at line[i], advancing i past what it consumed
	// Markers at the start of the brackets give the kind and strength: ! a transclusion, ~ loose, + strong. They can come in any order,
	// but each at most once, and never both ~ and +
	std::string Processor::Link(const std::string &line, size_t &i)
	{
		auto open = i + 2;
		auto close = line.find("]]", open);
		if (close == std::string::npos)
		{
			Warn("'[[' with no closing ']]' on the same line; left as written (to write a literal [[, use \\[[ or [ [)");
			i = open;
			return "[[";
		}
		auto written = line.substr(i, close + 2 - i);
		auto inner = line.substr(open, close - open);
		i = close + 2;

		/// READ THE MARKERS, THEN SPLIT INTO TARGET, LABEL AND TEXT
		auto parts = Notes::Syntax::ParseInner(inner);
		if (!parts.Valid)
		{
			Warn("link " + written + " has repeated or contradictory markers (! transcludes, ~ is loose, + is strong; each at most once, and not both ~ and +); left as written");
			return written;
		}
		bool transclusion = parts.Transclusion;
		Notes::Reference reference;
		reference.Marked = parts.Loose ? Notes::Strength::Loose : parts.Strong ? Notes::Strength::Strong : Notes::Strength::Default;
		reference.Line = LineNumber;
		// the column in the author's line: the text is copied verbatim, so it can be found there
		if (auto at = CurrentLine.find(written, SearchFrom); at != std::string::npos)
		{
			reference.Column = at;
			SearchFrom = at + written.size();
		}
		std::string target = parts.Target;
		std::string text = parts.Text;
		bool labelled = parts.LabelColumn != std::string::npos;
		reference.Label = parts.Label;
		reference.Target = target;
		reference.Slug = Notes::Syntax::Slug(target);

		/// TRANSCLUSION: OF WHOLE NOTES ONLY (any |text is meaningless here, and ignored)
		if (transclusion)
		{
			if (labelled)
			{
				Warn("transclusion " + written + " names a label, but only whole notes can be transcluded; left as written");
				return written;
			}
			if (reference.Slug.empty())
			{
				Warn("transclusion " + written + " has no target; left as written");
				return written;
			}
			reference.Type = Notes::Reference::Kind::Transclusion;
			Result.References.push_back(reference);
			return "\\CortexTransclude{" + Options.Origin + "}{" + reference.Slug + "}";
		}

		/// LABEL REFERENCES
		// the text, when given, is ordinary text, so it gets the same treatment (formatting, maths) as the rest of the line
		if (labelled)
		{
			if (reference.Label.empty() || (!target.empty() && reference.Slug.empty()))
			{
				Warn("reference " + written + " has no " + (reference.Label.empty() ? "label" : "target") + "; left as written");
				return written;
			}
			reference.Type = Notes::Reference::Kind::Label;
			Result.References.push_back(reference);
			return "\\CortexRef{" + Options.Origin + "}{" + reference.Slug + "}{" + reference.Label + "}{" + (text.empty() ? "" : ProcessLine(text)) + "}";
		}

		/// LINKS
		if (reference.Slug.empty())
		{
			Warn("link " + written + " has no target; left as written");
			return written;
		}
		Result.References.push_back(reference);
		return "\\CortexLink{" + Options.Origin + "}{" + reference.Slug + "}{" + ProcessLine(parts.TextColumn == std::string::npos ? target : text) + "}";
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

			/// CONTROL SEQUENCES: LEFT ALONE, BUT SOME OPEN OR CLOSE MATHS, AND LABELS ARE RECORDED
			if (c == '\\')
			{
				if (auto label = LabelAt(line, i)) Result.Labels.push_back({*label, LineNumber});
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
				else if (State == Maths::Environment && line.compare(i, 6 + MathsEnvironment.size(), "\\end{" + MathsEnvironment + "}") == 0)
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
			// the wiki-style ![[x]] is not a transclusion here (every marker goes inside the brackets): it's a '!' and then a link,
			// which is almost certainly a mistake, so it's pointed out
			if (line.compare(i, 3, "![[") == 0)
			{
				Warn("'![[...]]' is not a transclusion: write the ! inside the brackets, as [[!...]]. Treated as a '!' followed by a link");
				out += '!';
				++i;
				continue;
			}
			if (line.compare(i, 2, "[[") == 0)
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
		auto heading = Notes::Syntax::HeadingOf(line);
		if (!heading) return false;
		size_t start = line.find_first_not_of(" \t");
		size_t level = heading->Level;
		bool starred = heading->Starred;

		constexpr size_t deepest = std::size(Headings);
		if (level > deepest)
		{
			Warn("headings go " + std::to_string(deepest) + " levels deep (#####, \\subparagraph); this one is treated as that level");
			level = deepest;
		}
		std::string title = JSL::String::trim(line.substr(heading->TitleColumn));
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
		auto list = Notes::Syntax::ClassifyList(line, !OpenLists.empty(), ListIndent);
		using ListLine = Notes::Syntax::ListLine;
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
				if (OpenLists.empty()) ListIndent = list.Indent.size();
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

	Notes::Syntax::Result Processor::Run(std::string_view body)
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
			CurrentLine = line;
			SearchFrom = 0;

			/// MARKERS SWITCH THE SYNTAX OFF AND ON (an OFF ends any open lists first)
			bool turnsOn;
			if (!Verbatim.empty())
			{
				// copied as written, lists left as they are, until the line with the environment's \end
				Result.Text += line;
				if (line.find("\\end{" + Verbatim + "}") != std::string::npos) Verbatim.clear();
			}
			else if (Notes::Syntax::IsSwitch(line, turnsOn))
			{
				Result.Text += (!turnsOn ? CloseLists(0) : "") + line;
				enabled = turnsOn;
			}
			else if (!enabled)
			{
				for (auto &label : LabelsIn(line)) Result.Labels.push_back({label, LineNumber});
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
			if (enabled && Verbatim.empty() && !Notes::Syntax::IsSwitch(line, turnsOn)) Verbatim = VerbatimOpenedIn(line, Options.Verbatim);

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

Notes::Syntax::Result Notes::Syntax::Process(std::string_view body, const Options &options)
{
	return Processor(options).Run(body);
}

bool Notes::Syntax::IsSwitch(std::string_view line, bool &turnsOn)
{
	static const std::regex marker(R"(^\s*%+\s*cortex\s+(on|off)\s*$)", std::regex::icase);
	std::match_results<std::string_view::const_iterator> match;
	if (!std::regex_match(line.begin(), line.end(), match, marker)) return false;
	turnsOn = JSL::String::iEquals(match[1].str(), "on");
	return true;
}

std::optional<Notes::Syntax::HeadingLine> Notes::Syntax::HeadingOf(std::string_view line)
{
	auto start = line.find_first_not_of(" \t");
	if (start == std::string_view::npos || line[start] != '#') return std::nullopt;
	size_t pos = start;
	while (pos < line.size() && line[pos] == '#')
	{
		++pos;
	}
	HeadingLine heading;
	heading.Level = pos - start;
	heading.Starred = (pos < line.size() && line[pos] == '*');
	if (heading.Starred) ++pos;
	if (pos >= line.size() || line[pos] != ' ') return std::nullopt; // "#hashtag", "##" alone: not headings
	heading.TitleColumn = pos + 1;
	return heading;
}

Notes::Syntax::ListLine Notes::Syntax::ClassifyList(const std::string &line, bool inList, size_t listIndent)
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

	// a line indented further than the list continues its current item
	out.Type = (inList && start > listIndent) ? ListLine::Neutral : ListLine::Other;
	return out;
}

Notes::Syntax::LinkParts Notes::Syntax::ParseInner(std::string_view inner)
{
	LinkParts parts;
	for (; parts.Markers < inner.size() && (inner[parts.Markers] == '!' || inner[parts.Markers] == '~' || inner[parts.Markers] == '+'); ++parts.Markers)
	{
		char m = inner[parts.Markers];
		bool &flag = m == '!' ? parts.Transclusion : m == '~' ? parts.Loose : parts.Strong;
		if (flag) parts.Valid = false;
		flag = true;
	}
	if (parts.Loose && parts.Strong) parts.Valid = false;

	auto bar = inner.find('|', parts.Markers);
	auto targetPart = inner.substr(parts.Markers, bar == std::string_view::npos ? std::string_view::npos : bar - parts.Markers);
	if (bar != std::string_view::npos)
	{
		parts.TextColumn = bar + 1;
		parts.Text = std::string(inner.substr(bar + 1));
	}
	auto hash = targetPart.find('#');
	if (hash != std::string_view::npos)
	{
		parts.LabelColumn = parts.Markers + hash + 1;
		parts.Label = JSL::String::trim(targetPart.substr(hash + 1));
		targetPart = targetPart.substr(0, hash);
	}
	parts.Target = JSL::String::trim(targetPart);
	return parts;
}

std::vector<Notes::Syntax::LinkSpan> Notes::Syntax::LinksIn(std::string_view line)
{
	std::vector<LinkSpan> links;
	size_t i = 0;
	while (i < line.size())
	{
		if (line[i] == '\\')
		{
			i += 2; // a backslash and the character after it go together: \[[ is never a link, \% never a comment
			continue;
		}
		if (line[i] == '%') break;
		if (line.compare(i, 2, "[[") != 0)
		{
			++i;
			continue;
		}

		LinkSpan span;
		span.Open = i;
		auto close = line.find("]]", i + 2);
		span.Close = close;
		auto inner = line.substr(i + 2, close == std::string_view::npos ? std::string_view::npos : close - i - 2);
		span.Parts = ParseInner(inner);
		span.TargetColumn = i + 2 + span.Parts.Markers;
		if (span.Parts.LabelColumn != std::string::npos) span.LabelColumn = i + 2 + span.Parts.LabelColumn;
		if (span.Parts.TextColumn != std::string::npos) span.TextColumn = i + 2 + span.Parts.TextColumn;
		links.push_back(std::move(span));
		if (close == std::string_view::npos) break; // unclosed: the rest of the line is its own
		i = close + 2;
	}
	return links;
}

std::optional<Notes::Syntax::LinkSpan> Notes::Syntax::LinkAt(std::string_view line, size_t column)
{
	for (auto &link : LinksIn(line))
	{
		if (column < link.Open) return std::nullopt;
		if (link.Close == std::string::npos || column <= link.Close + 1) return link;
	}
	return std::nullopt;
}
