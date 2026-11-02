#include "Indent.h"
#include "Notes/Header.h"
#include "Notes/Syntax.h"
#include <algorithm>
#include <cctype>
#include <vector>

namespace
{
	//! The LaTeX sectioning commands, by heading level (1 = \section), as the compiler maps # headings to them
	constexpr std::string_view Sectioning[] = {"section", "subsection", "subsubsection", "paragraph", "subparagraph"};

	//! The heading level of a line starting with a sectioning command (\section, \subsection*, ...), or 0
	size_t CommandLevel(std::string_view line)
	{
		if (line.empty() || line[0] != '\\') return 0;
		for (size_t level = 1; level <= std::size(Sectioning); ++level)
		{
			auto name = Sectioning[level - 1];
			if (line.compare(1, name.size(), name) != 0) continue;
			auto after = 1 + name.size();
			if (after >= line.size() || !std::isalpha(static_cast<unsigned char>(line[after]))) return level;
		}
		return 0;
	}

	//! The name in a \begin{name} or \end{name} at line[i] (prefix is "\\begin{" or "\\end{"); empty if there isn't one
	std::string EnvironmentAt(std::string_view line, size_t i, std::string_view prefix)
	{
		if (line.compare(i, prefix.size(), prefix) != 0) return "";
		auto close = line.find('}', i + prefix.size());
		if (close == std::string_view::npos) return "";
		return std::string(line.substr(i + prefix.size(), close - i - prefix.size()));
	}

	//! Re-indents one section (the local preamble or the body), line by line, carrying the structure from line to line
	class Section
	{
	  public:
		Section(const Settings::Formatting &options, bool extended) : Options(options), Extended(extended)
		{
		}

		std::string Line(const std::string &line);

	  private:
		const Settings::Formatting &Options;
		bool Extended;

		std::vector<size_t> Headings;	 //!< the heading levels whose contents are indented, outermost first
		std::vector<std::string> Environments;
		int Braces = 0;
		bool DisplayMaths = false;
		std::string Verbatim; //!< the verbatim environment being copied, if any
		bool Off = false;	  //!< inside % cortex off

		bool InList = false;
		size_t ListIndent = 0; //!< the author's indentation of the line which opened the list
		size_t ItemDepth = 0;  //!< the stars of the current item

		//! The levels open before a line, without any it closes itself
		size_t Depth() const
		{
			size_t environments = Options.Environments ? Environments.size() : 0;
			return Headings.size() + environments + static_cast<size_t>(std::max(Braces, 0)) + (DisplayMaths ? 1 : 0);
		}

		std::string Indented(size_t depth, std::string_view text) const
		{
			std::string out;
			for (size_t i = 0; i < depth; ++i) out += Options.Indent;
			return out + std::string(text);
		}

		bool IsVerbatim(const std::string &name) const
		{
			return std::find(Options.Verbatim.begin(), Options.Verbatim.end(), name) != Options.Verbatim.end();
		}

		size_t LeadingClosers(std::string_view text) const;
		void Track(std::string_view text);
	};

	// How many of the levels open before the line it closes before anything else: a line starting "}", "\end{...}" or "\]" sits at the
	// level they close to
	size_t Section::LeadingClosers(std::string_view text) const
	{
		size_t closes = 0;
		size_t environments = Environments.size();
		int braces = Braces;
		size_t i = 0;
		while (i < text.size())
		{
			if (text[i] == ' ' || text[i] == '\t')
			{
				++i;
			}
			else if (text[i] == '}' && braces > 0 && Options.Braces)
			{
				--braces;
				++closes;
				++i;
			}
			else if (auto name = EnvironmentAt(text, i, "\\end{"); !name.empty() && environments > 0)
			{
				--environments;
				if (Options.Environments) ++closes;
				i += 6 + name.size();
			}
			else if (text.compare(i, 2, "\\]") == 0 && DisplayMaths)
			{
				++closes;
				i += 2;
			}
			else break;
		}
		return closes;
	}

	// Follows what a line opens and closes: environments, braces and display maths. Escaped characters and comments don't count
	void Section::Track(std::string_view text)
	{
		size_t i = 0;
		while (i < text.size())
		{
			char c = text[i];
			if (c == '%') return;
			if (c == '\\')
			{
				if (auto name = EnvironmentAt(text, i, "\\begin{"); !name.empty())
				{
					Environments.push_back(name);
					i += 7 + name.size() + 1;
					if (IsVerbatim(name))
					{
						Verbatim = name; // the rest of the line is the environment's: copied as written, like what follows
						return;
					}
					continue;
				}
				if (auto name = EnvironmentAt(text, i, "\\end{"); !name.empty())
				{
					// closes the innermost environment of that name (and any left open inside it)
					if (auto open = std::find(Environments.rbegin(), Environments.rend(), name); open != Environments.rend())
					{
						Environments.erase(std::next(open).base(), Environments.end());
					}
					i += 5 + name.size() + 1;
					continue;
				}
				if (text.compare(i, 2, "\\[") == 0) DisplayMaths = true;
				else if (text.compare(i, 2, "\\]") == 0) DisplayMaths = false;
				i += 2; // a backslash and the character after it go together, so \{ \} \% are never structure
				continue;
			}
			if (Options.Braces && c == '{') ++Braces;
			else if (Options.Braces && c == '}' && Braces > 0) --Braces;
			++i;
		}
	}

