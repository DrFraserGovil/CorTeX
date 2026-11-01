#include "Commands.h"
#include "Build/Pipeline.h"
#include "Link/Link.h"
#include "Platform/Desktop.h"
#include "Platform/Process.h"
#include "Project/Lock.h"
#include "Prompt.h"
#include "Report/Summary.h"
#include "Settings/ProjectSettings.h"
#include "Util/Files.h"
#include <JSL.h>

namespace
{
	//! One build (or rebuild), told in full to the build log and briefly to the terminal
	//! @throws runtime_error If the build couldn't happen at all (missing programs, clashing ids, every note failing); the log is written first
	void BuildOnce(const std::filesystem::path &root, bool clean)
	{
		LOG(INFO) << JSL::Display::Purple(true) << (clean ? "Rebuilding the whole project" : "Building") << JSL::Display::ResetAll();
		Report::Log report;
		auto logFile = root / ".cortex/build.log";
		try
		{
			auto scan = Build::Scan::Read(root);
			report.Absorb(std::move(scan.Problems));
			if (clean) Build::Rebuild(scan, report);
			else Build::Update(scan, report);
			Report::WriteLog(logFile, report, clean ? "rebuild" : "build");
			if (auto brief = Report::Brief(report, ".cortex/build.log"); !brief.empty())
			{
				LOG(WARN) << brief;
			}
			for (auto &line : report.AccountLines())
			{
				if (line.find("flagged todo") != std::string::npos || line.find("exist yet") != std::string::npos)
				{
					LOG(INFO) << line;
				}
			}
			LOG(INFO) << "Output written to " << (scan.Config.Compilation.Pdf ? scan.Paths.Output : scan.Paths.Output / "index.html");
		}
		catch (std::exception &e)
		{
			report.Account(std::string("Stopped: ") + e.what());
			Report::WriteLog(logFile, report, clean ? "rebuild" : "build");
			throw;
		}
	}

	//! Copies the built output into folder, as a copy to share: no path on this machine, no editor tools, no live refresh (DESIGN.md §13.3)
	void ExportTo(const std::filesystem::path &root, const std::filesystem::path &folder)
	{
		auto scan = Build::Scan::Read(root);
		auto &paths = scan.Paths;

		/// CHECK THE DESTINATION
		auto target = std::filesystem::weakly_canonical(std::filesystem::absolute(folder));
		if (target == root || target == paths.Output || Util::IsInside(target, paths.Output) || target == paths.Cortex || Util::IsInside(target, paths.Cortex) ||
			Util::IsInside(root, target))
		{
			throw std::runtime_error("can't export into the project root, its output folder or .cortex; choose a folder of its own");
		}
		auto marker = target / ".cortex-export";
		if (std::filesystem::exists(target))
		{
			if (!std::filesystem::is_directory(target)) throw std::runtime_error(target.string() + " is a file, not a folder");
			if (!std::filesystem::is_empty(target) && !std::filesystem::exists(marker))
			{
				throw std::runtime_error(target.string() + " already has files in it which aren't an earlier export; not touching them");
			}
			// an earlier export: replaced wholesale, so notes deleted since don't linger in it
			for (auto &entry : std::filesystem::directory_iterator(target)) std::filesystem::remove_all(entry.path());
		}

		/// COPY THE OUTPUT, THEN REWRITE ITS INDEX WITHOUT ANYTHING TIED TO THIS MACHINE
		std::filesystem::create_directories(target);
		std::filesystem::copy(paths.Output, target, std::filesystem::copy_options::recursive);
		std::filesystem::remove(target / "version.js"); // live refresh is for the author's own copy
		if (!scan.Config.Compilation.Pdf)
		{
			Report::Log ignored; // the build just reported everything
			Util::WriteAtomic(target / "index.js", Build::IndexJs(scan, Build::BuildState::Load(paths.State), true, ignored));
		}
		JSL::IO::writeString(marker, "Exported by CorTeX. This file lets the next export replace this folder\n");
		LOG(INFO) << "Exported a copy to share to " << target;
	}

	//! Does what other commands asked of the lock's holder; returns false once one asked it to stop
	bool Serve(const std::filesystem::path &root, Project::Lock &lock, const std::function<void(bool)> &build)
	{
		while (!lock.ShutdownRequested())
		{
			if (lock.TakeRebuild()) build(true);
			else if (lock.TakeRerun()) build(false);
			auto exports = lock.TakeExports();
			for (auto &folder : exports)
			{
				try
				{
					build(false);
					ExportTo(root, folder);
				}
				catch (std::exception &e)
				{
					LOG(ERROR) << "Export to " << folder << " failed: " << e.what();
				}
			}
			if (exports.empty()) return true; // nothing more was asked during the work just done
		}
		LOG(INFO) << "Shutdown requested; stopping";
		return false;
	}

