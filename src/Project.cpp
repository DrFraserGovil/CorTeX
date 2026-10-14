#include "Project.h"
#include "Build/Emitter.h"
#include "Build/SvgConverter.h"
#include "Build/TexRunner.h"
#include "Settings/ArchiveSettings.h"
#include <JSL.h>
#include <JSL/IO.h>
#include <map>
CortexProject::CortexProject(std::filesystem::path root) : Root(std::move(root))
{
	BuildDir = Root / ".cortex/build";
	OutputDir = Root / "view";
	StagingDir = Root / ".cortex/staging";
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
	/// visual info
	LOG(INFO) << JSL::Display::Purple(true) << "\t-- Project Recompile --\t\t" << JSL::Display::ResetAll();
	LOG(INFO) << "The entire project will now be rebuilt from scratch";
	auto tmp = JSL::Log::Indent();

	/// Clear the old staging and build directory
	std::filesystem::remove_all(StagingDir);

	auto preamble = JSL::IO::getFile(Root / ".cortex/preamble.tex");
	std::vector<Note> notes;
	std::map<std::string, std::filesystem::path> seen;
	for (auto &path : FindSources())
	{
		Note note(Root, path);
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
		LOG(WARN) << "Archive has no source files to compile";
		return;
	}
	LOG(INFO) << "Found " << notes.size() << " notes";

	size_t failures = BuildFiles(notes, preamble, true);

	if (failures < notes.size()) // i.e. if *some* files managed to compile
	{
		std::filesystem::remove_all(OutputDir);
		std::filesystem::rename(StagingDir, OutputDir);
	}
	else
	{
		LOG(ERROR) << "Catastrophic compilation failure; the project could not compile\n"
				   << "The files in " << OutputDir << " are unaffected";
		throw std::runtime_error("All files failed to compile");
	}
}

size_t CortexProject::BuildFiles(std::vector<Note> notes, std::string_view preamble, bool stageOutput)
{
	auto writeTarget = stageOutput ? StagingDir : OutputDir;
	bool pdf = Archive.PdfOutput;
	TexRunner tex(BuildDir, preamble, pdf);
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
