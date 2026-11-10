#include "Features.h"
#include "Format/Indent.h"
#include "Notes/Header.h"
#include "Notes/Syntax.h"
#include <JSL.h>
#include <regex>
#include <set>

namespace
{
	using Lsp::Json;
	using Syntax = Notes::Syntax::LinkSpan;

	/// LINES AND POSITIONS

	std::vector<std::string> Lines(std::string_view text)
	{
		std::vector<std::string> lines;
		size_t start = 0;
		while (true)
		{
			auto end = text.find('\n', start);
			std::string line(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
			if (!line.empty() && line.back() == '\r') line.pop_back();
			lines.push_back(std::move(line));
			if (end == std::string_view::npos) return lines;
			start = end + 1;
		}
	}

	//! The 0-based line of a note's file holding a line of its body (1-based within the body)
	size_t FileLine(const Notes::Note &note, size_t bodyLine)
	{
		return note.BodyLine + (bodyLine > 0 ? bodyLine - 1 : 0) - 1;
	}

	//! A line of a note's body (1-based) as last read
	std::string BodyLine(const Notes::Note &note, size_t line)
	{
		auto lines = Lines(note.Body);
		return line >= 1 && line <= lines.size() ? lines[line - 1] : "";
	}

	Json Point(size_t line, size_t character)
	{
		return {{"line", line}, {"character", character}};
	}

	//! A range within one line, from byte offsets in it
	Json Range(const std::string &text, size_t line, size_t from, size_t to, Lsp::Encoding encoding)
	{
		return {{"start", Point(line, Lsp::CharacterOf(text, from, encoding))}, {"end", Point(line, Lsp::CharacterOf(text, to, encoding))}};
	}

	Json Location(const std::filesystem::path &file, Json range)
	{
		return {{"uri", Lsp::UriOf(file)}, {"range", std::move(range)}};
	}

	/// THE PROJECT

	std::filesystem::path Relative(const Build::Scan &scan, const std::filesystem::path &file)
	{
		return file.lexically_normal().lexically_relative(scan.Paths.Root);
	}

	const Notes::Note *NoteOf(const Build::Scan &scan, const std::string &id)
	{
		auto node = scan.Net.Find(id);
		return node ? node->Source : nullptr;
	}

	//! What a target written in the note at from resolves to: the note it finally reaches (through any redirects), and the note it
	//! names directly (a stub, if it is one); both empty if it matches nothing
	struct Resolved
	{
		std::string Final;
		std::string Direct;
	};

	Resolved Resolve(const Build::Scan &scan, const std::string &slug, const std::filesystem::path &from)
	{
		Resolved out;
		out.Direct = scan.Net.Resolve(slug, from);
		if (!out.Direct.empty()) out.Final = scan.Net.Follow(out.Direct).value_or(out.Direct);
		return out;
	}

	//! The note owning a label reference's label: for [[#label]] the nearest note with it, else the target's (after redirects)
	std::string LabelOwner(const Build::Scan &scan, const Notes::Syntax::LinkParts &parts, const std::filesystem::path &from)
	{
		if (parts.Target.empty()) return scan.Net.LabelOwner(parts.Label, from);
		return Resolve(scan, Notes::Syntax::Slug(parts.Target), from).Final;
	}

	//! The [[...]] the cursor is in (closed ones only, unless open is allowed), with the line and the cursor's byte offset
	struct AtCursor
	{
		std::string Line;
		size_t Column = 0;
		std::optional<Syntax> Span;
	};

	AtCursor Find(const Lsp::Document &document, Lsp::Position position, Lsp::Encoding encoding, bool allowOpen)
	{
		AtCursor at;
		auto lines = Lines(document.Text);
		if (position.Line >= lines.size()) return at;
		at.Line = lines[position.Line];
		at.Column = Lsp::ByteOf(at.Line, position.Character, encoding);
		at.Span = Notes::Syntax::LinkAt(at.Line, at.Column);
		if (at.Span && ((!allowOpen && at.Span->Close == std::string::npos) || !at.Span->Parts.Valid)) at.Span.reset();
		return at;
	}

	/// COMPLETION

	Json NameItems(const Build::Scan &scan, Json range)
	{
		Json items = Json::array();
		for (auto &note : *scan.Notes)
		{
			auto path = note.Path.generic_string();
			bool title = true;
			for (auto &name : note.Names())
			{
				Json item = {{"label", name},
							 {"kind", title ? 17 : 18}, // a file; a reference (an alias)
							 {"detail", path},
							 {"filterText", name},
							 {"textEdit", {{"range", range}, {"newText", name}}}};
				if (!title) item["labelDetails"] = {{"description", "alias of " + note.Fields.Title}};
				else if (note.IsStub()) item["labelDetails"] = {{"description", "redirects to " + note.Fields.Redirect}};
				items.push_back(std::move(item));
				title = false;
			}
		}
		return items;
	}

	Json LabelItems(const std::vector<const Notes::Note *> &notes, Json range)
	{
		Json items = Json::array();
		for (auto *note : notes)
		{
			std::set<std::string> seen;
			for (auto &label : note->Labels)
			{
				if (!seen.insert(label.Name).second) continue;
				items.push_back({{"label", label.Name},
								 {"kind", 21}, // a constant
								 {"detail", note->Fields.Title},
								 {"filterText", label.Name},
								 {"textEdit", {{"range", range}, {"newText", label.Name}}}});
			}
		}
		return items;
	}

	//! Completion on a header line naming notes: "redirect: <name>", "sidebar: <name>|Label, [Heading], <name>", "copy-sidebar: <name>, <name>"
	Json HeaderCompletion(const Build::Scan &scan, const std::string &line, size_t lineNumber, size_t column, Lsp::Encoding encoding)
	{
		auto colon = line.find(':');
		if (colon == std::string::npos || column <= colon) return Json::array();
		auto key = JSL::String::getLower(JSL::String::trim(std::string_view(line).substr(0, colon)));
		size_t start = colon + 1, end = line.size();
		if (key == "sidebar" || key == "copy-sidebar")
		{
			auto comma = line.rfind(',', column - 1);
			if (comma != std::string::npos && comma > colon) start = comma + 1;
			auto first = line.find_first_not_of(" \t", start);
			if (first != std::string::npos && first < column && line[first] == '[') return Json::array(); // a [Heading], which is free text
			auto bar = key == "sidebar" ? line.find('|', start) : std::string::npos;
			if (bar != std::string::npos && bar < column) return Json::array(); // in a label, which is free text
			end = std::min(line.find(',', column), bar);
			if (end == std::string::npos) end = line.size();
		}
		else if (key != "redirect")
		{
			return Json::array();
		}
		while (start < column && (line[start] == ' ' || line[start] == '\t')) ++start;
		while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t')) --end;
		end = std::max(end, column);
		return NameItems(scan, Range(line, lineNumber, start, end, encoding));
	}
} // namespace

Json Lsp::Complete(const Build::Scan &scan, const Document &document, Position position, Encoding encoding)
{
	auto at = Find(document, position, encoding, true);
	auto lines = Lines(document.Text);
	if (position.Line >= lines.size()) return Json::array();

	/// IN THE HEADER (BEFORE THE FIRST SEPARATOR)
	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (!Notes::IsSeparator(lines[i])) continue;
		if (position.Line < i) return HeaderCompletion(scan, at.Line, position.Line, at.Column, encoding);
		break;
	}

