#pragma once
#include <JSL/Async.h>
#include <filesystem>
#include <string>

//! Exclusive access to a project for the lifetime of the object: constructing it acquires the lock, destroying it releases it
//! @details The lock is a named socket (in the system temp directory), bound for as long as this object exists. A crashed process leaves a stale socket, which the next lock takes over.
//! The object can neither be copied nor moved: the socket's callback runs on a monitor thread and will capture `this`, so it must never change address.
//! For explicit control over when the lock is released, hold it in a std::optional and use emplace()/reset().
class ProjectLock
{
  public:
	//! Acquires the lock for the project rooted at root
	//! @throws runtime_error If the lock cannot be acquired (most likely because another build is running on this project)
	explicit ProjectLock(const std::filesystem::path &root);

	ProjectLock(const ProjectLock &) = delete;
	ProjectLock &operator=(const ProjectLock &) = delete;
	ProjectLock(ProjectLock &&) = delete;
	ProjectLock &operator=(ProjectLock &&) = delete;

  private:
	//! A socket name unique to the project: a readable label (for humans) plus a hash of the canonical path (for uniqueness)
	static std::string SocketName(const std::filesystem::path &root);

	JSL::Async::Watcher::Socket Blocker;
};
