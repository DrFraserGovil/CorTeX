#include "Setup.h"
#include "JSL.h"
#include "Settings/ArchiveSettings.h"
#include "Settings/Settings.h"

void ConfigInterface()
{
	LOG(INFO) << JSL::Display::Yellow(true) << "\t-- Settings Interface --\t\t" << JSL::Display::ResetAll();
	auto out = JSL::Interface::Console(Archive);

	out.Export(Settings.RootPath / ".cortex/settings.config", true);
	Archive = out;
}

auto prompt = JSL::Display::Colour(100, 100, 100);
// Prints a question (showing the default, if any) and reads one line from std::cin.
// Empty input returns the default. If the input stream closes (EOF, or stdin piped/redirected),
// the default is used if there is one; otherwise this throws, since there's no sensible answer.
std::string Prompt(const std::string &question, const std::string &defaultValue = "")
{
	std::cout << JSL::Display::Purple() << question << ":";
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

void MakeCortex(std::string name, std::string documentclass)
{
	auto dir = Settings.RootPath / ".cortex";
	JSL::IO::mkdir(dir, JSL::IO::Policy::Generous); // can use generous because we only get here if an extant archive is malformed, so overwrite anything already present

	auto preamble = dir / "preamble.tex";
	JSL::IO::writeString(preamble, "\\def\\pgfsysdriver{pgfsys-dvisvgm.def}\n\\PassOptionsToPackage{dvisvgm}{graphicx} \\PassOptionsToPackage{dvisvgm}{graphics}\\PassOptionsToPackage{dvisvgm}{xcolor}\\documentclass{" + documentclass + "}");

	auto registry = dir / "registry.ctx";
	JSL::IO::writeString(registry, "name = " + name + "\n");

	Archive.Reset();
	Archive.Export(dir / "settings.config", true);
}

void CreateArchive()
{
	LOG(INFO) << "You will now be prompted for the setup information. Values presented " << prompt << "[like this]" << JSL::Display::ResetAll() << " are defaults, and will be used if no value is given";
	LOG(INFO)
		<< "What will the new CorTeX project be called?";
	auto name = Prompt("Archive Name", "My Project");

	std::cout << "Please select the documentclass to be assigned to files \n'cortex' will use the inbuilt class optimised for viewing CorTeX files\n";
	auto documentclass = Prompt("Document Class", "cortex");
	bool conf = Confirm("Open Settings Interface?", false);

	MakeCortex(name, documentclass);

	if (conf)
	{
		ConfigInterface();
	}
}
