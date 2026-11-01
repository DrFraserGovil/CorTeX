#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

//! The cortex:// link handler (DESIGN.md §10): how the viewer (a file:// page, which can't run programs or write files) asks for a note
//! to be opened in an editor, flagged as needing work, or created, or for the graph page's settings to become the project's.
//! @details Any web page can link to cortex://, so every link is treated as hostile: it may name only a project root, a note in that
//! project and an editor *name*. Commands only ever come from the project's own settings, and the only changes a link can make are a
//! note's `todo:` header line, a new note for a link which points at nothing, and the graph's four display settings
namespace Link
{
	/*! Carries out a cortex:// link: cortex://edit?root=R&note=N[&editor=E], cortex://todo?root=R&note=N&set=on|off, or
		cortex://create?root=R&dir=D&slug=S[&editor=E] (a new note D/S.tex titled S, only if the project has a link to S which finds no note), or
		cortex://graph?root=R[&layout=L][&colours=C][&folder-pull=P][&folder-repulsion=F] (saves those graph settings)
		@details Failures are also shown as a desktop notification, since a link opened from the browser has no terminal to report to
		@returns The project's root if the link changed it (a todo flag, a new note), so it should be rebuilt
		@throws runtime_error If the link is malformed, or names anything outside the rules above; then nothing has been run or written
	*/
	std::optional<std::filesystem::path> Handle(const std::string &url);

	//! Splits a command into arguments at whitespace; '...' or "..." keep spaces inside one argument (no other shell syntax means anything)
	std::vector<std::string> SplitCommand(std::string_view command);
} // namespace Link
