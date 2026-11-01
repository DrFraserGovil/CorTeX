#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

// Support for the end-to-end tests: each test works on its own copy of tests/fixture, runs the real cortex binary on it, and inspects
// what it produced. The binary is treated as a black box, so these tests describe DESIGN.md's behaviour, not the code's structure.
namespace Harness
{
	struct Output
	{
		int Exit;
		//! Everything the command printed, with the log's line wrapping and source-location column removed, so a message can be
		//! found as one string
		std::string Text;
	};

	//! A fresh copy of tests/fixture, deleted when the test ends
	class Project
	{
	  public:
		explicit Project(const std::string &name);
		~Project();
		Project(const Project &) = delete;
		Project &operator=(const Project &) = delete;

		std::filesystem::path Root;

		//! Runs cortex with these arguments from the project root; env is prefixed to the command (e.g. "PATH=/nonexistent")
		Output Cortex(const std::string &arguments, const std::string &env = "") const;
		//! A build, which must succeed (exit code 0)
		Output Build() const;

		std::string Read(const std::filesystem::path &relative) const;
		void Write(const std::filesystem::path &relative, const std::string &text) const;
		void Append(const std::filesystem::path &relative, const std::string &text) const;
		bool Exists(const std::filesystem::path &relative) const;
	};

	//! The viewer's index.js in a folder, as JSON
	struct Index
	{
		explicit Index(const std::filesystem::path &viewFolder);
		nlohmann::json Notes;	//!< id -> entry
		nlohmann::json Sidebar; //!< the global sidebar groups
		nlohmann::json Config;	//!< the CortexConfig object
		std::string Raw;		//!< the whole file

		const nlohmann::json &operator[](const std::string &id) const;
		//! The ids of the notes whose status is failed
		std::set<std::string> Failed() const;
	};

	//! The note paths a build says it compiled ("Compiling <path>"), from its output and the build log, if there is one
	std::set<std::string> Compiled(const Output &output, const Project &project);

	//! A build's output together with its build log (if any): where any message from a build may be
	std::string Messages(const Output &output, const Project &project);

	//! True if xelatex and dvisvgm can be run; tests which compile notes are skipped otherwise
	bool HaveTex();

	//! One full build of the fixture, shared by every test that only reads the result (building takes a while)
	const Project &Built();
	//! What that build printed
	const Output &BuiltOutput();
} // namespace Harness

#define REQUIRE_TEX()                                                                                                                       \
	do                                                                                                                                     \
	{                                                                                                                                      \
		if (!Harness::HaveTex()) SKIP("xelatex and dvisvgm are needed");                                                               \
	} while (false)
