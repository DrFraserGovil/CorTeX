#include "BuildState.h"
#include "../Utility/Files.h"
#include <JSL.h>

static constexpr std::string_view Magic = "cortex-buildstate";

// Fields are tab-separated and records are newline-separated, so neither may appear inside a field
static std::string Clean(std::string_view field)
{
	std::string out(field);
	for (char &c : out)
	{
		if (c == '\t' || c == '\n' || c == '\r') c = ' ';
	}
	return out;
}

BuildState BuildState::Load(const std::filesystem::path &file)
{
	BuildState state;
	if (!std::filesystem::exists(file))
	{
		LOG(DEBUG) << "No build state found at " << file << "; every note will be compiled";
		return state;
	}

	auto lines = JSL::IO::getFileLines(file);
	if (lines.empty())
	{
		return state;
	}

	/// CHECK THE HEADER
	auto header = JSL::String::split(lines[0], "\t");
	if (header.size() < 3 || header[0] != Magic || header[1] != std::to_string(FormatVersion))
	{
		LOG(WARN) << "Ignoring build state in an unrecognised format; every note will be compiled";
		return state;
	}
	state.GlobalHash = header[2];

	/// READ ONE RECORD PER LINE
	for (size_t i = 1; i < lines.size(); ++i)
	{
		if (lines[i].empty()) continue;
		auto fields = JSL::String::split(lines[i], "\t");
		size_t dependencyCount = 0;
		bool wellFormed = fields.size() >= 6;
		if (wellFormed)
		{
			try
			{
				dependencyCount = std::stoul(fields[5]);
			}
			catch (std::exception &)
			{
				wellFormed = false;
			}
			wellFormed = wellFormed && fields.size() >= 6 + 2 * dependencyCount;
		}
		if (!wellFormed)
		{
			// a dropped record just means that note is compiled again
			LOG(WARN) << "Ignoring malformed build state line " << i + 1;
			continue;
		}

		NoteRecord record;
		record.Id = fields[0];
		record.Path = fields[1];
		record.Hash = fields[2];
		record.Succeeded = (fields[3] == "ok");
		record.HasOutput = (fields[4] == "1");
		for (size_t d = 0; d < dependencyCount; ++d)
		{
			record.Dependencies.push_back({fields[6 + 2 * d], fields[7 + 2 * d]});
		}
		size_t diagnosticField = 6 + 2 * dependencyCount;
		record.Diagnostic = fields.size() > diagnosticField ? fields[diagnosticField] : "";
		state.Records[record.Id] = record;
	}
	return state;
}

void BuildState::Save(const std::filesystem::path &file) const
{
	std::string out = std::string(Magic) + "\t" + std::to_string(FormatVersion) + "\t" + GlobalHash + "\n";
	for (auto &[id, r] : Records)
	{
		out += Clean(r.Id) + "\t" + Clean(r.Path) + "\t" + r.Hash + "\t" + (r.Succeeded ? "ok" : "failed") + "\t" + (r.HasOutput ? "1" : "0");
		out += "\t" + std::to_string(r.Dependencies.size());
		for (auto &[path, hash] : r.Dependencies)
		{
			out += "\t" + Clean(path) + "\t" + hash;
		}
		out += "\t" + Clean(r.Diagnostic) + "\n";
	}
	Utility::WriteAtomic(file, out);
}

void BuildState::RetainOnly(const std::set<std::string> &ids)
{
	std::erase_if(Records, [&](const auto &entry) { return !ids.contains(entry.first); });
}
