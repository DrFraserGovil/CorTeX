#pragma once
#include <filesystem>
#include <string>

//! The desktop integration behind acting on the project from the viewer (DESIGN.md §10). Linux (freedesktop) only, like the Editor tools
//! themselves (DESIGN.md §13.5): elsewhere, everything here says it isn't supported
namespace Platform::Desktop
{
	//! Throws the "not supported" error on platforms without editor integration
	void RequireSupport(const std::string &what);

	//! Shows the author a message from a process with no terminal (a link opened from the browser); best effort, never fails
	void Notify(const std::string &message);

	//! Registers executable as the desktop's handler for cortex:// links (a .desktop file and xdg-mime)
	//! @throws runtime_error If it can't, or the platform isn't supported
	void InstallHandler(const std::filesystem::path &executable);
} // namespace Platform::Desktop