	/// IN A [[...]]: A NAME, OR A LABEL AFTER THE #; NOTHING IN THE |TEXT, WHICH IS FREE
	if (!at.Span) return Json::array();
	auto &span = *at.Span;
	if (at.Column < span.TargetColumn) return Json::array();
	if (span.TextColumn != std::string::npos && at.Column >= span.TextColumn) return Json::array();
	bool open = span.Close == std::string::npos;
	auto from = Relative(scan, document.File);

	if (span.LabelColumn != std::string::npos && at.Column >= span.LabelColumn)
	{
		size_t end = open ? at.Column : (span.TextColumn != std::string::npos ? span.TextColumn - 1 : span.Close);
		auto range = Range(at.Line, position.Line, span.LabelColumn, std::max(end, at.Column), encoding);
		std::vector<const Notes::Note *> notes;
		if (span.Parts.Target.empty())
		{
			for (auto &note : *scan.Notes) notes.push_back(&note);
		}
		else if (auto *note = NoteOf(scan, Resolve(scan, Notes::Syntax::Slug(span.Parts.Target), from).Final))
		{
			notes.push_back(note);
		}
		return LabelItems(notes, range);
	}

	size_t end = open ? at.Column : span.Close;
	if (span.LabelColumn != std::string::npos) end = span.LabelColumn - 1;
	else if (span.TextColumn != std::string::npos) end = span.TextColumn - 1;
	return NameItems(scan, Range(at.Line, position.Line, span.TargetColumn, std::max(end, at.Column), encoding));
}

