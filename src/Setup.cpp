#include "Setup.h"
#include "JSL.h"
#include "Settings/ArchiveSettings.h"
#include "Settings/RuntimeSettings.h"

void ConfigInterface()
{
	LOG(INFO) << JSL::Display::Yellow(true) << "\t-- Settings Interface --\t\t" << JSL::Display::ResetAll();
	auto out = JSL::Interface::Console(Archive);

	out.Export(Runtime.RootPath / ".cortex/settings.config", true);
	Archive = out;
}

auto prompt = JSL::Display::Colour(100, 100, 100);
// Prints a question (showing the default, if any) and reads one line from std::cin.
// Empty input returns the default. If the input stream closes (EOF, or stdin piped/redirected),
// the default is used if there is one; otherwise this throws, since there's no sensible answer.
std::string Prompt(const std::string &question, const std::string &defaultValue = "")
{
	std::cout << JSL::Display::Green() << question << ":";
	if (!defaultValue.empty())
	{
		std::cout << prompt << " [" << defaultValue << "] ";
	}
	std::cout << JSL::Display::Cyan() << std::flush;

	std::string response;
	if (!std::getline(std::cin, response))
	{
		std::cout << "\n"; // keep subsequent output off the prompt line
		if (defaultValue.empty())
		{
			LOG(ERROR) << "Input closed before an answer to '" << question << "' was given";
			throw std::runtime_error("No input");
		}
		return defaultValue;
	}
	std::cout << JSL::Display::ResetAll() << std::flush;
	// trim, including \r so piped Windows line endings don't leak into answers
	auto first = response.find_first_not_of(" \t\r");
	if (first == std::string::npos)
	{
		return defaultValue;
	}
	auto last = response.find_last_not_of(" \t\r");
	return response.substr(first, last - first + 1);
}
bool Confirm(const std::string &question, bool defaultValue)
{
	while (true)
	{
		auto response = Prompt(question, defaultValue ? "yes" : "no");
		if (JSL::String::iEquals(response, "y") || JSL::String::iEquals(response, "yes"))
		{
			return true;
		}
		if (JSL::String::iEquals(response, "n") || JSL::String::iEquals(response, "no"))
		{
			return false;
		}
		std::cout << "Please answer yes or no\n";
	}
}

const std::filesystem::path cortexDir = ".cortex";
const std::filesystem::path preambleFile = cortexDir / "preamble.tex";
const std::filesystem::path registryFile = cortexDir / "registry.ctx";
const std::filesystem::path settingsFile = cortexDir / "settings.config";

void PrepareCortex(bool writeSettings, bool writePreamble)
{
	JSL::IO::mkdir(Runtime.RootPath / cortexDir, JSL::IO::Policy::Generous);

	if (writePreamble)
	{
		JSL::IO::writeString(Runtime.RootPath / preambleFile, "%% ALL DECLARATIONS HERE ARE SHARED WITH ALL FILES IN THE ARCHIVE\n");
	}

	if (writeSettings)
	{
		Archive.Reset();
		Archive.Export(Runtime.RootPath / settingsFile, true);
	}
	else
	{
		Archive.Configure(Runtime.RootPath / settingsFile, " ");
	}

	auto reg = Runtime.RootPath / registryFile;
	JSL::IO::remove(reg, JSL::IO::Policy::Generous); // if the registry file exists, remove it here so that configuration can't complete it is re-written. Prevents half-formed files
}

void FinaliseCortex(std::string name)
{
	LOG(INFO) << JSL::Display::Green() << "Finalising CorTeX Registration";
	JSL::IO::writeString(Runtime.RootPath / registryFile, "name = " + name + "\n");
}

bool overwriteCheck(std::string name, std::filesystem::path path)
{
	bool fileExists = std::filesystem::exists(path);
	if (fileExists)
	{
		LOG(WARN) << "A " << name << " file already exists at " << path.string();
		return Confirm("Overwrite?", false);
	}
	return true;
}

void CreateArchive()
{
	LOG(INFO) << JSL::Display::Green(true) << "\t-- Creation Interface --\t\t" << JSL::Display::ResetAll();
	LOG(INFO) << "You will now be prompted for the setup information.\nValues presented " << prompt << "[like this]" << JSL::Display::ResetAll() << " are defaults, and will be used if no value is given";
	LOG(INFO)
		<< "What will the new CorTeX project be called?";
	auto name = Prompt("Archive Name", "My Project");

	bool writeSettings = overwriteCheck("Settings", Runtime.RootPath / ".cortex/settings.config");
	bool writePreamble = overwriteCheck("Preamble", Runtime.RootPath / ".cortex/preamble.tex");

	LOG(INFO) << "Do you want to change the default settings?";
	bool conf = Confirm("Change Settings?", false);

	PrepareCortex(writeSettings, writePreamble);

	if (conf)
	{
		ConfigInterface();
	}
	FinaliseCortex(name);
}
