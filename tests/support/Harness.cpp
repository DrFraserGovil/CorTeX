#include "Harness.h"
#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <sys/wait.h>

namespace
{
	std::string Quote(const std::string &text)
	{
		std::string out = "'";
		for (char c : text) out += (c == '\'') ? std::string("'\\''") : std::string(1, c);
		return out + "'";
	}

	// The log wraps messages to the terminal width and adds a "| File.cpp: 12" column; undo both, so each message is one string
	std::string Unwrap(const std::string &raw)
	{
		static const std::regex column(R"(\s*\|[^|]*$)");
		static const std::regex escape("\x1b\\[[0-9;]*m");
		std::istringstream in(std::regex_replace(raw, escape, ""));
		std::string line, out;
		while (std::getline(in, line))
		{
			line = std::regex_replace(line, column, "");
			auto first = line.find_first_not_of(" \t");
			if (first == std::string::npos) continue;
			auto last = line.find_last_not_of(" \t");
			out += line.substr(first, last - first + 1) + " ";
		}
		return out;
	}

	std::string Slurp(const std::filesystem::path &file)
	{
		std::ifstream in(file, std::ios::binary);
		std::stringstream buffer;
		buffer << in.rdbuf();
		return buffer.str();
	}

	// The argument of a top-level call such as CortexIndex(...) in index.js
	nlohmann::json Call(const std::string &js, const std::string &name)
	{
		auto start = js.find(name + "(");
		if (start == std::string::npos) return nullptr;
		start += name.size() + 1;
		auto end = js.find(");\n", start);
		return nlohmann::json::parse(js.substr(start, end - start));
	}
} // namespace

namespace Harness
{
	Project::Project(const std::string &name)
	{
		static std::atomic<int> counter = 0;
		Root = std::filesystem::path(CORTEX_TEST_SCRATCH) / (name + "-" + std::to_string(counter++));
		std::filesystem::remove_all(Root);
		std::filesystem::create_directories(Root.parent_path());
		std::filesystem::copy(CORTEX_FIXTURE, Root, std::filesystem::copy_options::recursive);
	}

	Project::~Project()
	{
		std::error_code ignored;
		std::filesystem::remove_all(Root, ignored);
	}

	Output Project::Cortex(const std::string &arguments, const std::string &env) const
	{
		auto command = "cd " + Quote(Root.string()) + " && " + env + " " + Quote(CORTEX_BINARY) + " " + arguments + " 2>&1 < /dev/null";
		std::string raw;
		FILE *pipe = popen(command.c_str(), "r");
		REQUIRE(pipe != nullptr);
		std::array<char, 4096> buffer;
		while (auto n = fread(buffer.data(), 1, buffer.size(), pipe)) raw.append(buffer.data(), n);
		int status = pclose(pipe);
		return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, Unwrap(raw)};
	}

	Output Project::Build() const
	{
		auto output = Cortex("build");
		INFO(output.Text);
		REQUIRE(output.Exit == 0);
		return output;
	}

	std::string Project::Read(const std::filesystem::path &relative) const { return Slurp(Root / relative); }

	void Project::Write(const std::filesystem::path &relative, const std::string &text) const
	{
		std::filesystem::create_directories((Root / relative).parent_path());
		std::ofstream(Root / relative, std::ios::binary) << text;
	}

	void Project::Append(const std::filesystem::path &relative, const std::string &text) const { Write(relative, Read(relative) + text); }

	bool Project::Exists(const std::filesystem::path &relative) const { return std::filesystem::exists(Root / relative); }

	Index::Index(const std::filesystem::path &viewFolder)
	{
		Raw = Slurp(viewFolder / "index.js");
		Notes = nlohmann::json::object();
		for (auto &note : Call(Raw, "CortexIndex")) Notes[note["id"].get<std::string>()] = note;
		Sidebar = Call(Raw, "CortexSidebar");
		Config = Call(Raw, "CortexConfig");
	}

	const nlohmann::json &Index::operator[](const std::string &id) const
	{
		INFO("note " << id);
		REQUIRE(Notes.contains(id));
		return Notes.at(id);
	}

	std::set<std::string> Index::Failed() const
	{
		std::set<std::string> failed;
		for (auto &[id, note] : Notes.items())
		{
			if (note["status"] == "failed") failed.insert(note["path"].get<std::string>());
		}
		return failed;
	}

	std::string Messages(const Output &output, const Project &project)
	{
		auto log = project.Root / ".cortex/build.log";
		return output.Text + (std::filesystem::exists(log) ? " " + Unwrap(Slurp(log)) : "");
	}

	std::set<std::string> Compiled(const Output &output, const Project &project)
	{
		static const std::regex compiling(R"re(Compiling "?([^"\s]+\.(?:tex|ctx)))re");
		auto text = Messages(output, project);
		std::set<std::string> paths;
		for (std::sregex_iterator match(text.begin(), text.end(), compiling), end; match != end; ++match) paths.insert((*match)[1]);
		return paths;
	}

	bool HaveTex()
	{
		static bool have = std::system("command -v xelatex > /dev/null && command -v dvisvgm > /dev/null") == 0;
		return have;
	}

	namespace
	{
		struct Shared
		{
			Project Copy{"built"};
			Output Result = Copy.Build();
		};
		const Shared &SharedBuild()
		{
			static Shared shared;
			return shared;
		}
	} // namespace

	const Project &Built() { return SharedBuild().Copy; }
	const Output &BuiltOutput() { return SharedBuild().Result; }
} // namespace Harness
