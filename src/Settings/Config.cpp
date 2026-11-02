#include "Config.h"
#include "ProjectSettings.h"
#include "Util/Files.h"
#include <JSL.h>
#include <algorithm>
#include <thread>

std::string Settings::Compile::Key() const
{
	std::string key;
	key += JSL::String::makeFrom(WidthCM) + '\0' + JSL::String::makeFrom(MarginPT);
	key += '\0' + std::string(Pdf ? "pdf" : "svg");
	key += '\0' + std::string(Extended ? "extended" : "plain") + '\0' + ListType;
	key += '\0' + std::string(InsertTitle ? "title" : "untitled");
	return key;
}

std::vector<Settings::Editor> Settings::ParseEditors(const std::vector<std::string> &entries, Report::Log &report)
{
	std::vector<Editor> editors;
	for (auto &entry : entries)
	{
		auto colon = entry.find(':');
		std::string name = JSL::String::trim(std::string_view(entry).substr(0, colon == std::string::npos ? entry.size() : colon));
		std::string command = colon == std::string::npos ? "" : JSL::String::trim(std::string_view(entry).substr(colon + 1));
		if (name.empty() || command.empty())
		{
			report.Add(Report::Kind::Setting, "editor entry '" + entry + "' should be \"name: command\"");
		}
		else if (std::any_of(editors.begin(), editors.end(), [&](auto &e) { return e.Name == name; }))
		{
			report.Add(Report::Kind::Setting, "editor '" + name + "' is defined twice; the first is used");
		}
		else
		{
			if (command.find("{file}") == std::string::npos)
			{
				report.Add(Report::Kind::Setting, "editor '" + name + "' doesn't say where the note goes ({file}), so its path is added at the end of the command");
			}
			editors.push_back({name, command});
		}
	}
	return editors;
}

Settings::Config Settings::Config::Load(const std::filesystem::path &root, Report::Log &report)
{
	ProjectSettings settings;
	settings.Reset();
	settings.Configure(root / ".cortex/settings.config", " ");
	return From(root, settings, report);
}

Settings::Config Settings::Config::From(const std::filesystem::path &root, const ProjectSettings &settings, Report::Log &report)
{
	auto invalid = [&](const std::string &message) { report.Add(Report::Kind::Setting, message, ".cortex/settings.config"); };

	Config config;
	config.Root = root;
	config.Allowed = settings.Files.AllowedFilePatterns;
	config.Ignored = settings.Files.IgnoredFilePatterns;
	config.Editors = ParseEditors(settings.Files.Editors, report);

	// the output folder is the author's choice, but never one whose sweep would delete the project itself
	std::filesystem::path output = settings.Files.Output.empty() ? "view" : settings.Files.Output;
	output = std::filesystem::weakly_canonical(output.is_absolute() ? output : root / output);
	if (output == root || Util::IsInside(root, output) || output == root / ".cortex" || Util::IsInside(output, root / ".cortex"))
	{
		invalid("output '" + settings.Files.Output + "' would overwrite the project itself; using view");
		output = root / "view";
	}
	config.Output = output;

	auto &compile = config.Compilation;
	compile.Pdf = settings.Render.PdfOutput;
	compile.WidthCM = settings.Render.WidthCM;
	if (!(compile.WidthCM > 0))
	{
		invalid("width must be a positive number of cm; using 25");
		compile.WidthCM = 25;
	}
	compile.MarginPT = settings.Render.MarginPT;
	if (!(compile.MarginPT >= 0))
	{
		invalid("margin must be 0 or more (in pt); using 8");
		compile.MarginPT = 8;
	}
	compile.InsertTitle = settings.Render.InsertTitle;
	if (settings.Render.CompileJobs < 0) invalid("jobs must be 0 (one per core) or more; using one per core");
	compile.Jobs = settings.Render.CompileJobs > 0 ? static_cast<size_t>(settings.Render.CompileJobs) : std::max(1u, std::thread::hardware_concurrency());
	compile.Extended = settings.Syntax.ExtendedSyntax;
	compile.ListType = settings.Syntax.DefaultListType;
	if (compile.ListType != "itemize" && compile.ListType != "enumerate")
	{
		invalid("list-type must be itemize or enumerate; using itemize");
		compile.ListType = "itemize";
	}

	auto &display = config.Viewer;
	display.Zoom = settings.Render.ViewerZoom;
	if (!(display.Zoom > 0))
	{
		invalid("zoom must be a positive number (1 = true size); using 1.4");
		display.Zoom = 1.4;
	}
	display.Background = settings.Render.BackgroundColour;
	display.FolderPull = settings.Render.FolderPull;
	if (!(display.FolderPull >= 0 && display.FolderPull <= 1))
	{
		invalid("folder-pull must be between 0 and 1 (1 ignores folders); using 0.5");
		display.FolderPull = 0.5;
	}
	display.GraphLayout = settings.Render.GraphLayout;
	if (display.GraphLayout != "centre" && display.GraphLayout != "rings")
	{
		invalid("graph-layout must be centre or rings; using centre");
		display.GraphLayout = "centre";
	}
	display.GraphColours = settings.Render.GraphColours;
	if (display.GraphColours != "top" && display.GraphColours != "shades" && display.GraphColours != "every")
	{
		invalid("graph-colours must be top, shades or every; using top");
		display.GraphColours = "top";
	}
	display.FolderRepulsion = settings.Render.FolderRepulsion;
	if (!(display.FolderRepulsion >= 0))
	{
		invalid("folder-repulsion must be 0 or more (0 treats every folder alike); using 0");
		display.FolderRepulsion = 0;
	}
	if (!settings.Render.Logo.empty())
	{
		auto logo = root / settings.Render.Logo;
		if (std::filesystem::is_regular_file(logo) && Util::ImageMediaType(logo)) display.Logo = logo;
		else invalid("logo '" + settings.Render.Logo + "' isn't an image file in the project (PNG, JPEG, GIF, WebP or SVG); the sidebar will have no logo");
	}
	display.Home = settings.Files.HomeNote;

	auto &format = config.Format;
	auto indent = JSL::String::getLower(JSL::String::trim(settings.Format.Indent));
	if (indent == "tab") format.Indent = "\t";
	else if (!indent.empty() && indent.size() <= 2 && std::all_of(indent.begin(), indent.end(), [](char c) { return c >= '0' && c <= '9'; }) && std::stoi(indent) > 0)
	{
		format.Indent = std::string(std::stoi(indent), ' ');
	}
	else invalid("format-indent must be tab or a number of spaces; using tab");
	if (settings.Format.Headings < 0)
	{
		invalid("format-headings must be 0 or more (0 indents after no heading); using 3");
	}
	else format.Headings = settings.Format.Headings;
	format.Environments = settings.Format.Environments;
	format.Braces = settings.Format.Braces;
	format.Verbatim = settings.Format.Verbatim;
	return config;
}