Json Lsp::Definition(const Build::Scan &scan, const Document &document, Position position, Encoding encoding)
{
	auto at = Find(document, position, encoding, false);
	if (!at.Span) return nullptr;
	auto &parts = at.Span->Parts;
	auto from = Relative(scan, document.File);

	/// A LABEL REFERENCE: THE \label IN ITS OWNER
	if (at.Span->LabelColumn != std::string::npos && !parts.Transclusion)
	{
		auto *owner = NoteOf(scan, LabelOwner(scan, parts, from));
		if (!owner) return nullptr;
		for (auto &label : owner->Labels)
		{
			if (label.Name != parts.Label) continue;
			auto text = BodyLine(*owner, label.Line);
			auto column = text.find("\\label{" + label.Name + "}");
			if (column == std::string::npos) column = 0;
			return Location(owner->Source, Range(text, FileLine(*owner, label.Line), column, column, encoding));
		}
		return Location(owner->Source, Range("", 0, 0, 0, encoding));
	}

	/// A LINK OR TRANSCLUSION: THE NOTE IT FINALLY REACHES
	auto *note = NoteOf(scan, Resolve(scan, Notes::Syntax::Slug(parts.Target), from).Final);
	if (!note) return nullptr;
	return Location(note->Source, Range("", 0, 0, 0, encoding));
}

Json Lsp::Hover(const Build::Scan &scan, const Document &document, Position position, Encoding encoding)
{
	auto at = Find(document, position, encoding, false);
	if (!at.Span) return nullptr;
	auto &span = *at.Span;
	auto &parts = span.Parts;
	auto from = Relative(scan, document.File);

	std::vector<std::string> kinds;
	if (parts.Transclusion) kinds.push_back("transclusion");
	if (parts.Loose) kinds.push_back("loose");
	if (parts.Strong) kinds.push_back("strong");
	std::string text;

	auto describe = [&](const Notes::Note &note) {
		std::string out = "**" + note.Fields.Title + "**  \n`" + note.Path.generic_string() + "`";
		if (!note.Fields.Aliases.empty()) out += "  \nalso: " + JSL::String::stitch(note.Fields.Aliases, 0, note.Fields.Aliases.size(), ", ");
		if (note.Fields.Todo) out += "  \nneeds work" + (note.Fields.Todo->empty() ? std::string() : ": " + *note.Fields.Todo);
		return out;
	};

	if (span.LabelColumn != std::string::npos && !parts.Transclusion)
	{
		auto *owner = NoteOf(scan, LabelOwner(scan, parts, from));
		bool found = owner && std::any_of(owner->Labels.begin(), owner->Labels.end(), [&](auto &l) { return l.Name == parts.Label; });
		if (found) text = "Label `" + parts.Label + "` in " + describe(*owner);
		else if (owner) text = "**" + owner->Fields.Title + "** has no label `" + parts.Label + "`";
		else text = parts.Target.empty() ? "No note has the label `" + parts.Label + "`" : "No note called **" + parts.Target + "** yet: a wanted note";
	}
	else
	{
		auto resolved = Resolve(scan, Notes::Syntax::Slug(parts.Target), from);
		auto *note = NoteOf(scan, resolved.Final);
		if (!note)
		{
			auto *stub = NoteOf(scan, resolved.Direct);
			text = stub ? "**" + stub->Fields.Title + "** redirects to '" + stub->Fields.Redirect + "', which matches no note (or loops)"
						: "No note called **" + parts.Target + "** yet: a wanted note";
		}
		else
		{
			text = describe(*note);
			if (resolved.Direct != resolved.Final)
			{
				if (auto *stub = NoteOf(scan, resolved.Direct)) text += "  \nreached through the redirect `" + stub->Path.generic_string() + "`";
			}
		}
	}
	if (!kinds.empty()) text = "*" + JSL::String::stitch(kinds, 0, kinds.size(), ", ") + "*  \n" + text;
	return {{"contents", {{"kind", "markdown"}, {"value", text}}}, {"range", Range(at.Line, position.Line, span.Open, span.Close + 2, encoding)}};
}

