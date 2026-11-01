#pragma once
#include <JSL/Async.h>
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Project
{
	/*! Exclusive access to a project, for as long as the object lives (DESIGN.md §13.2), and the way other commands reach its holder
		@details The lock is a named socket in the system's temporary folder: binding it is taking the lock, and a crashed process leaves a
		stale socket which the next lock takes over. Other cortex commands send the holder a message ("rerun", "rebuild", "shutdown" or
		"export <folder>"), recorded here as requests for the holder to act on between builds.
		It can be held two ways, with one protocol: on its own (a build), when messages arrive on a monitor thread; or through a watch's
		Panopticon, when they arrive on the watch's own thread, which onMessage is then called on.
		It can neither be copied nor moved: the socket's callback captures `this`. Hold it in a std::optional for control over its lifetime
	*/
	class Lock
	{
	  public:
		//! Takes the lock on its own
		//! @throws runtime_error If another process holds it
		explicit Lock(const std::filesystem::path &root);
		//! Takes the lock through a Panopticon; onMessage runs (on the Panopticon's thread) after each request is recorded
		//! @throws runtime_error If another process holds it
		Lock(const std::filesystem::path &root, JSL::Async::Watcher::Panopticon &eye, std::function<void()> onMessage);

		Lock(const Lock &) = delete;
		Lock &operator=(const Lock &) = delete;

		/*! Takes the lock, or, if another process holds it, hands the work to that process instead
			@details There is no separate "is it locked?" check (it would be out of date the moment it returned): the attempt to take the lock
			*is* the check. If the message isn't delivered (the holder exited in between), the lock is tried once more
			@returns true if this process now holds the lock (in lock); false if the message was delivered to the holder
			@throws runtime_error If the lock can't be taken and its holder can't be reached
		*/
		static bool TakeOrSend(const std::filesystem::path &root, std::optional<Lock> &lock, std::string_view message);

		//! Sends a message to the holder, if there is one
		//! @returns true if it was delivered
		static bool Send(const std::filesystem::path &root, std::string_view message);

		//! A socket name unique to the project: a readable label (for humans) and a hash of the canonical path (for uniqueness)
		static std::string SocketName(const std::filesystem::path &root);

		//! True (once) if another command asked for an incremental build since the last call
		bool TakeRerun() { return Rerun.exchange(false); }
		//! True (once) if another command asked for a clean rebuild since the last call
		bool TakeRebuild() { return Rebuild.exchange(false); }
		//! True if another command asked the holder to stop
		bool ShutdownRequested() const { return Shutdown; }
		//! The folders other commands asked to export into since the last call (each after a build), in order
		std::vector<std::filesystem::path> TakeExports();

	  private:
		// declared before Socket, so they outlive the monitor thread which sets them
		std::atomic<bool> Rerun{false};
		std::atomic<bool> Rebuild{false};
		std::atomic<bool> Shutdown{false};
		std::mutex ExportMutex;
		std::vector<std::filesystem::path> Exports;
		std::optional<JSL::Async::Watcher::Socket> Socket;

		//! Records one message as a request; false if it isn't one
		bool Receive(std::string_view line);
	};
} // namespace Project
