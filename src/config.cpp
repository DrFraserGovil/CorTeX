#include "config.h"
#include "ProjectSettings.h"
#include "Settings.h"
#include <JSL.h>

void ConfigInterface()
{
	LOG(INFO) << JSL::Display::Yellow(true) << "\t-- Settings Interface --\t\t" << JSL::Display::ResetAll();
	auto out = JSL::Interface::Console(Project);

	out.Export(Settings.RootPath / ".cortex/settings.config", true);
	Project = out;
}
