#include "Report.h"
#include <algorithm>

std::string Report::Describe(Kind kind)
{
	switch (kind)
	{
		case Kind::Failure:
			return "failed note";
		case Kind::Setting:
			return "invalid setting";
		case Kind::Skipped:
			return "skipped file";
		case Kind::SharedName:
			return "shared name";
		case Kind::Redirect:
			return "broken redirect";
		case Kind::Sidebar:
			return "broken sidebar entry";
		case Kind::Syntax:
			return "syntax problem";
		case Kind::Transclusion:
			return "transclusion problem";
		case Kind::Label:
			return "label problem";
		case Kind::Note:
			return "warning";
	}
	return "warning";
}

void Report::Log::Add(Kind kind, std::string message, std::filesystem::path file, int line)
{
	All.push_back({kind, std::move(message), std::move(file), line});
}

size_t Report::Log::Count(Kind kind) const
{
	return std::count_if(All.begin(), All.end(), [&](auto &e) { return e.Type == kind; });
}

std::vector<Report::Entry> Report::Log::Of(Kind kind) const
{
	std::vector<Entry> out;
	std::copy_if(All.begin(), All.end(), std::back_inserter(out), [&](auto &e) { return e.Type == kind; });
	return out;
}

void Report::Log::Absorb(Log &&other)
{
	All.insert(All.end(), std::make_move_iterator(other.All.begin()), std::make_move_iterator(other.All.end()));
	Lines.insert(Lines.end(), std::make_move_iterator(other.Lines.begin()), std::make_move_iterator(other.Lines.end()));
	other.All.clear();
	other.Lines.clear();
}

std::string Report::Format(const Entry &entry)
{
	if (entry.File.empty()) return entry.Message;
	return entry.File.generic_string() + (entry.Line > 0 ? ":" + std::to_string(entry.Line) : "") + ": " + entry.Message;
}
