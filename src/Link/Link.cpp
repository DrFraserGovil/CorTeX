#include "Link.h"
#include "Build/Scan.h"
#include "Notes/Header.h"
#include "Notes/Reader.h"
#include "Platform/Desktop.h"
#include "Platform/Process.h"
#include "Settings/Config.h"
#include "Settings/ProjectSettings.h"
#include "Util/Text.h"
#include <JSL.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>

namespace
{
	//! Splits "cortex://action?key=value&..." into its action and (decoded) parameters
	std::pair<std::string, std::map<std::string, std::string>> ParseUrl(std::string url)
	{
		if (!url.starts_with("cortex:")) throw std::runtime_error("not a cortex: link");
		url = url.substr(7);
		while (url.starts_with("/")) url = url.substr(1);

		auto query = url.find('?');
		auto action = url.substr(0, query);
		while (action.ends_with("/")) action.pop_back();

		std::map<std::string, std::string> params;
		if (query != std::string::npos)
		{
			for (auto &pair : JSL::String::split(url.substr(query + 1), "&"))
			{
				auto equals = pair.find('=');
				if (equals == std::string::npos) continue;
				params[Util::PercentDecode(pair.substr(0, equals))] = Util::PercentDecode(pair.substr(equals + 1));
			}
		}
		return {action, params};
	}

	void ReplaceAll(std::string &text, std::string_view from, const std::string &to)
	{
		for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) text.replace(at, from.size(), to);
	}

	bool IsInside(const std::filesystem::path &real, const std::filesystem::path &root)
	{
		auto inside = real.lexically_relative(std::filesystem::canonical(root));
		return !inside.empty() && *inside.begin() != "..";
	}

	//! The project root a link names: absolute, and holding a CorTeX project
	std::filesystem::path ValidRoot(const std::string &given)
	{
		std::filesystem::path root = given;
		if (given.empty() || !root.is_absolute()) throw std::runtime_error("the link's project root must be an absolute path");
		root = std::filesystem::weakly_canonical(root);
		if (!Project::Paths::IsProject(root)) throw std::runtime_error(root.string() + " is not a CorTeX project");
		return root;
	}

	//! The note a link names: a note (with a header and title) inside the root, reached without leaving it
	std::filesystem::path ValidNote(const std::filesystem::path &root, const std::string &given)
	{
		std::filesystem::path relative = std::filesystem::path(given).lexically_normal();
		if (given.empty() || relative.is_absolute() || relative.empty() || *relative.begin() == "..")
		{
			throw std::runtime_error("the link's note must be a path inside the project");
		}
		if (!Notes::ReaderFor(relative)) throw std::runtime_error("'" + given + "' is not a note file (.ctx or .tex)");
		auto file = root / relative;
		if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("no note at " + file.string());

		// symlinks could still lead out of the project: compare where the file really is
		auto real = std::filesystem::canonical(file);
		if (!IsInside(real, root)) throw std::runtime_error(file.string() + " leads outside the project");
		auto sections = Notes::Split(JSL::IO::getFile(real));
		std::vector<Notes::HeaderProblem> ignored;
		if (!sections.HasHeader || Notes::Header::Parse(sections.Header, ignored).Title.empty())
		{
			throw std::runtime_error(file.string() + " is not a CorTeX note (no header with a title)");
		}
		return real;
	}

	//! The folder a link names for a new note: inside the root (empty means the root itself), and existing
	std::filesystem::path ValidFolder(const std::filesystem::path &root, const std::string &given)
	{
		std::filesystem::path relative = std::filesystem::path(given).lexically_normal();
		if (relative.is_absolute() || (!relative.empty() && *relative.begin() == "..")) throw std::runtime_error("the link's folder must be inside the project");
		auto folder = root / relative;
		if (!std::filesystem::is_directory(folder)) throw std::runtime_error("no folder at " + folder.string());
		auto real = std::filesystem::canonical(folder);
		if (real != std::filesystem::canonical(root) && !IsInside(real, root)) throw std::runtime_error(folder.string() + " leads outside the project");
		return real;
	}

	void Edit(const std::filesystem::path &root, const std::filesystem::path &note, const std::string &editorName)
	{
		Report::Log ignored;
		auto editors = Settings::Config::Load(root, ignored).Editors;
		if (editors.empty()) throw std::runtime_error("no editors are set up: add one to the project's 'editors' setting, e.g. [nvim: neovide {file}]");
		auto editor = editors.begin();
		if (!editorName.empty())
		{
			editor = std::find_if(editors.begin(), editors.end(), [&](auto &e) { return e.Name == editorName; });
			if (editor == editors.end()) throw std::runtime_error("no editor called '" + editorName + "' in the project's 'editors' setting");
		}

		// split first, then substitute: a path with spaces (or quotes) stays one argument, and can't add arguments of its own
		auto arguments = Link::SplitCommand(editor->Command);
		bool namesFile = false;
		for (auto &argument : arguments)
		{
			namesFile = namesFile || argument.find("{file}") != std::string::npos;
			ReplaceAll(argument, "{file}", note.string());
			ReplaceAll(argument, "{dir}", note.parent_path().string());
			ReplaceAll(argument, "{root}", root.string());
		}
		if (!namesFile) arguments.push_back(note.string()); // a command without {file} gets the note at the end, as most editors expect
		LOG(INFO) << "Opening " << note << " with '" << editor->Name << "'";
		if (auto failure = Platform::Launch({.Arguments = arguments, .WorkingDirectory = note.parent_path()}); !failure.empty())
		{
			throw std::runtime_error("editor '" + editor->Name + "': " + failure);
		}
	}

	//! Writes a new note for a link which points at nothing, then opens it: only for a slug the project actually wants, so a hostile link
	//! can at most create a note the project already asked for; the file name is the slug (letters, digits, - and _ only), never overwritten
	void Create(const std::filesystem::path &root, const std::filesystem::path &folder, const std::string &slug, const std::string &editorName)
	{
		bool safe = !slug.empty() && std::all_of(slug.begin(), slug.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; });
		if (!safe) throw std::runtime_error("'" + slug + "' is not a link slug");
		if (!Build::Scan::Read(root).Net.IsWanted(slug)) throw std::runtime_error("no note links to '" + slug + "' without finding it, so there's nothing to create");

		auto file = folder / (slug + ".ctx");
		if (std::filesystem::exists(file)) throw std::runtime_error(file.string() + " already exists (but isn't a note called '" + slug + "'); not overwriting it");
		// the slug is the provisional title; it is also an alias, so the links which wanted the note still find it after it's retitled
		JSL::IO::writeString(file, "title: " + slug + "\nalias: " + slug + "\n-----\n\n");
		LOG(INFO) << "Created " << file;
		Edit(root, file, editorName);
	}

	//! A number, written in full: no trailing text, nothing infinite or undefined
	double Number(const std::string &key, const std::string &text)
	{
		double value = 0;
		auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (text.empty() || error != std::errc() || end != text.data() + text.size() || !std::isfinite(value))
		{
			throw std::runtime_error(key + " must be a number, not '" + text + "'");
		}
		return value;
	}

	//! Saves the graph controls' values as the project's settings (DESIGN.md §9.4). Only these four settings can be named, each value is
	//! checked as strictly as in the settings file (but a bad one refuses the whole link, rather than falling back), and every other
	//! setting stays as it is
	void SaveGraph(const std::filesystem::path &root, const std::map<std::string, std::string> &params)
	{
		auto file = root / ".cortex/settings.config";
		ProjectSettings settings;
		settings.Reset();
		if (std::filesystem::exists(file)) settings.Configure(file, " ");

		bool any = false;
		for (auto &[key, value] : params)
		{
			if (key == "root") continue;
			if (key == "layout")
			{
				if (value != "centre" && value != "rings") throw std::runtime_error("layout must be centre or rings");
				settings.Render.GraphLayout = value;
			}
			else if (key == "colours")
			{
				if (value != "top" && value != "shades" && value != "every") throw std::runtime_error("colours must be top, shades or every");
				settings.Render.GraphColours = value;
			}
			else if (key == "folder-pull")
			{
				auto pull = Number(key, value);
				if (pull < 0 || pull > 1) throw std::runtime_error("folder-pull must be between 0 and 1");
				settings.Render.FolderPull = pull;
			}
			else if (key == "folder-repulsion")
			{
				auto repulsion = Number(key, value);
				if (repulsion < 0) throw std::runtime_error("folder-repulsion must be 0 or more");
				settings.Render.FolderRepulsion = repulsion;
			}
			else
			{
				throw std::runtime_error("a graph link can't change '" + key + "'");
			}
			any = true;
		}
		if (!any) throw std::runtime_error("a graph link must give at least one setting");
		settings.Export(file, true);
		LOG(INFO) << "Saved the graph settings as the project's defaults";
	}

	//! @returns true if the note changed
	bool Todo(const std::filesystem::path &note, const std::string &set)
	{
		if (set != "on" && set != "off") throw std::runtime_error("a todo link must say set=on or set=off");
		bool changed = Notes::SetTodo(note, set == "on");
		LOG(INFO) << (changed ? (set == "on" ? "Flagged " : "Unflagged ") : (set == "on" ? "Already flagged: " : "Already unflagged: ")) << note;
		return changed;
	}
} // namespace

