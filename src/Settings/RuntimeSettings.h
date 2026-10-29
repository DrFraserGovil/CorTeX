#pragma once
#include <JSL/Interface.h>

//! @name Settings
//! @command build A one-shot build command that builds any out-of-date files
//! @command rebuild Force a rebuild of the entire project
//! @command new Begins the process of opening a new CorTeX archive
//! @command shutdown Sends a shutdown message to a running CorTeX instance
//! @command settings Open the settings-modification interface
//! @command watch Begins a persistent process which watches the current archive for changes to the source file
//! @command link Carries out a cortex:// link from the viewer (given with -url): opens a note in an editor, or flags it as needing work
//! @command install-handler Registers this cortex as the desktop's handler for cortex:// links, so the viewer's Edit and todo buttons work
class RuntimeSettingsObject: public JSL::Interface::Aggregator<RuntimeSettingsObject>
{
  public:
	//! @alias v verbose
	//! @brief If true, recieve DEBUG level logs to the output stream. Takes priority over Quiet mode.
	bool Verbose = false;

	//! @alias q quiet
	//! @brief If true, recieve only ERROR logs to the output stream; everything else is suppressed unless Verbose is also set to true
	bool Quiet = false;

	//! @alias root dir
	//! @brief The directory to target as the root (if none provided, uses the current working directory)
	std::filesystem::path RootPath;

	//! @alias url
	//! @brief The cortex:// link to carry out (with the link command)
	std::string Url = "";

	void Initialise(int argc, char **argv)
	{
		Parse(argc, argv);
		if (RootPath.empty())
		{
			RootPath = std::filesystem::current_path();
		}
		RootPath = std::filesystem::weakly_canonical(RootPath);
	}
	#include "RuntimeSettings.RuntimeSettingsObject.autogen"
};

extern RuntimeSettingsObject Runtime;
