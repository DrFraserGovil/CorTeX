#include "Scan.h"
#include "Notes/Reader.h"
#include "Util/Files.h"
#include <JSL.h>
#include <map>

std::vector<std::filesystem::path> Build::Scan::Candidates(const Settings::Config &config)
{
	auto ignored = config.Ignored;
	ignored.push_back(".cortex");
	auto folder = JSL::IO::Directory::Snapshot(config.Root, JSL::IO::multiGlobToRegex(ignored));
	std::vector<std::filesystem::path> candidates;
	for (auto &file : folder.MatchFiles(JSL::IO::multiGlobToRegex(config.Allowed)))
	{
		if (!Util::IsInside(file, config.Output)) candidates.push_back(file);
	}
	return candidates;
}

Build::Scan Build::Scan::Read(const std::filesystem::path &root)
{
	Report::Log problems;
	auto config = Settings::Config::Load(root, problems);
	Scan scan{std::move(problems), config, Project::Paths(root, config.Output), Project::Registry::Read(root), std::make_unique<std::vector<Notes::Note>>(), {}};

	Notes::ReadOptions options{config.Compilation.Extended, config.Compilation.ListType};
	std::map<std::string, std::filesystem::path> seen;
	for (auto &file : Candidates(config))
	{
		auto note = Notes::Read(root, file, options, scan.Problems);
		if (!note) continue;
		if (auto clash = seen.find(note->Id); clash != seen.end())
		{
			throw std::runtime_error("'" + clash->second.generic_string() + "' and '" + note->Path.generic_string() + "' both have the id '" + note->Id +
									 "' (ids ignore case and punctuation); rename one of them");
		}
		seen[note->Id] = note->Path;
		scan.Notes->push_back(std::move(*note));
	}
	std::sort(scan.Notes->begin(), scan.Notes->end(), [](auto &a, auto &b) { return a.Id < b.Id; });
	scan.Net = Network::Resolve(*scan.Notes, scan.Registry.Sidebar, scan.Problems);
	return scan;
}
