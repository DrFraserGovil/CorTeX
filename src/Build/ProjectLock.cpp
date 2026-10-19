#include "ProjectLock.h"
#include "../Utility/Hash.h"
#include "../Utility/Sanitise.h"
#include <JSL.h>
#include <format>

ProjectLock::ProjectLock(const std::filesystem::path &root, bool quiet)
{
	auto name = SocketName(root);
	try
	{
		// runs on the monitor thread: it only records requests, which the holder acts on between builds
		Blocker.Initialise(name, [this](std::string_view line) {
			auto message = JSL::String::trim(line);
			if (message == "rerun") Rerun = true;
			else if (message == "rebuild") Rebuild = true;
			else if (message == "shutdown") Shutdown = true;
			else
			{
				LOG(WARN) << "Ignoring unknown message '" << message << "' on the project lock";
				return;
			}
			LOG(INFO) << "Another cortex command requested a " << message << "; it will happen when the current build finishes";
		});
		Blocker.Start();
	}
	catch (std::exception &e)
	{
		if (!quiet)
		{
			LOG(ERROR) << JSL::Display::Yellow() << "Could not acquire a lock for this project\nThe most likely cause is another build process in process.\nFull socket diagnostic below\n";
		}
		throw std::runtime_error(e.what());
	}
	LOG(DEBUG) << "Acquired project lock " << name;
}

bool ProjectLock::AcquireOrSend(const std::filesystem::path &root, std::optional<ProjectLock> &lock, std::string_view message)
{
	// two rounds: if the holder exits between our failed bind and our message, the second bind succeeds
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		try
		{
			lock.emplace(root, true);
			return true;
		}
		catch (std::exception &)
		{
			// held by someone else: fall through and hand them the work
		}

		try
		{
			if (JSL::Async::Socket::Transmit(SocketName(root), message))
			{
				return false;
			}
		}
		catch (std::exception &)
		{
			// nobody answered: the holder has gone, so try the lock again
		}
	}
	LOG(ERROR) << "Could not take the project lock, or reach the process holding it";
	throw std::runtime_error("Project lock unavailable");
}

bool ProjectLock::TakeRerun()
{
	return Rerun.exchange(false);
}

bool ProjectLock::TakeRebuild()
{
	return Rebuild.exchange(false);
}

bool ProjectLock::ShutdownRequested() const
{
	return Shutdown;
}

std::string ProjectLock::SocketName(const std::filesystem::path &root)
{
	// canonical: resolves symlinks, '..' and trailing slashes, so every route to the same folder agrees
	auto key = std::filesystem::canonical(root).string();

	auto hash = Utility::Hash(key);

	// the label is only for recognising the socket in a directory listing; uniqueness comes from the hash, so it can be safely shortened
	auto label = Utility::Sanitise(root.filename().string());
	if (label.size() > 32)
	{
		label = label.substr(0, 32);
	}
	return std::format("cortex-{}-{:016x}", label, hash);
}
