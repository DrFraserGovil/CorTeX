#include "Link.h"
#include "Project.h"
#include "Settings/ArchiveSettings.h"
#include "Settings/RuntimeSettings.h"
#include "Setup.h"
#include <JSL.h>
RuntimeSettingsObject Runtime;
ArchiveSettings Archive;
enum class Mode
{
	Shutdown,
	Build,
	Rebuild,
	New,
	Settings,
	Watch,
	Link,
	InstallHandler,
};
void ConfigureLog()
{
	JSL::Log::Global().SetLevel(INFO);
	JSL::Log::Global().ShowHeaders = false;
	if (Runtime.Verbose)
	{
		JSL::Log::Global().ShowHeaders = true;
		JSL::Log::Global().SetLevel(DEBUG);
	}
	else if (Runtime.Quiet)
	{
		JSL::Log::Global().SetLevel(WARN);
	}
}

void Welcome()
{
	auto banner = std::string(24, '=');
	LOG(INFO) << banner << "\n  CorTeX Note System  \n"
			  << banner;
}

Mode ProcessCommands()
{
	LOG(DEBUG) << "Input commands detected: " << Runtime.Commands;
	if (Runtime.Commands.size() > 1)
	{
		LOG(ERROR) << "CORTEX ERROR\nMultiple commands detected: " << Runtime.Commands << "\nCorTeX accepts only one command at a time";
		throw std::runtime_error("Bad input");
	}

	if (Runtime.Commands.empty())
	{
		Runtime.Commands = {"build"};
		LOG(DEBUG) << "No commands inserted, so providing default: " << Runtime.Commands;
	}
	std::map<std::string, std::pair<std::string, Mode>> actions = {
		{"build", {"Rebuilding out-of-sync files", Mode::Build}},
		{"new", {"Initialising new CorTeX Archive", Mode::New}},
		{"rebuild", {"Rebuilding entire archive from source", Mode::Rebuild}},
		{"settings", {"Updating the settings of an archive", Mode::Settings}},
		{"shutdown", {"Locating running process to terminate", Mode::Shutdown}},
		{"watch", {"Initialising watcher process", Mode::Watch}},
		{"link", {"Following a link from the viewer", Mode::Link}},
		{"install-handler", {"Registering the cortex:// link handler", Mode::InstallHandler}},

	};

	std::string cmd = JSL::String::getLower(Runtime.Commands[0]);
	if (actions.contains(cmd))
	{
		auto [msg, mode] = actions[cmd];
		LOG(INFO) << msg;
		return mode;
	}
	else
	{
		LOG(ERROR) << "CORTEX ERROR\nUnrecognised command '" << Runtime.Commands[0] << "'. Use -h to see the available options";
		throw std::runtime_error("Bad input");
	}
}

bool RootIsCortex()
{
	auto path = Runtime.RootPath / ".cortex/registry.ctx";
	auto settings = Runtime.RootPath / ".cortex/settings.config";
	return std::filesystem::exists(path) && std::filesystem::exists(settings);
}

void ValidateCortexDirectory(Mode mode)
{
	// these name their project themselves (a link carries its root), or need none
	if (mode == Mode::Link || mode == Mode::InstallHandler)
	{
		return;
	}
	bool isCortex = RootIsCortex();

	if (mode == Mode::New)
	{
		if (isCortex)
		{
			LOG(ERROR) << "A cortex archive already exists at the specified location; cannot overwrite";
			throw std::runtime_error("Bad directory");
		}
		else
		{
			LOG(DEBUG) << "No cortex detected, archive initialisation can begin";
		}
	}
	else
	{
		if (!isCortex)
		{
			LOG(ERROR) << "No valid CorTeX registry detected. \nMove to a valid directory, or create a new registry with 'cortex new'";
			throw std::runtime_error("Bad directory");
		}
		else
		{
			LOG(DEBUG) << "Located registry file in" << Runtime.RootPath;
			LOG(INFO) << "Loading project settings";
			Archive.Configure(Runtime.RootPath / ".cortex/settings.config", " ");
		}
	}
}

int main(int argc, char **argv)
{
	ConfigureLog(); // do it first so configured of defaults in case an error encountered during processing

	try
	{
		Runtime.Initialise(argc, argv);
		ConfigureLog();
		Welcome();
		auto indent = JSL::Log::Indent();
		auto mode = ProcessCommands();
		ValidateCortexDirectory(mode);

		switch (mode)
		{
			case Mode::New:
				CreateArchive();
				break;
			case Mode::Settings:
				ConfigInterface();
				break;
			case Mode::Build:
				CortexProject(Runtime.RootPath).Build();
				break;
			case Mode::Shutdown:
				CortexProject(Runtime.RootPath).Shutdown();
				break;
			case Mode::Watch:
				CortexProject(Runtime.RootPath).Watch();
				break;
			case Mode::Rebuild:
				CortexProject(Runtime.RootPath).CleanRebuild();
				break;
			case Mode::Link:
				Link::Handle(Runtime.Url);
				break;
			case Mode::InstallHandler:
				Link::InstallHandler();
				break;
			default:
				throw std::runtime_error("End of the line");
		}
	}
	catch (std::exception &e)
	{
		LOG(ERROR) << JSL::Display::Red() << "Encountered error: " << e.what() << "\nCannot recover; exiting process";
		exit(1);
	}
	catch (...)
	{
		LOG(ERROR) << "Cannot recover from error, exiting with error code 1";
		exit(1);
	}

	LOG(INFO)
		<< "CorTeX operations complete, exiting";
	return (0);
}
