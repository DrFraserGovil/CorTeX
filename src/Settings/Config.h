#pragma once
#include "Report/Report.h"
#include <filesystem>
#include <string>
#include <vector>

class ProjectSettings;

//! A project's settings, read from .cortex/settings.config and checked (DESIGN.md §11): an invalid value is reported and replaced by
//! its default, so everything downstream can trust what it's given. Read once per build and passed by reference, never global
namespace Settings
{
	//! An editor the viewer can open notes in: a name, and a command template (split into arguments, then {file}, {dir} and {root}
	//! are filled in; the note's path is added at the end if the command doesn't say where it goes)
	struct Editor
	{
		std::string Name;
		std::string Command;
	};

	//! The settings which change how notes are compiled: any change to these recompiles every note
	struct Compile
	{
		bool Pdf = false;
		double WidthCM = 25;
		double MarginPT = 8;
		bool InsertTitle = true;
		bool Extended = false;
		std::string ListType = "itemize";
		//! How many notes compile at once (at least 1); not part of the key, as it never changes what a note compiles to
		size_t Jobs = 1;

		//! A string which differs whenever any of these does (except Jobs), for a note's hash
		std::string Key() const;
	};

	//! The settings which only change how the viewer shows things: never recompile anything
	struct Display
	{
		double Zoom = 1.4;
		std::string Background;
		//! The logo image (absolute), or empty for none
		std::filesystem::path Logo;
		std::string GraphLayout = "centre";
		std::string GraphColours = "top";
		double FolderPull = 0.5;
		double FolderRepulsion = 0;
		//! The home note as written (relative to the root); matched against the notes only once they are known
		std::string Home;
	};

	//! How `cortex format` indents notes: never affects what they compile to
	struct Formatting
	{
		//! One level of indentation
		std::string Indent = "\t";
		//! The deepest heading level whose contents are indented (0: none)
		size_t Headings = 3;
		bool Environments = true;
		bool Braces = true;
		//! Environments left exactly as written
		std::vector<std::string> Verbatim = {"verbatim", "lstlisting", "minted", "comment"};
	};

	struct Config
	{
		//! The project root (absolute)
		std::filesystem::path Root;
		//! The output folder (absolute)
		std::filesystem::path Output;
		std::vector<std::string> Allowed;
		std::vector<std::string> Ignored;
		std::vector<Editor> Editors;
		Compile Compilation;
		Display Viewer;
		Formatting Format;

		//! Reads and checks the settings of the project at root; problems are added to report
		static Config Load(const std::filesystem::path &root, Report::Log &report);
		//! Checks settings already read (for the project at root)
		static Config From(const std::filesystem::path &root, const ProjectSettings &settings, Report::Log &report);
	};

	//! Parses the editors setting's "name: command" entries; problems are added to report (a malformed or repeated one is dropped)
	std::vector<Editor> ParseEditors(const std::vector<std::string> &entries, Report::Log &report);
} // namespace Settings
