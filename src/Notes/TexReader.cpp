#include "TexReader.h"
#include "Syntax.h"
#include "Util/Text.h"
#include <JSL.h>

std::optional<Notes::Note> Notes::TexReader::Read(const std::filesystem::path &root, const std::filesystem::path &source, const ReadOptions &options,
												  Report::Log &report) const
{
	Note note;
	note.Source = source;
	note.Path = source.lexically_relative(root);
	note.Format = JSL::String::getLower(source.extension().string().substr(1));
	auto file = note.Path.generic_string();

	auto sections = Split(JSL::IO::getFile(source));
	std::vector<HeaderProblem> problems;
	if (sections.HasHeader) note.Fields = Header::Parse(sections.Header, problems);
	// a file without a title can't be linked to or listed, so it isn't a note: usually an \input fragment, so only counted
	if (note.Fields.Title.empty())
	{
		report.Add(Report::Kind::Skipped, "skipped: it has no title (add a header with a 'title:' line, followed by a line of five dashes)", file);
		return std::nullopt;
	}
	for (auto &problem : problems) report.Add(Report::Kind::Note, problem.Message, file, problem.Line);

	note.LocalPreamble = sections.LocalPreamble;
	note.Body = sections.Body;
	note.LocalPreambleLine = sections.LocalPreambleLine;
	note.BodyLine = sections.BodyLine;
	note.Id = IdOf(note.Path);
	note.Key = Util::HashHex(note.Id);

	/// A STUB HAS NO CONTENT OF ITS OWN
	// anything written in its body is ignored (and said so, so it isn't silently lost); its references don't count
	if (note.IsStub())
	{
		for (auto &line : JSL::String::split(note.Body, "\n"))
		{
			auto trimmed = JSL::String::trim(line);
			if (!trimmed.empty() && !trimmed.starts_with("%"))
			{
				report.Add(Report::Kind::Redirect, "redirects to '" + note.Fields.Redirect + "', so the text in its body is ignored", file, note.BodyLine);
				break;
			}
		}
		return note;
	}

	/// APPLY THE SYNTAX
	// purely local to this note: what the references point at is resolved later, by the Network
	auto processed = Syntax::Process(note.Body, {options.Extended, options.ListType, note.Key});
	note.Processed = std::move(processed.Text);
	note.References = std::move(processed.References);
	note.Labels = std::move(processed.Labels);
	for (auto &warning : processed.Warnings) report.Add(Report::Kind::Syntax, warning.Message, file, note.BodyLine + warning.Line - 1);
	return note;
}
