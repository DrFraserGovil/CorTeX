#include "Desktop.h"
#include "Process.h"
#include <JSL.h>
#include <cstdlib>

void Platform::Desktop::RequireSupport(const std::string &what)
{
	if (!EditorIntegration) throw std::runtime_error(what + " isn't supported on this platform: acting on a project from the viewer only works on Linux");
}

void Platform::Desktop::Notify(const std::string &message)
{
	if (!EditorIntegration || !Find("notify-send")) return;
	Launch({.Arguments = {"notify-send", "CorTeX", message}, .WorkingDirectory = "/"});
}

void Platform::Desktop::InstallHandler(const std::filesystem::path &executable)
{
	RequireSupport("install-handler");
	auto home = std::getenv("HOME");
	if (!home) throw std::runtime_error("HOME is not set, so there is nowhere to register the link handler");
	auto applications = std::filesystem::path(home) / ".local/share/applications";
	std::filesystem::create_directories(applications);

	auto desktopFile = applications / "cortex-url.desktop";
	JSL::IO::writeString(desktopFile, "[Desktop Entry]\n"
									  "Type=Application\n"
									  "Name=CorTeX link handler\n"
									  "Comment=Opens notes from the CorTeX viewer in an editor, and flags them as needing work\n"
									  "Exec=\"" + executable.string() + "\" link -url %u\n"
									  "MimeType=x-scheme-handler/cortex;\n"
									  "NoDisplay=true\n"
									  "Terminal=false\n");
	LOG(INFO) << "Wrote " << desktopFile;

	auto log = applications / "cortex-url.log";
	auto registered = Run({.Arguments = {"xdg-mime", "default", "cortex-url.desktop", "x-scheme-handler/cortex"}, .WorkingDirectory = applications, .OutputFile = log});
	if (!registered.Succeeded()) throw std::runtime_error("xdg-mime could not register the handler; see " + log.string());
	std::filesystem::remove(log);
	Run({.Arguments = {"update-desktop-database", applications.string()}}); // optional: not every desktop has it
	LOG(INFO) << "cortex:// links now open with " << executable << ". The browser asks once before handing one over";
}
