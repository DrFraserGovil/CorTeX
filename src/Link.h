#pragma once
#include <filesystem>
#include <string>
#include <vector>

//! The cortex:// link handler: how the viewer (a file:// page, which can't run programs or write files) asks for a note to be opened in an
//! editor or flagged as needing work.
//! @details Any web page can link to cortex://, so every link is treated as hostile: it may name only a project root, a note in that project
//! and an editor *name*. Commands only ever come from the project's own settings, and the only changes a link can make are a note's `todo:`
//! header line, and a new note for a link which points at nothing.
namespace Link
{
	//! One entry of the `editors` setting
	struct Editor
	{
		std::string Name;
		std::string Command;
	};

	//! Parses the `editors` setting ("name: command" entries); malformed or duplicate entries are skipped, with a message in problems
	std::vector<Editor> ParseEditors(const std::vector<std::string> &entries, std::vector<std::string> &problems);

	//! Carries out a cortex:// link: cortex://edit?root=R&note=N[&editor=E], cortex://todo?root=R&note=N&set=on|off, or
	//! cortex://create?root=R&dir=D&slug=S[&editor=E] (a new note D/S.tex titled S, only if the project has a link to S which finds no note)
	//! @details Configures the global Archive from R's settings. Failures are also shown as a desktop notification (when notify-send exists),
	//! since a link launched from the browser has no terminal to report to
	//! @throws runtime_error If the link is malformed, or names anything outside the rules above; nothing has been run or written
	void Handle(const std::string &url);

	//! Registers this executable as the desktop's handler for cortex:// links (a .desktop file plus xdg-mime). Linux/freedesktop only
	void InstallHandler();
} // namespace Link
