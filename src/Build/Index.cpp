#include "Index.h"
#include "../Utility/Files.h"

static std::string StatusName(Index::Status status)
{
	switch (status)
	{
		case Index::Status::Ok:
			return "ok";
		case Index::Status::Stale:
			return "stale";
		default:
			return "failed";
	}
}

Index::Index(const std::vector<Note> &notes, const BuildState &state)
{
	for (auto &note : notes)
	{
		Entry entry{note.Id, note.Title, note.RelativePath.generic_string(), Status::Failed, "not yet built"};

		// the status comes from the last time the note was compiled, whether that was this run or an earlier one
		auto record = state.Records.find(note.Id);
		if (record != state.Records.end())
		{
			auto &r = record->second;
			entry.State = r.Succeeded ? Status::Ok : (r.HasOutput ? Status::Stale : Status::Failed);
			entry.Diagnostic = r.Diagnostic;
		}
		List.push_back(entry);
	}
}

const std::vector<Index::Entry> &Index::Entries() const
{
	return List;
}

std::string Index::Serialise() const
{
	using Utility::JsonString;
	std::string js = "CortexIndex([\n";
	for (size_t i = 0; i < List.size(); ++i)
	{
		auto &e = List[i];
		js += (i > 0 ? ",\n" : "");
		js += "{\"id\":" + JsonString(e.Id) + ",\"title\":" + JsonString(e.Title) + ",\"path\":" + JsonString(e.Path);
		js += ",\"status\":" + JsonString(StatusName(e.State)) + ",\"diagnostic\":" + JsonString(e.Diagnostic) + "}";
	}
	return js + "\n]);\n";
}
