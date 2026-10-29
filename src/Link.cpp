#include "Link.h"
#include "Build/Note.h"
#include "Build/Shell.h"
#include "Project.h"
#include "Settings/ArchiveSettings.h"
#include <JSL.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
	//! Undoes %XX escapes (as written by the viewer's encodeURIComponent)
	std::string PercentDecode(std::string_view text)
	{
		std::string out;
		for (size_t i = 0; i < text.size(); ++i)
		{
			if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) && std::isxdigit(static_cast<unsigned char>(text[i + 2])))
			{
				out += static_cast<char>(std::stoi(std::string(text.substr(i + 1, 2)), nullptr, 16));
				i += 2;
			}
			else
			{
				out += text[i];
			}
		}
		return out;
	}

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
				params[PercentDecode(pair.substr(0, equals))] = PercentDecode(pair.substr(equals + 1));
			}
		}
		return {action, params};
	}

	//! Splits a command into arguments at whitespace; '...' or "..." keep spaces inside one argument (no other shell syntax means anything)
	std::vector<std::string> SplitCommand(std::string_view command)
	{
		std::vector<std::string> args;
		std::string current;
		bool inArgument = false;
		char quote = 0;
		for (char c : command)
		{
			if (quote)
			{
				if (c == quote)
					quote = 0;
				else
					current += c;
			}
			else if (c == '\'' || c == '"')
			{
				quote = c;
				inArgument = true;
			}
			else if (std::isspace(static_cast<unsigned char>(c)))
			{
				if (inArgument) args.push_back(current);
				current.clear();
				inArgument = false;
			}
			else
			{
				current += c;
				inArgument = true;
			}
		}
		if (inArgument) args.push_back(current);
		return args;
	}

	void ReplaceAll(std::string &text, std::string_view from, const std::string &to)
	{
		for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		{
			text.replace(at, from.size(), to);
		}
	}

	//! Starts a program directly (no shell), detached from cortex so it outlives it
	//! @returns An empty string if the program started, else why it didn't
	std::string Spawn(const std::vector<std::string> &args, const std::filesystem::path &workingDir)
	{
		if (args.empty()) return "the command is empty";

		// the child reports a failed exec down this pipe; on success the pipe closes unwritten (close-on-exec)
		int report[2];
		if (pipe2(report, O_CLOEXEC) != 0) return std::strerror(errno);

		pid_t child = fork();
		if (child < 0)
		{
			close(report[0]);
			close(report[1]);
			return std::strerror(errno);
		}
		if (child == 0)
		{
			setsid(); // its own session: closing the browser (or whatever launched cortex) doesn't take the editor with it
			int null = open("/dev/null", O_RDWR);
			dup2(null, STDIN_FILENO);
			dup2(null, STDOUT_FILENO);
			dup2(null, STDERR_FILENO);
			if (chdir(workingDir.c_str()) != 0) _exit(127);

			std::vector<char *> argv;
			for (auto &arg : args) argv.push_back(const_cast<char *>(arg.c_str()));
			argv.push_back(nullptr);
			execvp(argv[0], argv.data());

			int error = errno;
			[[maybe_unused]] auto written = write(report[1], &error, sizeof(error));
			_exit(127);
		}

		close(report[1]);
		int error = 0;
		auto got = read(report[0], &error, sizeof(error));
		close(report[0]);
		if (got == sizeof(error))
		{
			waitpid(child, nullptr, 0);
			return "could not run '" + args[0] + "': " + std::strerror(error);
		}
		return "";
	}

	//! Tells the user something from a process with no terminal (best effort)
	void Notify(const std::string &message)
	{
		Spawn({"notify-send", "CorTeX", message}, "/");
	}

	//! The project root a link names: absolute, and holding a CorTeX project
	std::filesystem::path ValidRoot(const std::string &given)
	{
		std::filesystem::path root = given;
		if (given.empty() || !root.is_absolute()) throw std::runtime_error("the link's project root must be an absolute path");
		root = std::filesystem::weakly_canonical(root);
		if (!std::filesystem::exists(root / ".cortex/registry.ctx") || !std::filesystem::exists(root / ".cortex/settings.config"))
		{
			throw std::runtime_error(root.string() + " is not a CorTeX project");
		}
		return root;
	}

	//! The note a link names: a .tex note (with a header) inside the root, reached without leaving it
	std::filesystem::path ValidNote(const std::filesystem::path &root, const std::string &given)
	{
		std::filesystem::path relative = std::filesystem::path(given).lexically_normal();
		if (given.empty() || relative.is_absolute() || relative.empty() || *relative.begin() == "..")
		{
			throw std::runtime_error("the link's note must be a path inside the project");
		}
		if (relative.extension() != ".tex") throw std::runtime_error("'" + given + "' is not a .tex file");

		auto file = root / relative;
		if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("no note at " + file.string());

		// symlinks could still lead out of the project: compare where the file really is
		auto real = std::filesystem::canonical(file);
		auto inside = real.lexically_relative(std::filesystem::canonical(root));
		if (inside.empty() || *inside.begin() == "..") throw std::runtime_error(file.string() + " leads outside the project");
		if (!Note::HasNoteHeader(real)) throw std::runtime_error(file.string() + " is not a CorTeX note (no header with a title)");
		return real;
	}

	//! The folder a link names for a new note: inside the root (empty means the root itself), and existing
	std::filesystem::path ValidFolder(const std::filesystem::path &root, const std::string &given)
	{
		std::filesystem::path relative = std::filesystem::path(given).lexically_normal();
		if (relative.is_absolute() || (!relative.empty() && *relative.begin() == ".."))
		{
			throw std::runtime_error("the link's folder must be inside the project");
		}
		auto folder = root / relative;
		if (!std::filesystem::is_directory(folder)) throw std::runtime_error("no folder at " + folder.string());

		auto real = std::filesystem::canonical(folder);
		auto inside = real.lexically_relative(std::filesystem::canonical(root));
		if (inside.empty() || *inside.begin() == "..") throw std::runtime_error(folder.string() + " leads outside the project");
		return real;
	}

	void Edit(const std::filesystem::path &root, const std::filesystem::path &note, const std::string &editorName)
	{
		std::vector<std::string> problems;
		auto editors = Link::ParseEditors(Archive.Files.Editors, problems);
		if (editors.empty()) throw std::runtime_error("no editors are set up: add one to the project's 'editors' setting, e.g. [nvim: neovide {file}]");

		auto editor = editors.begin();
		if (!editorName.empty())
		{
			editor = std::find_if(editors.begin(), editors.end(), [&](auto &e) { return e.Name == editorName; });
			if (editor == editors.end()) throw std::runtime_error("no editor called '" + editorName + "' in the project's 'editors' setting");
		}

		// split first, then substitute: a path with spaces (or quotes) stays one argument, and can't add arguments of its own
		auto args = SplitCommand(editor->Command);
		bool namesFile = false;
		for (auto &arg : args)
		{
			namesFile = namesFile || arg.find("{file}") != std::string::npos;
			ReplaceAll(arg, "{file}", note.string());
			ReplaceAll(arg, "{dir}", note.parent_path().string());
			ReplaceAll(arg, "{root}", root.string());
		}
		if (!namesFile)
		{
			args.push_back(note.string()); // a command without {file} gets the note at the end, as most editors expect
		}
		LOG(INFO) << "Opening " << note << " with '" << editor->Name << "'";
		if (auto failure = Spawn(args, note.parent_path()); !failure.empty())
		{
			throw std::runtime_error("editor '" + editor->Name + "' " + failure);
		}
	}

	//! Writes a new note for a link which points at nothing, then opens it: only for a slug the project actually wants, so a hostile link
	//! can at most create a note the project already asked for; the file name is the slug (letters, digits, - and _ only), never overwritten
	void Create(const std::filesystem::path &root, const std::filesystem::path &folder, const std::string &slug, const std::string &editorName)
	{
		bool safe = !slug.empty() && std::all_of(slug.begin(), slug.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; });
		if (!safe) throw std::runtime_error("'" + slug + "' is not a link slug");

		CortexProject project(root);
		if (!project.IsWanted(slug)) throw std::runtime_error("no note links to '" + slug + "' without finding it, so there's nothing to create");

		auto file = folder / (slug + ".tex");
		if (std::filesystem::exists(file)) throw std::runtime_error(file.string() + " already exists (but isn't a note called '" + slug + "'); not overwriting it");
		// the slug is the provisional title; it is also an alias, so the links which wanted the note still find it after it's retitled
		JSL::IO::writeString(file, "title: " + slug + "\nalias: " + slug + "\n-----\n\n");
		LOG(INFO) << "Created " << file;

		Edit(root, file, editorName);
		project.Build(); // so the link resolves (a running watch is asked to rebuild instead, and will have seen the new file anyway)
	}

	void Todo(const std::filesystem::path &root, const std::filesystem::path &note, const std::string &set)
	{
		if (set != "on" && set != "off") throw std::runtime_error("a todo link must say set=on or set=off");
		if (!Note::SetTodo(note, set == "on"))
		{
			LOG(INFO) << note << " was already " << (set == "on" ? "flagged" : "unflagged");
			return;
		}
		LOG(INFO) << (set == "on" ? "Flagged " : "Unflagged ") << note;
		// a running watch is asked to rebuild instead; the flag is display-only, so this only rewrites the index (and the open viewer refreshes)
		CortexProject(root).Build();
	}
} // namespace

