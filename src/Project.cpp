#include "Project.h"
#include "Build/Emitter.h"
#include "Build/SvgConverter.h"
#include "Build/TexRunner.h"
#include "Settings/ArchiveSettings.h"
#include <JSL.h>
#include <JSL/Async.h>
#include <JSL/IO.h>
#include <map>
CortexProject::CortexProject(std::filesystem::path root) : Root(std::move(root))
{
	BuildDir = Root / ".cortex/build";
	OutputDir = Root / "view";
	StagingDir = Root / ".cortex/staging";
	PreviousDir = Root / ".cortex/previous";
}

std::set<std::filesystem::path> CortexProject::FindSources() const
{
	auto forceExclude = Archive.IgnoredFilePatterns;
	forceExclude.push_back(".cortex");
	forceExclude.push_back(OutputDir.filename());
	auto exclude = JSL::IO::multiGlobToRegex(forceExclude);
	auto include = JSL::IO::multiGlobToRegex(Archive.AllowedFilePatterns);
	auto dir = JSL::IO::Directory::Snapshot(Root, exclude);

	auto matching = dir.MatchFiles(include);
	LOG(DEBUG) << "The following files are in the archive:\n\t" << matching;

	return dir.MatchFiles(include);
}

void CortexProject::CleanRebuild()
{
	BuildLock();
	/// visual info
	LOG(INFO) << JSL::Display::Purple(true) << "\t-- Project Recompile --\t\t" << JSL::Display::ResetAll();
	LOG(INFO) << "The entire project will now be rebuilt from scratch";
	auto tmp = JSL::Log::Indent();

	/// Clear the old staging directory (and any output left over from an interrupted swap)
	std::filesystem::remove_all(StagingDir);
	std::filesystem::remove_all(PreviousDir);
	JSL::IO::mkdir(StagingDir, JSL::IO::Policy::Generous); // exists even if nothing is written into it, so an empty project still swaps

	auto preamble = JSL::IO::getFile(Root / ".cortex/preamble.tex");
	std::vector<Note> notes;
	std::map<std::string, std::filesystem::path> seen;
	for (auto &path : FindSources())
	{
		Note note(Root, path);

		// a note without a title can't be linked to or listed, so it is never built
		if (note.Title.empty())
		{
			LOG(WARN) << "Skipping " << note.RelativePath << ": it has no title. Add a header with a 'title:' line, followed by a line of five dashes";
			continue;
		}

		if (seen.contains(note.Id))
		{
			LOG(ERROR) << "Note id '" << note.Id << "' is used by both " << seen[note.Id] << " and " << path;
			throw std::runtime_error("Duplicate note id");
		}
		seen[note.Id] = path;
		notes.push_back(std::move(note));
	}

	if (notes.empty())
	{
		LOG(WARN) << "Archive has no buildable notes; the output will be empty";
	}
	else
	{
		LOG(INFO) << "Found " << notes.size() << " notes";
	}

	size_t failures = BuildFiles(notes, preamble, true);

	if (notes.empty() || failures < notes.size()) // i.e. unless every note failed (which suggests a project-wide problem)
	{
		SwapStagingIntoOutput();
	}
	else
	{
		LOG(ERROR) << "Catastrophic compilation failure; the project could not compile\n"
				   << "The files in " << OutputDir << " are unaffected";
		throw std::runtime_error("All files failed to compile");
	}
}
void CortexProject::Shutdown()
{
	Blocker.Stop();
}

void CortexProject::SwapStagingIntoOutput()
{
	/// MOVE THE OLD OUTPUT ASIDE
	// kept (not deleted) until the new output is in place, so a failed rename can be undone
	bool hadOutput = std::filesystem::exists(OutputDir);
	if (hadOutput)
	{
		std::filesystem::rename(OutputDir, PreviousDir);
	}

	/// MOVE THE NEW OUTPUT IN
	try
	{
		std::filesystem::rename(StagingDir, OutputDir);
	}
	catch (std::exception &e)
	{
		if (hadOutput)
		{
			std::filesystem::rename(PreviousDir, OutputDir);
		}
		LOG(ERROR) << "Could not move the new output into " << OutputDir << " (" << e.what() << "); the previous output has been restored";
		throw;
	}

	/// DISCARD THE OLD OUTPUT
	std::filesystem::remove_all(PreviousDir);
}

size_t CortexProject::BuildFiles(std::vector<Note> notes, std::string_view preamble, bool stageOutput)
{
	auto writeTarget = stageOutput ? StagingDir : OutputDir;
	bool pdf = Archive.PdfOutput;
	TexRunner tex(BuildDir, preamble, pdf, Archive.RenderWidthCM);
	Emitter emitter(writeTarget);

	size_t failures = 0;
	for (auto &note : notes)
	{
		LOG(INFO) << "Compiling " << note.RelativePath;
		auto output = tex.Compile(note);
		if (!output)
		{
			++failures;
			continue;
		}

		if (pdf)
		{
			emitter.WritePdf(note, *output);
			continue;
		}

		auto svg = Svg::Convert(*output);
		if (!svg)
		{
			++failures;
			continue;
		}
		emitter.WriteNote(note, {*svg});
	}
	// PDF mode produces only the PDFs themselves; the index and viewer exist only for SVG output
	if (!pdf)
	{
		emitter.WriteIndex(notes);
	}

	if (failures > 0)
	{
		LOG(WARN) << failures << " of " << notes.size() << " notes failed to build";
	}
	LOG(INFO) << "Output written to " << (pdf ? writeTarget : writeTarget / "index.html");
	return failures;
}
void CortexProject::BuildLock()
{
	auto name = CanonicaliseName();
	// Watcher.SetSocketCallback(name, [this](std::string line) {
	// 	LOG(INFO) << "Message recieved on blocking socket:\n"
	// 			  << line;
	// });
	// Watcher.Start();

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
}

std::string CortexProject::CanonicaliseName()
{
	// canonical: resolves symlinks, '..' and trailing slashes, so every route to the same folder agrees
	auto key = std::filesystem::canonical(Root).string();

	// FNV-1a (64-bit): tiny, and gives the same answer in every build, on every compiler
	uint64_t hash = 14695981039346656037ull;
	for (unsigned char c : key)
	{
		hash ^= c;
		hash *= 1099511628211ull;
	}

	return std::format("cortex-{}-{:016x}", Root.filename().string(), hash);
}
