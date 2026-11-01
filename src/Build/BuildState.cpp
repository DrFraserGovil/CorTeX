#include "BuildState.h"
#include "Util/Files.h"
#include <JSL.h>

namespace
{
	constexpr std::string_view Magic = "cortex-buildstate";

	// Fields are tab-separated and records are newline-separated, so neither may appear inside a field
	std::string Clean(std::string_view field)
	{
		std::string out(field);
		for (char &c : out)
		{
			if (c == '\t' || c == '\n' || c == '\r') c = ' ';
		}
		return out;
	}

	// Reads a count at fields[at], then that many pairs after it; false if the line is too short or the count isn't a number
	bool Pairs(const std::vector<std::string> &fields, size_t &at, std::vector<std::pair<std::string, std::string>> &out)
	{
		if (at >= fields.size()) return false;
		size_t count;
		try
		{
			count = std::stoul(fields[at]);
		}
		catch (std::exception &)
		{
			return false;
		}
		if (fields.size() < at + 1 + 2 * count) return false;
		for (size_t i = 0; i < count; ++i) out.push_back({fields[at + 1 + 2 * i], fields[at + 2 + 2 * i]});
		at += 1 + 2 * count;
		return true;
	}
} // namespace

Build::BuildState Build::BuildState::Load(const std::filesystem::path &file)
{
	BuildState state;
	if (!std::filesystem::exists(file)) return state;
	auto lines = JSL::IO::getFileLines(file);
	if (lines.empty()) return state;

	/// CHECK THE HEADER (an old or unknown format is discarded: every note is compiled again)
	auto header = JSL::String::split(lines[0], "\t");
	if (header.size() < 3 || header[0] != Magic || header[1] != std::to_string(FormatVersion)) return state;
	state.GlobalHash = header[2];

	/// READ ONE RECORD PER LINE (a malformed one is dropped, which only means that note is compiled again)
	for (size_t i = 1; i < lines.size(); ++i)
	{
		if (lines[i].empty()) continue;
		auto fields = JSL::String::split(lines[i], "\t");
		if (fields.size() < 4) continue;
		Record record;
		record.Id = fields[0];
		record.Hash = fields[1];
		record.Succeeded = (fields[2] == "ok");
		record.HasOutput = (fields[3] == "1");
		size_t at = 4;
		std::vector<std::pair<std::string, std::string>> labels;
		if (!Pairs(fields, at, record.Dependencies) || !Pairs(fields, at, labels)) continue;
		record.Labels.insert(labels.begin(), labels.end());
		record.Diagnostic = at < fields.size() ? fields[at] : "";
		state.Records[record.Id] = record;
	}
	return state;
}

void Build::BuildState::Save(const std::filesystem::path &file) const
{
	std::string out = std::string(Magic) + "\t" + std::to_string(FormatVersion) + "\t" + GlobalHash + "\n";
	for (auto &[id, r] : Records)
	{
		out += Clean(r.Id) + "\t" + r.Hash + "\t" + (r.Succeeded ? "ok" : "failed") + "\t" + (r.HasOutput ? "1" : "0");
		out += "\t" + std::to_string(r.Dependencies.size());
		for (auto &[path, hash] : r.Dependencies) out += "\t" + Clean(path) + "\t" + hash;
		out += "\t" + std::to_string(r.Labels.size());
		for (auto &[label, text] : r.Labels) out += "\t" + Clean(label) + "\t" + Clean(text);
		out += "\t" + Clean(r.Diagnostic) + "\n";
	}
	Util::WriteAtomic(file, out);
}

void Build::BuildState::RetainOnly(const std::set<std::string> &ids)
{
	std::erase_if(Records, [&](const auto &entry) { return !ids.contains(entry.first); });
}
