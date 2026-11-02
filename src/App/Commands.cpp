#include "Commands.h"
#include "Session.h"
#include "Build/Pipeline.h"
#include "Format/Indent.h"
#include "Lsp/Server.h"
#include "Link/Link.h"
#include "Platform/Desktop.h"
#include "Platform/Process.h"
#include "Project/Lock.h"
#include "Prompt.h"
#include "Report/Summary.h"
#include "Settings/ProjectSettings.h"
#include "Util/Files.h"
#include <JSL.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace
{
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
		App::BuildOnce(root, clean);
		if (then) then();
		App::Serve(root, *lock, [&](bool c) { App::BuildOnce(root, c); }); // requests which arrived meanwhile, so none are lost
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
	enum class SettingsChange
	{
		Unchanged,
		Saved,
		//! saved, and the change recompiles every note, which the author has agreed to: the caller rebuilds from scratch
		SavedNeedsRebuild
	};

	//! The settings menu: edits the project's settings interactively, and saves them
	SettingsChange EditSettingsMenu(const std::filesystem::path &root)
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
			if (!App::Confirm("Save the changes and rebuild now?", false))
			{
				LOG(INFO) << "Changes discarded; the settings are unchanged";
				return SettingsChange::Unchanged;
			}
			settings.Export(file, true); // saved first: the rebuild must read the new settings
			return SettingsChange::SavedNeedsRebuild;
		}
		settings.Export(file, true);
		return SettingsChange::Saved;
	}
} // namespace

void App::Watch(const std::filesystem::path &root)
{
	LOG(INFO) << "Watching the project, and rebuilding whenever it changes. Type q and Enter (or run 'cortex shutdown') to stop, or settings "
				 "and Enter to change the project's settings";
	JSL::Async::Watcher::Panopticon eye;

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

	/// THE AUTHOR'S COMMANDS. The settings menu runs here, on the watch's own thread, with the input watcher paused so the menu gets
	/// every line typed; the lock stays held, and file changes and other commands' requests wait until the menu closes
	eye.SetInputCallback([&](std::string line) {
		auto command = JSL::String::trim(line);
		if (command == "q") eye.Stop();
		else if (command == "settings")
		{
			eye.PauseInput();
			auto change = SettingsChange::Unchanged;
			try
			{
				change = EditSettingsMenu(root);
			}
			catch (std::exception &e)
			{
				LOG(ERROR) << "Settings not changed: " << e.what();
			}
			eye.ResumeInput();
			if (change == SettingsChange::SavedNeedsRebuild) build(true);
			LOG(INFO) << "Still watching. Type q and Enter to stop, or settings and Enter to change the settings";
		}
		else if (!command.empty())
		{
			LOG(INFO) << "Type q and Enter to stop watching, or settings and Enter to change the project's settings";
		}
	});

	build(false); // bring the output up to date before waiting for changes
	eye.Start();
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
	if (EditSettingsMenu(root) == SettingsChange::SavedNeedsRebuild) Build(root, true); // or a watch holding the lock is asked to
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

void App::Format(const std::filesystem::path &file, const std::filesystem::path &root)
{
	/// THE SETTINGS: OF THE PROJECT GIVEN, ELSE THE ONE CONTAINING THE FILE (OR THE CURRENT FOLDER); THE DEFAULTS OUTSIDE ANY PROJECT
	auto project = !root.empty() ? std::filesystem::weakly_canonical(std::filesystem::absolute(root)) : Project::Paths::Find(file.empty() ? std::filesystem::current_path() : file);
	Settings::Config config;
	if (!project.empty() && Project::Paths::IsProject(project))
	{
		Report::Log problems;
		config = Settings::Config::Load(project, problems);
	}

	if (file.empty())
	{
		std::string text(std::istreambuf_iterator<char>(std::cin), {});
		std::cout << Formatter::Indent(text, config.Format, config.Compilation.Extended) << std::flush;
		return;
	}
	if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("no file at " + file.string());
	auto text = JSL::IO::getFile(file);
	auto formatted = Formatter::Indent(text, config.Format, config.Compilation.Extended);
	if (formatted == text)
	{
		LOG(INFO) << file.string() << " is already formatted";
		return;
	}
	Util::WriteAtomic(file, formatted);
	LOG(INFO) << "Formatted " << file.string();
}

int App::Lsp()
{
#ifdef _WIN32
	// the protocol counts bytes: no \r\n translation either way
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	// standard output belongs to the protocol: everything CorTeX would print (JSL's LOG writes to std::cout) goes to standard error,
	// which editors keep in their log. Only warnings and errors, so a long session doesn't fill it with build progress
	std::ostream protocol(std::cout.rdbuf());
	std::cout.rdbuf(std::cerr.rdbuf());
	JSL::Log::Global().SetLevel(WARN);
	int code = Lsp::Serve(std::cin, protocol);
	std::cout.rdbuf(protocol.rdbuf());
	return code;
}