Json Lsp::References(const Build::Scan &scan, const Document &document, Position position, Encoding encoding)
{
	auto at = Find(document, position, encoding, false);
	auto from = Relative(scan, document.File);

	/// THE NOTE WHOSE REFERENCES ARE WANTED: THE ONE THE CURSOR'S [[...]] POINTS TO, ELSE THE DOCUMENT'S OWN
	std::string target;
	if (at.Span)
	{
		auto &parts = at.Span->Parts;
		target = (at.Span->LabelColumn != std::string::npos && !parts.Transclusion) ? LabelOwner(scan, parts, from)
																					: Resolve(scan, Notes::Syntax::Slug(parts.Target), from).Final;
	}
	else
	{
		for (auto &note : *scan.Notes)
		{
			if (note.Path == from) target = note.Id;
		}
	}
	Json locations = Json::array();
	if (target.empty()) return locations;

	/// EVERY REFERENCE IN THE PROJECT WHICH REACHES IT
	for (auto &note : *scan.Notes)
	{
		for (auto &reference : note.References)
		{
			std::string reaches;
			if (reference.Type == Notes::Reference::Kind::Label && reference.Slug.empty()) reaches = scan.Net.LabelOwner(reference.Label, note.Path);
			else reaches = Resolve(scan, reference.Slug, note.Path).Final;
			if (reaches != target) continue;
			auto text = BodyLine(note, reference.Line);
			auto close = text.find("]]", reference.Column);
			locations.push_back(Location(note.Source, Range(text, FileLine(note, reference.Line), reference.Column, close == std::string::npos ? text.size() : close + 2, encoding)));
		}
	}
	return locations;
}

Json Lsp::Formatting(const Build::Scan &scan, const Document &document, Encoding encoding)
{
	auto formatted = Formatter::Indent(document.Text, scan.Config.Format, scan.Config.Compilation.Extended);
	if (formatted == document.Text) return Json::array();
	auto lines = Lines(document.Text);
	size_t last = lines.size() - 1;
	Json range = {{"start", Point(0, 0)}, {"end", Point(last, CharacterOf(lines[last], lines[last].size(), encoding))}};
	return Json::array({{{"range", range}, {"newText", formatted}}});
}

namespace
{
	//! The modifiers, as bits in the order TokenLegend lists them
	enum Modifier
	{
		Resolved = 1 << 0,
		Unresolved = 1 << 1,
		Transclusion = 1 << 2,
		LabelReference = 1 << 3,
		Loose = 1 << 4,
		Strong = 1 << 5,
	};
} // namespace

Json Lsp::TokenLegend()
{
	return {{"tokenTypes", {"link"}}, {"tokenModifiers", {"resolved", "unresolved", "transclusion", "label", "loose", "strong"}}};
}