	//! Takes the lock, builds, then does whatever else is asked (then) before letting go; if another process holds the lock, it's sent
	//! message instead
	void BuildHoldingLock(const std::filesystem::path &root, bool clean, const std::string &message, const std::string &handedOver,
						  const std::function<void()> &then = {})
	{
		std::optional<Project::Lock> lock;
		if (!Project::Lock::TakeOrSend(root, lock, message))
		{
			LOG(INFO) << handedOver;
			return;
		}
		BuildOnce(root, clean);
		if (then) then();
		Serve(root, *lock, [&](bool c) { BuildOnce(root, c); }); // requests which arrived meanwhile, so none are lost
	}

	// The output and .cortex are written by the build itself (watching them would rebuild forever); other hidden files are editor or
	// version control noise. The project's own .cortex files are the exception
	bool AffectsBuild(const std::filesystem::path &root, const std::filesystem::path &output, const std::filesystem::path &changed)
	{
		auto file = changed.is_absolute() ? changed : root / changed;
		auto relative = file.lexically_relative(root).generic_string();
		if (relative == ".cortex/preamble.tex" || relative == ".cortex/settings.config" || relative == ".cortex/registry.ctx") return true;
		if (file == output || Util::IsInside(file, output)) return false;
		for (auto &part : file.lexically_relative(root))
		{
			if (part.string().starts_with(".")) return false;
		}
		return true;
	}
} // namespace

void App::Build(const std::filesystem::path &root, bool clean)
{
	BuildHoldingLock(root, clean, clean ? "rebuild" : "rerun",
					 std::string("Another CorTeX process (a watch or build) is working on this project; it has been asked to ") + (clean ? "rebuild from scratch" : "rebuild"));
}

void App::Export(const std::filesystem::path &root, const std::filesystem::path &folder)
{
	// sent as an absolute path: a running watch has its own working directory
	auto target = std::filesystem::weakly_canonical(std::filesystem::absolute(folder));
	BuildHoldingLock(root, false, "export " + target.string(),
					 "A CorTeX watch (or build) is running on this project; it will export to " + target.string() + " once its build is up to date",
					 [&] { ExportTo(root, target); });
}

void App::Shutdown(const std::filesystem::path &root)
{
	if (Project::Lock::Send(root, "shutdown")) LOG(INFO) << "Asked the running CorTeX process to stop (a watch stops at once; a build stops after its current build)";
	else LOG(INFO) << "No CorTeX process is running on this project";
}

namespace
{
	//! One round of watching: builds, then rebuilds on every change, until it's stopped
	//! @returns true if it stopped because the author asked to edit the settings (so watching should resume afterwards)
	bool WatchRound(const std::filesystem::path &root)
	{
		JSL::Async::Watcher::Panopticon eye;
		bool editSettings = false;

		// one build, which never takes the watch down with it: problems are reported, and the next change tries again. Settings are read
		// afresh by every build: another command may have just saved new ones, and its message can arrive before the file event
		auto build = [&](bool clean) {
			try
			{
				BuildOnce(root, clean);
			}
			catch (std::exception &e)
			{
				LOG(ERROR) << "Build failed: " << e.what() << "\nStill watching for changes";
			}
		};

		/// THE LOCK, AND MESSAGES FROM OTHER CORTEX COMMANDS: THEY ARRIVE ON THIS THREAD, BETWEEN BUILDS
		std::optional<Project::Lock> lock;
		try
		{
			lock.emplace(root, eye, [&]() {
				if (!Serve(root, *lock, build)) eye.Stop();
			});
		}
		catch (std::exception &)
		{
			throw std::runtime_error("another CorTeX process (a build or watch) is already working on this project");
		}

		/// FILE CHANGES: CALLBACKS RUN ON THIS THREAD, ONE BATCH AT A TIME, SO CHANGES MADE DURING A BUILD QUEUE UP FOR THE NEXT ONE
		Report::Log ignored;
		auto output = Settings::Config::Load(root, ignored).Output;
		eye.SetFileBatchCallback(root, true, [&](std::set<JSL::Async::Watcher::FileChange> batch) {
			for (auto &change : batch)
			{
				if (!AffectsBuild(root, output, change.Path)) continue;
				LOG(INFO) << "Change detected: " << change.Path.lexically_relative(root).generic_string();
				build(false);
				output = Settings::Config::Load(root, ignored).Output; // the output setting may have been what changed
				return; // one build covers the whole batch
			}
		});
		eye.SetDebounceTime(300); // editors often save as several quick writes: one build per save
		eye.SetInputCallback([&](std::string line) {
			auto command = JSL::String::trim(line);
			if (command == "q") eye.Stop();
			else if (command == "settings")
			{
				editSettings = true;
				eye.Stop();
			}
			else if (!command.empty())
			{
				LOG(INFO) << "Type q and Enter to stop watching, or settings and Enter to change the project's settings";
			}
		});

		build(false); // bring the output up to date before waiting for changes (including any made while the settings were edited)
		eye.Start();
		return editSettings;
	}
} // namespace

