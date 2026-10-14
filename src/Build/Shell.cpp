#include "Shell.h"
#include <JSL.h>
#include <cstdlib>
#include <sys/wait.h>

namespace Shell
{
	std::string Quote(const std::filesystem::path &path)
	{
		std::string out = "'";
		for (char c : path.string())
		{
			if (c == '\'')
			{
				out += "'\\''";
			}
			else
			{
				out += c;
			}
		}
		return out + "'";
	}

	int Run(const std::string &command, const std::filesystem::path &workingDir, const std::filesystem::path &logFile)
	{
		auto full = "cd " + Quote(workingDir) + " && " + command + " > " + Quote(logFile) + " 2>&1 < /dev/null";
		LOG(DEBUG) << "Executing: " << full;
		int status = std::system(full.c_str());
		if (status == -1 || !WIFEXITED(status))
		{
			return -1;
		}
		return WEXITSTATUS(status);
	}
} // namespace Shell