std::vector<std::string> Link::SplitCommand(std::string_view command)
{
	std::vector<std::string> arguments;
	std::string current;
	bool inArgument = false;
	char quote = 0;
	for (char c : command)
	{
		if (quote)
		{
			if (c == quote) quote = 0;
			else current += c;
		}
		else if (c == '\'' || c == '"')
		{
			quote = c;
			inArgument = true;
		}
		else if (std::isspace(static_cast<unsigned char>(c)))
		{
			if (inArgument) arguments.push_back(current);
			current.clear();
			inArgument = false;
		}
		else
		{
			current += c;
			inArgument = true;
		}
	}
	if (inArgument) arguments.push_back(current);
	return arguments;
}

std::optional<std::filesystem::path> Link::Handle(const std::string &url)
{
	try
	{
		Platform::Desktop::RequireSupport("cortex link");
		auto [action, params] = ParseUrl(url);
		if (action != "edit" && action != "todo" && action != "create" && action != "graph") throw std::runtime_error("unknown link action '" + action + "'");
		auto root = ValidRoot(params["root"]);
		if (action == "graph")
		{
			SaveGraph(root, params);
			return root;
		}
		if (action == "create")
		{
			Create(root, ValidFolder(root, params["dir"]), params["slug"], params["editor"]);
			return root;
		}
		auto note = ValidNote(root, params["note"]);
		if (action == "edit")
		{
			Edit(root, note, params["editor"]);
			return std::nullopt;
		}
		return Todo(note, params["set"]) ? std::optional(root) : std::nullopt;
	}
	catch (std::exception &e)
	{
		Platform::Desktop::Notify(std::string("Link refused: ") + e.what());
		throw;
	}
}
