#include "SvgConverter.h"
#include "Shell.h"
#include <JSL.h>
#include <map>

// Returns the page number if filename is exactly <prefix><digits>.svg
static std::optional<int> PageNumber(const std::filesystem::path &file, const std::string &prefix)
{
	auto name = file.filename().string();
	if (!name.starts_with(prefix) || file.extension() != ".svg") return std::nullopt;
	auto digits = name.substr(prefix.size(), name.size() - prefix.size() - 4);
	if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos) return std::nullopt;
	return std::stoi(digits);
}

std::vector<std::string> SvgConverter::Convert(const std::filesystem::path &xdv)
{
	auto dir = xdv.parent_path();
	auto stem = xdv.stem().string();
	auto prefix = stem + "-page";

	// clear out pages from previous runs, so a note which shrinks doesn't keep its old trailing pages
	for (auto &entry : std::filesystem::directory_iterator(dir))
	{
		if (PageNumber(entry.path(), prefix))
		{
			std::filesystem::remove(entry.path());
		}
	}

	auto cmd = "dvisvgm --font-format=woff2 --page=1- --output=" + Shell::Quote(prefix + "%p.svg") + " " + Shell::Quote(xdv.filename());
	auto log = dir / (stem + ".dvisvgm.log");
	int code = Shell::Run(cmd, dir, log);
	if (code != 0)
	{
		LOG(WARN) << "dvisvgm failed on " << xdv << " (exit code " << code << "); see " << log;
		return {};
	}

	std::map<int, std::filesystem::path> pages;
	for (auto &entry : std::filesystem::directory_iterator(dir))
	{
		if (auto number = PageNumber(entry.path(), prefix))
		{
			pages[*number] = entry.path();
		}
	}

	std::vector<std::string> out;
	for (auto &[number, path] : pages)
	{
		out.push_back(JSL::IO::getFile(path));
	}
	return out;
}