std::vector<Link::Editor> Link::ParseEditors(const std::vector<std::string> &entries, std::vector<std::string> &problems)
{
	std::vector<Editor> editors;
	for (auto &entry : entries)
	{
		auto colon = entry.find(':');
		std::string name = JSL::String::trim(std::string_view(entry).substr(0, colon == std::string::npos ? entry.size() : colon));
		std::string command = colon == std::string::npos ? "" : JSL::String::trim(std::string_view(entry).substr(colon + 1));
		if (name.empty() || command.empty())
		{
			problems.push_back("editor entry '" + entry + "' should be \"name: command\"");
		}
		else if (std::any_of(editors.begin(), editors.end(), [&](auto &e) { return e.Name == name; }))
		{
			problems.push_back("editor '" + name + "' is defined twice; the first is used");
		}
		else
		{
			if (command.find("{file}") == std::string::npos)
			{
				problems.push_back("editor '" + name + "' doesn't say where the note goes ({file}), so its path is added at the end of the command");
			}
			editors.push_back({name, command});
		}
	}
	return editors;
}

void Link::Handle(const std::string &url)
{
	try
	{
		auto [action, params] = ParseUrl(url);
		if (action != "edit" && action != "todo" && action != "create") throw std::runtime_error("unknown link action '" + action + "'");

		auto root = ValidRoot(params["root"]);
		Archive.Configure(root / ".cortex/settings.config", " ");
		if (action == "create")
		{
			Create(root, ValidFolder(root, params["dir"]), params["slug"], params["editor"]);
			return;
		}
		auto note = ValidNote(root, params["note"]);

		if (action == "edit")
			Edit(root, note, params["editor"]);
		else
			Todo(root, note, params["set"]);
	}
	catch (std::exception &e)
	{
		Notify(std::string("Link refused: ") + e.what());
		throw;
	}
}

void Link::InstallHandler()
{
	auto home = std::getenv("HOME");
	if (!home) throw std::runtime_error("HOME is not set, so there is nowhere to register the link handler");
	auto applications = std::filesystem::path(home) / ".local/share/applications";
	std::filesystem::create_directories(applications);

	auto executable = std::filesystem::canonical("/proc/self/exe");
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
	if (Shell::Run("xdg-mime default cortex-url.desktop x-scheme-handler/cortex", applications, log) != 0)
	{
		throw std::runtime_error("xdg-mime could not register the handler; see " + log.string());
	}
	std::filesystem::remove(log);
	Shell::Run("update-desktop-database " + Shell::Quote(applications), applications, "/dev/null"); // optional: not every desktop has it
	LOG(INFO) << "cortex:// links now open with " << executable << ". The browser asks once before handing one over";
}