	std::string Section::Line(const std::string &line)
	{
		auto first = line.find_first_not_of(" \t");
		std::string_view text = first == std::string::npos ? std::string_view() : std::string_view(line).substr(first);
		while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
		size_t written = first == std::string::npos ? line.size() : first; // the author's indentation

		/// COPIED AS WRITTEN: cortex off regions (until the switch back on), verbatim environments (until their \end)
		bool turnsOn = false;
		if (Off)
		{
			if (!Notes::Syntax::IsSwitch(line, turnsOn) || !turnsOn) return line;
			Off = false;
			return Indented(Depth(), text);
		}
		if (!Verbatim.empty())
		{
			auto end = "\\end{" + Verbatim + "}";
			auto at = line.find(end);
			if (at == std::string::npos) return line;
			// the \end and anything after it are ordinary text again; text before it on the line is the environment's own
			if (line.find_first_not_of(" \t") != at) return line;
			Verbatim.clear();
			size_t depth = Depth() - LeadingClosers(text);
			Track(text);
			return Indented(depth, text);
		}

		/// BLANK LINES STAY BLANK, AND END ANY STAR LIST
		if (text.empty())
		{
			InList = false;
			return "";
		}
		if (Notes::Syntax::IsSwitch(line, turnsOn))
		{
			InList = false;
			if (!turnsOn) Off = true;
			return Indented(Depth(), text);
		}

		/// HEADINGS: CLOSE THOSE AT THEIR LEVEL OR DEEPER, AND (UP TO THE SETTING'S DEPTH) INDENT WHAT FOLLOWS
		size_t level = CommandLevel(text);
		if (!level && Extended)
		{
			if (auto heading = Notes::Syntax::HeadingOf(text)) level = std::min(heading->Level, std::size(Sectioning));
		}
		if (level)
		{
			InList = false;
			while (!Headings.empty() && Headings.back() >= level) Headings.pop_back();
			auto out = Indented(Depth(), text);
			if (level <= Options.Headings) Headings.push_back(level);
			Track(text);
			return out;
		}

		/// STAR LISTS: ITEMS BY THEIR STARS, CONTINUATIONS ONE DEEPER THAN THEIR ITEM
		size_t depth = Depth() - LeadingClosers(text);
		if (Extended)
		{
			auto list = Notes::Syntax::ClassifyList(line, InList, ListIndent);
			if (list.Type == Notes::Syntax::ListLine::Item)
			{
				if (!InList) ListIndent = written;
				InList = true;
				ItemDepth = list.Depth;
				depth += list.Depth - 1;
			}
			else if (InList && (list.Type == Notes::Syntax::ListLine::Neutral || list.Type == Notes::Syntax::ListLine::ManualItem))
			{
				depth += ItemDepth; // a continuation, a comment or an \item inside the list: the list stays open
			}
			else if (list.Type != Notes::Syntax::ListLine::Neutral)
			{
				InList = false;
			}
		}
		Track(text);
		return Indented(depth, text);
	}

	//! Splits text into lines, keeping each line's \r (if it has one) apart, so it can be put back
	struct Lines
	{
		std::vector<std::string> Text;
		std::vector<bool> CarriageReturn;
	};

	Lines SplitLines(std::string_view text)
	{
		Lines lines;
		size_t start = 0;
		while (true)
		{
			auto end = text.find('\n', start);
			std::string line(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
			bool cr = !line.empty() && line.back() == '\r';
			if (cr) line.pop_back();
			lines.Text.push_back(std::move(line));
			lines.CarriageReturn.push_back(cr);
			if (end == std::string_view::npos) break;
			start = end + 1;
		}
		return lines;
	}
} // namespace

std::string Formatter::Indent(std::string_view text, const Settings::Formatting &options, bool extended)
{
	auto lines = SplitLines(text);

	/// THE SECTIONS: THE FIRST TWO SEPARATORS ARE STRUCTURAL (DESIGN.md §3). With none, the whole file is a body
	std::vector<size_t> separators;
	for (size_t i = 0; i < lines.Text.size() && separators.size() < 2; ++i)
	{
		if (Notes::IsSeparator(lines.Text[i])) separators.push_back(i);
	}
	size_t preambleStart = separators.size() == 2 ? separators[0] + 1 : std::string::npos;
	size_t bodyStart = separators.empty() ? 0 : separators.back() + 1;

	std::string out;
	Section preamble(options, false);
	Section body(options, extended);
	for (size_t i = 0; i < lines.Text.size(); ++i)
	{
		auto &line = lines.Text[i];
		// the final newline ends the last line rather than starting another, so an empty last piece is left alone
		bool last = (i + 1 == lines.Text.size());
		if (last && line.empty() && !lines.CarriageReturn[i]) break;

		if (i >= bodyStart) out += body.Line(line);
		else if (preambleStart != std::string::npos && i >= preambleStart && i < separators[1]) out += preamble.Line(line);
		else out += line; // the header and the separators
		if (lines.CarriageReturn[i]) out += '\r';
		if (!last) out += '\n';
	}
	return out;
}
