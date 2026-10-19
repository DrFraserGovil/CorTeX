#pragma once
#include <JSL/Async.h>
#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

//! Exclusive access to a project for the lifetime of the object: constructing it acquires the lock, destroying it releases it
//! @details The lock is a named socket (in the system temp directory), bound for as long as this object exists. A crashed process leaves a stale socket, which the next lock takes over.
//! The socket is also how other cortex commands reach the holder: they send "rerun", "rebuild" or "shutdown", which are recorded as requests for the holder to act on between builds.
//! The object can neither be copied nor moved: the socket's callback runs on a monitor thread and captures `this`, so it must never change address.
//! For explicit control over when the lock is released, hold it in a std::optional and use emplace()/reset().
class ProjectLock
{
  public:
	//! Acquires the lock for the project rooted at root
	//! @param quiet If true, a failure is not logged (for callers with a fallback, such as AcquireOrSend)
	//! @throws runtime_error If the lock cannot be acquired (most likely because another build is running on this project)
	explicit ProjectLock(const std::filesystem::path &root, bool quiet = false);

	ProjectLock(const ProjectLock &) = delete;
	ProjectLock &operator=(const ProjectLock &) = delete;
	ProjectLock(ProjectLock &&) = delete;
	ProjectLock &operator=(ProjectLock &&) = delete;

	//! A socket name unique to the project: a readable label (for humans) plus a hash of the canonical path (for uniqueness)
	//! @details Public so that `cortex watch` can hold the lock through its own socket watcher, and other commands can message it
	static std::string SocketName(const std::filesystem::path &root);

	/*! Takes the project lock, or, if another process holds it, hands the work to that process instead
		@details There is no separate "is it locked?" check (it would be out of date the moment it returned): the attempt to take the lock *is* the check.
		If it fails, the message is sent to the holder; if that isn't acknowledged (the holder exited in between), the lock is tried once more.
		@param lock Emplaced with the lock if this process takes it
		@param message What to ask the holder to do: "rerun", "rebuild" or "shutdown"
		@returns true if this process now holds the lock (in `lock`); false if the message was delivered to the holder
		@throws runtime_error If the lock can't be taken and the holder can't be reached
	*/
	static bool AcquireOrSend(const std::filesystem::path &root, std::optional<ProjectLock> &lock, std::string_view message);

	//! True (once) if another command asked for an incremental build since the last call
	bool TakeRerun();
	//! True (once) if another command asked for a clean rebuild since the last call
	bool TakeRebuild();
	//! True if another command asked this process to stop (after its current build)
	bool ShutdownRequested() const;

  private:
	// declared before Blocker, so they outlive the monitor thread which sets them
	std::atomic<bool> Rerun{false};
	std::atomic<bool> Rebuild{false};
	std::atomic<bool> Shutdown{false};

	JSL::Async::Watcher::Socket Blocker;
};
