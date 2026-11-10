#pragma once
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

//! Everything that differs between operating systems, kept here so the rest of CorTeX is portable
namespace Platform
{
	//! True where the viewer's Editor tools work (DESIGN.md §13.5): opening editors and handling cortex:// links
	constexpr bool EditorIntegration =
#if defined(__linux__)
		true;
#else
		false;
#endif

	//! The separator between folders in a search path such as PATH or TEXINPUTS
	constexpr char PathListSeparator =
#if defined(_WIN32)
		';';
#else
		':';
#endif

	//! A program to run: never through a shell, so no argument is ever interpreted (quotes, spaces, $ and the like are just characters)
	struct Command
	{
		//! The program (found on PATH unless it contains a separator), then its arguments
		std::vector<std::string> Arguments;
		//! Where it runs; empty for the current directory
		std::filesystem::path WorkingDirectory;
		//! Variables set (or replaced) in its environment; the rest is inherited
		std::vector<std::pair<std::string, std::string>> Environment;
		//! Where its output (stdout and stderr together) goes; empty discards it. Its input is always empty
		std::filesystem::path OutputFile;
		//! Stopped (with everything it started) if still running after this long; zero for no limit
		std::chrono::seconds Timeout{0};
	};

	struct Outcome
	{
		enum class Status
		{
			Exited,		  //!< it ran and finished: see ExitCode
			TimedOut,	  //!< it was stopped after Timeout
			CouldNotStart //!< it never ran: see Error
		};
		Status State;
		int ExitCode = -1;
		std::string Error;

		bool Succeeded() const { return State == Status::Exited && ExitCode == 0; }
	};

	//! Runs a program and waits for it
	Outcome Run(const Command &command);

	//! A program started with a pipe to its input, for CorTeX to write to later (see Tex::WarmPool); command.Timeout is ignored, as the
	//! time limit starts when it is waited for. It is stopped, with everything it started, if still running when this is destroyed
	class Child
	{
	  public:
		//! Starts the program; nullptr (and error says why) if it couldn't be. Not available on Windows, where this always fails
		static std::unique_ptr<Child> Start(const Command &command, std::string &error);
		~Child();
		Child(const Child &) = delete;
		Child &operator=(const Child &) = delete;

		//! Writes text to the program's input, then closes it, so that reading any more gives end-of-file. False if the program has
		//! already exited
		bool Finish(std::string_view text);
		//! Waits for the program to exit, stopping it (and everything it started) if it is still running after timeout (zero: no limit)
		Outcome Wait(std::chrono::seconds timeout);
		//! False once the program has exited
		bool Running();

	  private:
		Child() = default;
		long Id = -1;	   //!< the process id
		int Input = -1;	   //!< the writing end of the pipe to its input; -1 once closed
		bool Done = false; //!< it has exited, and been waited for
		int Status = 0;
	};

	//! Starts a program and leaves it running on its own, detached from CorTeX, so it outlives it (an editor)
	//! @returns An empty string if it started, else why it didn't
	std::string Launch(const Command &command);

	//! Where a program would be found on PATH, or nullopt if it can't be
	std::optional<std::filesystem::path> Find(const std::string &program);

	//! The path of the running cortex executable
	std::filesystem::path Executable();
} // namespace Platform
