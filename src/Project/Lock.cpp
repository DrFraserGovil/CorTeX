#include "Lock.h"
#include "Util/Text.h"
#include <JSL.h>
#include <format>
#include <utility>

bool Project::Lock::Receive(std::string_view line)
{
	auto message = JSL::String::trim(line);
	if (message == "rerun") Rerun = true;
	else if (message == "rebuild") Rebuild = true;
	else if (message == "shutdown") Shutdown = true;
	else if (message.starts_with("export "))
	{
		std::lock_guard guard(ExportMutex);
		Exports.push_back(std::string(message.substr(7)));
	}
	else
	{
		LOG(WARN) << "Ignoring unknown message '" << message << "' on the project lock";
		return false;
	}
	LOG(INFO) << "Another cortex command asked for: " << message;
	return true;
}

Project::Lock::Lock(const std::filesystem::path &root)
{
	Socket.emplace();
	// runs on the monitor thread: it only records requests, which the holder acts on between builds
	Socket->Initialise(SocketName(root), [this](std::string_view line) { Receive(line); });
	Socket->Start();
}

Project::Lock::Lock(const std::filesystem::path &root, JSL::Async::Watcher::Panopticon &eye, std::function<void()> onMessage)
{
	eye.SetSocketCallback(SocketName(root), [this, onMessage](std::string line) {
		if (Receive(line)) onMessage();
	});
}

bool Project::Lock::Send(const std::filesystem::path &root, std::string_view message)
{
	try
	{
		return JSL::Async::Socket::Transmit(SocketName(root), std::string(message));
	}
	catch (std::exception &)
	{
		return false;
	}
}

bool Project::Lock::TakeOrSend(const std::filesystem::path &root, std::optional<Lock> &lock, std::string_view message)
{
	// two rounds: if the holder exits between our failed bind and our message, the second bind succeeds
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		try
		{
			lock.emplace(root);
			return true;
		}
		catch (std::exception &)
		{
			lock.reset(); // held by someone else: hand them the work
		}
		if (Send(root, message)) return false;
	}
	throw std::runtime_error("could not take the project lock, or reach the process holding it");
}

std::string Project::Lock::SocketName(const std::filesystem::path &root)
{
	// canonical: resolves symlinks, '..' and trailing slashes, so every route to the same folder agrees
	auto hash = Util::Hash(std::filesystem::canonical(root).string());
	// the label is only for recognising the socket in a listing; uniqueness comes from the hash, so it can be safely shortened
	auto label = Util::Sanitise(root.filename().string()).substr(0, 32);
	return std::format("cortex-{}-{:016x}", label, hash);
}

std::vector<std::filesystem::path> Project::Lock::TakeExports()
{
	std::lock_guard guard(ExportMutex);
	return std::exchange(Exports, {});
}
