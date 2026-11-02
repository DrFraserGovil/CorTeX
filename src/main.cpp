#include "App/Commands.h"
#include "Project/Paths.h"
#include "Settings/RuntimeSettings.h"
#include <JSL.h>
#include <functional>
#include <map>

namespace
{
	void ConfigureLog(const RuntimeSettings &runtime)
	{
		JSL::Log::Global().SetLevel(INFO);
		JSL::Log::Global().ShowHeaders = false;
		if (runtime.Verbose)
		{
			JSL::Log::Global().ShowHeaders = true;
			JSL::Log::Global().SetLevel(DEBUG);
		}
		else if (runtime.Quiet)
		{
			JSL::Log::Global().SetLevel(WARN);
		}
	}
} // namespace

int main(int argc, char **argv)
{
	RuntimeSettings runtime;
	ConfigureLog(runtime);
	int exitCode = 0;
	try
	{
		runtime.Parse(argc, argv);
		ConfigureLog(runtime);
		if (runtime.Commands.size() > 1) throw std::runtime_error("one command at a time, please (given: " + JSL::String::stitch(runtime.Commands, 0, runtime.Commands.size(), ", ") + ")");
		auto command = runtime.Commands.empty() ? std::string("build") : JSL::String::getLower(runtime.Commands[0]);
		auto root = std::filesystem::weakly_canonical(runtime.RootPath.empty() ? std::filesystem::current_path() : std::filesystem::absolute(runtime.RootPath));

		// the commands which work on an existing project; the rest name their own (a link carries its root), make one, or need none
		std::map<std::string, std::function<void()>> onProject = {
			{"build", [&] { App::Build(root, false); }},
			{"rebuild", [&] { App::Build(root, true); }},
			{"watch", [&] { App::Watch(root); }},
			{"shutdown", [&] { App::Shutdown(root); }},
			{"settings", [&] { App::EditSettings(root); }},
			{"export", [&] {
				 if (runtime.ExportFolder.empty()) throw std::runtime_error("export needs a folder: cortex export -to <folder>");
				 App::Export(root, runtime.ExportFolder);
			 }},
		};
		std::map<std::string, std::function<void()>> others = {
			{"new", [&] { App::New(root); }},
			{"link", [&] { App::Link(runtime.Url); }},
			{"install-handler", [&] { App::InstallHandler(); }},
			{"format", [&] { App::Format(runtime.File, runtime.RootPath); }},
			{"lsp", [&] { exitCode = App::Lsp(); }},
		};

		if (auto action = onProject.find(command); action != onProject.end())
		{
			if (!Project::Paths::IsProject(root)) throw std::runtime_error("there is no CorTeX project at " + root.string() + ": move to one, or create one with 'cortex new'");
			action->second();
		}
		else if (auto other = others.find(command); other != others.end())
		{
			other->second();
		}
		else
		{
			throw std::runtime_error("unrecognised command '" + command + "'; use -h to see the commands");
		}
	}
	catch (std::exception &e)
	{
		LOG(ERROR) << JSL::Display::Red() << "CorTeX stopped: " << e.what();
		return 1;
	}
	return exitCode;
}
