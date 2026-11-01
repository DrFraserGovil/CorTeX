#pragma once
#include "Network/Network.h"
#include "Notes/Note.h"
#include "Project/Paths.h"
#include "Project/Registry.h"
#include "Report/Report.h"
#include "Settings/Config.h"
#include <memory>
#include <vector>

namespace Build
{
	//! One reading of a project: its settings, every note, and how they connect. Everything a build or the link handler needs to know
	//! before touching any output. Problems found reading it are in Problems
	struct Scan
	{
		Report::Log Problems;
		Settings::Config Config;
		Project::Paths Paths;
		Project::Registry Registry;
		//! Sorted by id. Held by pointer, so the Network's pointers into it survive the Scan being moved
		std::unique_ptr<std::vector<Notes::Note>> Notes;
		Network::Network Net;

		//! Reads the project at root (absolute)
		//! @throws runtime_error If two files give the same id (DESIGN.md §2.3), which stops everything
		static Scan Read(const std::filesystem::path &root);

		//! The candidate note files (DESIGN.md §2.2): matching an allowed pattern and no ignored one, outside .cortex and the output folder
		static std::vector<std::filesystem::path> Candidates(const Settings::Config &config);
	};
} // namespace Build