void App::Watch(const std::filesystem::path &root)
{
	LOG(INFO) << "Watching the project, and rebuilding whenever it changes. Type q and Enter (or run 'cortex shutdown') to stop, or settings "
				 "and Enter to change the project's settings";
	// the settings are edited between rounds of watching, with nothing else reading the terminal (the menu needs every line typed), and
	// with the lock released, so that a change which recompiles every note can rebuild just as 'cortex settings' would
	while (WatchRound(root))
	{
		LOG(INFO) << "Watching paused while the settings are edited";
		try
		{
			EditSettings(root);
		}
		catch (std::exception &e)
		{
			LOG(ERROR) << "Settings not changed: " << e.what();
		}
		LOG(INFO) << "Watching again. Type q and Enter to stop, or settings and Enter to change the settings";
	}
	LOG(INFO) << "Stopped watching";
}

void App::New(const std::filesystem::path &root)
{
	if (Project::Paths::IsProject(root)) throw std::runtime_error("a CorTeX project already exists at " + root.string() + "; not overwriting it");
	Project::Paths paths(root, root / "view");
	LOG(INFO) << "Creating a CorTeX project in " << root << ". Values shown [like this] are defaults, used if you give no answer";
	auto name = Prompt("Project name", root.filename().string());

	auto keep = [](const std::filesystem::path &file, const std::string &what) {
		if (!std::filesystem::exists(file)) return false;
		LOG(WARN) << "A " << what << " file already exists at " << file;
		return !Confirm("Overwrite it?", false);
	};
	bool keepSettings = keep(paths.Settings, "settings");
	bool keepPreamble = keep(paths.Preamble, "preamble");
	bool edit = Confirm("Change the default settings?", false);

	std::filesystem::create_directories(paths.Cortex);
	if (!keepPreamble) JSL::IO::writeString(paths.Preamble, "% Everything here is shared by every note in the project\n");
	if (!keepSettings)
	{
		ProjectSettings settings;
		settings.Reset();
		settings.Export(paths.Settings, true);
	}
	if (edit) EditSettings(root);
	// written last: a folder is only a project once both files exist, so an interrupted setup leaves no half-made project
	JSL::IO::writeString(paths.Registry, "name = " + name + "\n\n"
										 "% The sidebar shown on every page of the viewer: one entry per line after \"sidebar:\", each a note's title or alias\n"
										 "% (optionally \"target|Label\"). A line \"[Heading]\" starts a new group. Notes can add their own with a sidebar: header line.\n"
										 "sidebar:\n");
	LOG(INFO) << "Created the project " << name;
}

void App::EditSettings(const std::filesystem::path &root)
{
	auto file = root / ".cortex/settings.config";
	ProjectSettings settings;
	settings.Reset();
	if (std::filesystem::exists(file)) settings.Configure(file, " ");
	Report::Log ignored;
	auto before = Settings::Config::From(root, settings, ignored).Compilation.Key();
	settings = JSL::Interface::Console(settings);

	/// CHANGES WHICH RECOMPILE EVERY NOTE ARE ONLY SAVED WITH A REBUILD (once a project has been built: a new one has nothing to rebuild)
	bool built = std::filesystem::exists(root / ".cortex/buildstate.tsv");
	if (built && Settings::Config::From(root, settings, ignored).Compilation.Key() != before)
	{
		LOG(WARN) << "These changes affect every note, so the whole project must be rebuilt from scratch";
		if (!Confirm("Save the changes and rebuild now?", false))
		{
			LOG(INFO) << "Changes discarded; the settings are unchanged";
			return;
		}
		// saved first: whoever does the rebuild (this process, or a watch holding the lock) must read the new settings
		settings.Export(file, true);
		Build(root, true);
		return;
	}
	settings.Export(file, true);
}

void App::Link(const std::string &url)
{
	// the change shows once the project is rebuilt (by a running watch, if there is one), and an open viewer then refreshes
	if (auto changed = Link::Handle(url)) Build(*changed, false);
}

void App::InstallHandler()
{
	Platform::Desktop::InstallHandler(Platform::Executable());
}
