#include "ProjectLock.h"
#include "../Utility/Sanitise.h"
#include <JSL.h>
#include <format>

ProjectLock::ProjectLock(const std::filesystem::path &root)
{
	auto name = SocketName(root);
	try
	{
		Blocker.Initialise(name, [](std::string_view line) {
			LOG(INFO) << "Message recieved on blocking socket:\n"
					  << line;
		});
		Blocker.Start();
	}
	catch (std::exception &e)
	{
		LOG(ERROR) << JSL::Display::Yellow() << "Could not acquire a lock for this project\nThe most likely cause is another build process in process.\nFull socket diagnostic below\n";
		throw std::runtime_error(e.what());
	}
	LOG(DEBUG) << "Acquired project lock " << name;
}

std::string ProjectLock::SocketName(const std::filesystem::path &root)
{
	// canonical: resolves symlinks, '..' and trailing slashes, so every route to the same folder agrees
	auto key = std::filesystem::canonical(root).string();

	// FNV-1a (64-bit): tiny, and gives the same answer in every build, on every compiler
	uint64_t hash = 14695981039346656037ull;
	for (unsigned char c : key)
	{
		hash ^= c;
		hash *= 1099511628211ull;
	}

	// the label is only for recognising the socket in a directory listing; uniqueness comes from the hash, so it can be safely shortened
	auto label = Utility::Sanitise(root.filename().string());
	if (label.size() > 32)
	{
		label = label.substr(0, 32);
	}
	return std::format("cortex-{}-{:016x}", label, hash);
}
