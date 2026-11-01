#include "Summary.h"
#include "Util/Files.h"
#include <chrono>
#include <format>

namespace
{
	constexpr Report::Kind Order[] = {Report::Kind::Failure,	  Report::Kind::Setting,	  Report::Kind::Note,  Report::Kind::Syntax,
									  Report::Kind::Transclusion, Report::Kind::SharedName, Report::Kind::Redirect, Report::Kind::Sidebar,
									  Report::Kind::Label,		  Report::Kind::Skipped};

	std::string Count(size_t n, const std::string &what)
	{
		return std::to_string(n) + " " + what + (n == 1 ? "" : "s");
	}
} // namespace

void Report::WriteLog(const std::filesystem::path &file, const Log &log, const std::string &title)
{
	auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	std::string text = "CorTeX " + title + ", " + std::format("{:%Y-%m-%d %H:%M:%S}", now) + "\n\n";
	for (auto &line : log.AccountLines()) text += line + "\n";
	for (auto kind : Order)
	{
		auto entries = log.Of(kind);
		if (entries.empty()) continue;
		text += "\n" + Count(entries.size(), Describe(kind)) + ":\n";
		for (auto &entry : entries) text += "  " + Format(entry) + "\n";
	}
	std::filesystem::create_directories(file.parent_path());
	Util::WriteAtomic(file, text);
}

std::string Report::Brief(const Log &log, const std::filesystem::path &logFile)
{
	std::string text;
	for (auto &failure : log.Of(Kind::Failure)) text += "Failed: " + Format(failure) + "\n";
	std::vector<std::string> counts;
	for (auto kind : Order)
	{
		if (kind == Kind::Failure) continue;
		if (auto n = log.Count(kind); n > 0) counts.push_back(Count(n, Describe(kind)));
	}
	if (!counts.empty())
	{
		text += "Also: ";
		for (size_t i = 0; i < counts.size(); ++i) text += (i > 0 ? ", " : "") + counts[i];
		text += "\n";
	}
	if (!text.empty()) text += "Details: " + logFile.string();
	return text;
}
