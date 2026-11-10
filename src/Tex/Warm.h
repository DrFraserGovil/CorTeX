#pragma once
#include "Platform/Process.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

//! Warm workers (DESIGN.md §6.7): XeLaTeX runs started ahead of time, each paused just after the project preamble, waiting to be told
//! the name of a file holding the rest of a page. Loading a heavy preamble (fonts, TikZ) can take most of a pass, so a warm worker
//! compiles a note in a fraction of the time. A worker runs in the folder of the note it will compile, exactly as a fresh XeLaTeX
//! would (so relative inputs and images behave the same): the pool keeps its workers waiting in the folder of the last note compiled,
//! where the next is most likely to be. Only a process which holds a project between builds (a watch, or a language server) keeps a
//! pool; a worker can never outlive it, as its input is a pipe from CorTeX, and reading past its end ends the run
namespace Tex
{
	class WarmPool
	{
	  public:
		//! What every worker is started with: a change to any of it (the project preamble, the class, the settings) replaces them all
		struct Spec
		{
			//! Where the workers write everything: CorTeX's own folder, holding nothing of the author's
			std::filesystem::path Folder;
			//! Where workers wait before any note has been compiled (the project root)
			std::filesystem::path Root;
			//! The class and the project preamble, followed by the line which waits for the rest of the page
			std::string Driver;
			//! XeLaTeX and its options, without the job name, output folder or file
			std::vector<std::string> Arguments;
			std::vector<std::pair<std::string, std::string>> Environment;

			bool operator==(const Spec &) const = default;
		};

		//! A worker, taken from the pool: its run, the job name its files are written under in Folder, and the folder it runs in
		struct Worker
		{
			std::unique_ptr<Platform::Child> Run;
			std::string Name;
			std::filesystem::path Folder;
			std::filesystem::path In;
		};

		//! How long the pool may go unused before its waiting workers are stopped (each holds the loaded preamble in memory); they are
		//! started again by the next build
		static constexpr std::chrono::minutes IdleTime{10};

		WarmPool();
		~WarmPool();
		WarmPool(const WarmPool &) = delete;
		WarmPool &operator=(const WarmPool &) = delete;

		//! Called as each build starts: keeps size workers waiting, started from spec (replacing any started from a different one)
		void Prepare(const Spec &spec, size_t size);

		//! A worker for one pass of a note in folder, removed from the pool, or a new one if none is waiting there; the pool then keeps
		//! its workers waiting in folder. Nothing if the pool doesn't warm (size 0, or not on this platform), or if waiting workers have
		//! been exiting by themselves: a sign the preamble doesn't load like this, so the pool stops warming until the spec changes
		std::optional<Worker> Take(const std::filesystem::path &folder);

		//! Takes the line which makes a worker wait: it reads one line, a file name, and inputs that file
		static std::string WaitLine();

	  private:
		std::mutex Guard;
		Spec Current;
		size_t Size = 0;
		bool Broken = false;
		size_t Generation = 0; //!< bumped when the spec changes, so a new driver never overwrites one an older worker is still reading
		size_t Started = 0;
		std::deque<Worker> Waiting; //!< oldest first
		std::filesystem::path Recent; //!< the folder of the last note compiled, where workers wait
		std::chrono::steady_clock::time_point LastUsed;

		std::thread Reaper;
		std::condition_variable Wake;
		bool Stopping = false;

		std::filesystem::path DriverFile() const;
		std::optional<Worker> StartOne(const std::filesystem::path &in); //!< with Guard held
		void TopUp();					   //!< fills the recent folder; with Guard held
		void Replace(const std::filesystem::path &folder); //!< after a take; with Guard held
		void Discard(Worker &worker);	   //!< stops it, and removes its files
		void StopAll();					   //!< with Guard held
		void Reap();
	};
} // namespace Tex
