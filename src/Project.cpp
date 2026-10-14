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
}

std::set<std::filesystem::path> CortexProject::FindSources() const
{
	auto forceExclude = Archive.IgnoredFilePatterns;
	forceExclude.push_back(".cortex");
	forceExclude.push_back(OutputDir);
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
	LOG(INFO) << "Found " << notes.size() << " notes";

	BuildFiles(notes, preamble);
}

void CortexProject::BuildFiles(std::vector<Note> notes, std::string_view preamble)
{
	bool pdf = Archive.PdfOutput;
	TexRunner tex(BuildDir, preamble, pdf);
	Emitter emitter(OutputDir);

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
	emitter.WriteIndex(notes, pdf);

	if (failures > 0)
	{
		LOG(WARN) << failures << " of " << notes.size() << " notes failed to build";
	}
	LOG(INFO) << "Output written to " << OutputDir / "index.html";
}