Json Lsp::SemanticTokens(const Build::Scan &scan, const Document &document, Encoding encoding)
{
	auto lines = Lines(document.Text);
	auto from = Relative(scan, document.File);

	/// THE BODY: AFTER THE LAST STRUCTURAL SEPARATOR (THE FIRST TWO), OR THE WHOLE FILE IF THERE ARE NONE
	size_t body = 0, separators = 0;
	for (size_t i = 0; i < lines.size() && separators < 2; ++i)
	{
		if (!Notes::IsSeparator(lines[i])) continue;
		++separators;
		body = i + 1;
	}

	Json data = Json::array();
	size_t previousLine = 0, previousStart = 0;
	bool off = false;
	for (size_t row = body; row < lines.size(); ++row)
	{
		auto &line = lines[row];
		bool turnsOn = false;
		if (Notes::Syntax::IsSwitch(line, turnsOn))
		{
			off = !turnsOn;
			continue;
		}
		if (off) continue;

		for (auto &link : Notes::Syntax::LinksIn(line))
		{
			if (link.Close == std::string::npos) continue; // still being typed
			auto &parts = link.Parts;
			// the coloured part: the target and any #label, up to the |text or the ]]
			size_t start = link.TargetColumn, end = link.TextColumn != std::string::npos ? link.TextColumn - 1 : link.Close;
			if (end <= start) continue;

			int modifiers = 0;
			bool labelled = link.LabelColumn != std::string::npos;
			if (parts.Transclusion) modifiers |= Transclusion;
			if (labelled) modifiers |= LabelReference;
			if (parts.Loose) modifiers |= Loose;
			if (parts.Strong) modifiers |= Strong;

			bool resolved = false;
			if (parts.Valid && !(parts.Transclusion && labelled))
			{
				if (labelled)
				{
					auto *owner = NoteOf(scan, LabelOwner(scan, parts, from));
					resolved = !parts.Label.empty() && owner && std::any_of(owner->Labels.begin(), owner->Labels.end(), [&](auto &l) { return l.Name == parts.Label; });
				}
				else if (!parts.Target.empty())
				{
					resolved = !Resolve(scan, Notes::Syntax::Slug(parts.Target), from).Final.empty();
				}
			}
			modifiers |= resolved ? Resolved : Unresolved;

			// each token relative to the previous one: line delta, start (relative if on the same line), length, type, modifiers
			auto first = CharacterOf(line, start, encoding);
			auto length = CharacterOf(line, end, encoding) - first;
			data.push_back(row - previousLine);
			data.push_back(row == previousLine ? first - previousStart : first);
			data.push_back(length);
			data.push_back(0);
			data.push_back(modifiers);
			previousLine = row;
			previousStart = first;
		}
	}
	return {{"data", data}};
}

std::map<std::string, Json> Lsp::Diagnostics(const Report::Log &report, const std::filesystem::path &root)
{
	std::map<std::string, Json> out;
	for (auto &entry : report.Entries())
	{
		if (entry.File.empty() || entry.Type == Report::Kind::Skipped) continue;
		auto file = entry.File;
		auto message = entry.Message;
		size_t line = entry.Line > 0 ? static_cast<size_t>(entry.Line - 1) : 0;

		// a failure's diagnosis starts with where LaTeX stopped ("file:line: message"), which may be a note transcluded into this one
		static const std::regex located(R"(^([^:]+):(\d+): (.*)$)");
		std::smatch match;
		if (entry.Line == 0 && std::regex_match(entry.Message, match, located) && std::filesystem::is_regular_file(root / match[1].str()))
		{
			file = match[1].str();
			line = std::max<size_t>(std::stoul(match[2].str()), 1) - 1;
			message = match[3].str();
			if (file != entry.File) message += " (while compiling " + entry.File.generic_string() + ")";
		}

		auto &list = out[UriOf((root / file).lexically_normal())];
		if (list.is_null()) list = Json::array();
		list.push_back({{"range", {{"start", Point(line, 0)}, {"end", Point(line + 1, 0)}}},
						{"severity", entry.Type == Report::Kind::Failure ? 1 : 2},
						{"source", "cortex"},
						{"message", message}});
	}
	return out;
}
