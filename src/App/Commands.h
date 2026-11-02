#pragma once
#include <filesystem>
#include <string>

//! The commands (DESIGN.md §13): each takes the project root (absolute) and does its whole job, reporting as it goes
namespace App
{
	//! Brings the output up to date (or, with clean, rebuilds it from scratch); if another process is working on the project, it is asked to instead
	void Build(const std::filesystem::path &root, bool clean);
	//! Builds, then rebuilds whenever anything in the project changes, until q + Enter or `cortex shutdown`
	void Watch(const std::filesystem::path &root);
	//! Stops whatever process is working on the project
	void Shutdown(const std::filesystem::path &root);
	//! Brings the output up to date, then copies it into folder as a copy to share (DESIGN.md §13.3)
	void Export(const std::filesystem::path &root, const std::filesystem::path &folder);
	//! Re-indents the note file in place (DESIGN.md §13.6), with the settings of the project containing it (or root, if given; defaults
	//! if neither is a project); with no file, formats standard input to standard output
	void Format(const std::filesystem::path &file, const std::filesystem::path &root);
	//! Runs the language server (DESIGN.md §13.7) on standard input and output until the editor ends it
	//! @returns The exit code the protocol asks for: 0 after a clean shutdown, else 1
	int Lsp();
	//! Asks for a name, and creates a project at root
	void New(const std::filesystem::path &root);
	//! Edits the project's settings interactively; a change which recompiles every note is only saved with a rebuild
	void EditSettings(const std::filesystem::path &root);
	//! Carries out a cortex:// link from the viewer
	void Link(const std::string &url);
	//! Registers this cortex as the handler for cortex:// links
	void InstallHandler();
} // namespace App
