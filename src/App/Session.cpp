#include "Session.h"
#include "Build/Pipeline.h"
#include "Report/Summary.h"
#include "Util/Files.h"
#include <JSL.h>

//! One build (or rebuild), told in full to the build log and briefly to the terminal
//! @throws runtime_error If the build couldn't happen at all (missing programs, clashing ids, every note failing); the log is written first
void App::BuildOnce(const std::filesystem::path &root, bool clean, const Built &built, const Build::Options &options)
{
	LOG(INFO) << JSL::Display::Purple(true) << (clean ? "Rebuilding the whole project" : "Building") << JSL::Display::ResetAll();
	Report::Log report;
	auto logFile = root / ".cortex/build.log";
	try
	{
		auto scan = Build::Scan::Read(root);
		report.Absorb(std::move(scan.Problems));
		if (clean) Build::Rebuild(scan, report, options);
		else Build::Update(scan, report, options);
		Report::WriteLog(logFile, report, clean ? "rebuild" : "build");
		if (auto brief = Report::Brief(report, ".cortex/build.log"); !brief.empty())
		{
			LOG(WARN) << brief;
		}
		for (auto &line : report.AccountLines())
		{
			if (line.find("flagged todo") != std::string::npos || line.find("exist yet") != std::string::npos)
			{
				LOG(INFO) << line;
			}
		}
		LOG(INFO) << "Output written to " << (scan.Config.Compilation.Pdf ? scan.Paths.Output : scan.Paths.Output / "index.html");
		if (built) built(std::move(scan), report);
	}
	catch (std::exception &e)
	{
		report.Account(std::string("Stopped: ") + e.what());
		Report::WriteLog(logFile, report, clean ? "rebuild" : "build");
		throw;
	}
}

//! Copies the built output into folder, as a copy to share: no path on this machine, no editor tools, no live refresh (DESIGN.md §13.3)
void App::ExportTo(const std::filesystem::path &root, const std::filesystem::path &folder)
{
	auto scan = Build::Scan::Read(root);
	auto &paths = scan.Paths;

	/// CHECK THE DESTINATION
	auto target = std::filesystem::weakly_canonical(std::filesystem::absolute(folder));
	if (target == root || target == paths.Output || Util::IsInside(target, paths.Output) || target == paths.Cortex || Util::IsInside(target, paths.Cortex) ||
		Util::IsInside(root, target))
	{
		throw std::runtime_error("can't export into the project root, its output folder or .cortex; choose a folder of its own");
	}
	auto marker = target / ".cortex-export";
	if (std::filesystem::exists(target))
	{
		if (!std::filesystem::is_directory(target)) throw std::runtime_error(target.string() + " is a file, not a folder");
		if (!std::filesystem::is_empty(target) && !std::filesystem::exists(marker))
		{
			throw std::runtime_error(target.string() + " already has files in it which aren't an earlier export; not touching them");
		}
		// an earlier export: replaced wholesale, so notes deleted since don't linger in it
		for (auto &entry : std::filesystem::directory_iterator(target)) std::filesystem::remove_all(entry.path());
	}

	/// COPY THE OUTPUT, THEN REWRITE ITS INDEX WITHOUT ANYTHING TIED TO THIS MACHINE
	std::filesystem::create_directories(target);
	std::filesystem::copy(paths.Output, target, std::filesystem::copy_options::recursive);
	std::filesystem::remove(target / "version.js"); // live refresh is for the author's own copy
	if (!scan.Config.Compilation.Pdf)
	{
		Report::Log ignored; // the build just reported everything
		Util::WriteAtomic(target / "index.js", Build::IndexJs(scan, Build::BuildState::Load(paths.State), true, ignored));
	}
	JSL::IO::writeString(marker, "Exported by CorTeX. This file lets the next export replace this folder\n");
	LOG(INFO) << "Exported a copy to share to " << target;
}

//! Does what other commands asked of the lock's holder; returns false once one asked it to stop
bool App::Serve(const std::filesystem::path &root, Project::Lock &lock, const std::function<void(bool)> &build)
{
	while (!lock.ShutdownRequested())
	{
		if (lock.TakeRebuild()) build(true);
		else if (lock.TakeRerun()) build(false);
		auto exports = lock.TakeExports();
		for (auto &folder : exports)
		{
			try
			{
				build(false);
				ExportTo(root, folder);
			}
			catch (std::exception &e)
			{
				LOG(ERROR) << "Export to " << folder << " failed: " << e.what();
			}
		}
		if (exports.empty()) return true; // nothing more was asked during the work just done
	}
	LOG(INFO) << "Shutdown requested; stopping";
	return false;
}

// The output and .cortex are written by the build itself (watching them would rebuild forever); other hidden files are editor or
// version control noise. The project's own .cortex files are the exception
bool App::AffectsBuild(const std::filesystem::path &root, const std::filesystem::path &output, const std::filesystem::path &changed)
{
	auto file = changed.is_absolute() ? changed : root / changed;
	auto relative = file.lexically_relative(root).generic_string();
	if (relative == ".cortex/preamble.tex" || relative == ".cortex/settings.config" || relative == ".cortex/registry.config") return true;
	if (file == output || Util::IsInside(file, output)) return false;
	for (auto &part : file.lexically_relative(root))
	{
		if (part.string().starts_with(".")) return false;
	}
	return true;
}
