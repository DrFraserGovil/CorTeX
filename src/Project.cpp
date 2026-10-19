#include "Project.h"
#include "Build/Emitter.h"
#include "Build/Index.h"
#include "Build/ProjectLock.h"
#include "Build/SvgConverter.h"
#include "Build/TexRunner.h"
#include "Settings/ArchiveSettings.h"
#include "Utility/Hash.h"
#include <JSL.h>
#include <JSL/Async.h>
#include <JSL/IO.h>
#include <JSL/Strings/Stitch.h>
#include <algorithm>
#include <map>
CortexProject::CortexProject(std::filesystem::path root) : Root(std::move(root))
{
	BuildDir = Root / ".cortex/build";
	OutputDir = Root / "view";
	StagingDir = Root / ".cortex/staging";
	PreviousDir = Root / ".cortex/previous";
	BuildStateFile = Root / ".cortex/buildstate.tsv";
}

std::set<std::filesystem::path> CortexProject::FindSources() const
{
	auto forceExclude = Archive.Files.IgnoredFilePatterns;
	forceExclude.push_back(".cortex");
	forceExclude.push_back(OutputDir.filename());
	auto exclude = JSL::IO::multiGlobToRegex(forceExclude);
	auto include = JSL::IO::multiGlobToRegex(Archive.Files.AllowedFilePatterns);
	auto dir = JSL::IO::Directory::Snapshot(Root, exclude);

	auto matching = dir.MatchFiles(include);
	LOG(DEBUG) << "The following files are in the archive:\n\t" << matching;

	return dir.MatchFiles(include);
}

std::vector<Note> CortexProject::ScanNotes() const
{
	std::vector<Note> notes;
	std::map<std::string, std::filesystem::path> seen;
	auto syntax = SyntaxOptions();
	for (auto &path : FindSources())
	{
		Note note(Root, path, syntax);

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
	return notes;
}

std::string CortexProject::ProjectName() const
{
	// registry.ctx holds "name = <name>", written by cortex new
	auto registry = Root / ".cortex/registry.ctx";
	if (std::filesystem::exists(registry))
	{
		for (auto &line : JSL::IO::getFileLines(registry))
		{
			auto eq = line.find('=');
			if (eq != std::string::npos && JSL::String::trim(std::string_view(line).substr(0, eq)) == "name")
			{
				return JSL::String::trim(std::string_view(line).substr(eq + 1));
			}
		}
	}
	return Root.filename().string();
}

std::string CortexProject::GlobalHash() const
{
	// '\0' separates the parts, so that moving text from one part to the next can't produce the same hash
	std::string key = JSL::IO::getFile(Root / ".cortex/preamble.tex");
	key += '\0' + std::string(TexRunner::ClassSource());
	key += '\0' + JSL::String::makeFrom(Archive.Render.WidthCM);
	key += '\0' + std::string(Archive.Render.PdfOutput ? "pdf" : "svg");
	key += '\0' + std::to_string(EngineVersion);
	// the extended syntax and title change the generated TeX of every note
	key += '\0' + std::string(Archive.Syntax.ExtendedSyntax ? "extended" : "plain") + '\0' + Archive.Syntax.DefaultListType;
	key += '\0' + std::string(Archive.Render.InsertTitle ? "title" : "untitled");
	return Utility::HashHex(key);
}

Syntax::Options CortexProject::SyntaxOptions()
{
	Syntax::Options options;
	options.Extended = Archive.Syntax.ExtendedSyntax;
	options.ListType = Archive.Syntax.DefaultListType;
	if (options.ListType != "itemize" && options.ListType != "enumerate")
	{
		LOG(WARN) << "list-type '" << options.ListType << "' is not 'itemize' or 'enumerate'; using itemize";
		options.ListType = "itemize";
	}
	return options;
}

std::string CortexProject::NoteHash(const Note &note) const
{
	// the header isn't included: a new title changes the index, but not the compiled note
	std::string key = note.LocalPreamble + '\0' + note.Body;
	if (auto targets = PdfLinkTargets.find(note.Id); targets != PdfLinkTargets.end())
	{
		for (auto &[slug, path] : targets->second)
		{
			key += '\0' + slug + '\0' + path;
		}
	}
	return Utility::HashHex(key);
}

void CortexProject::ResolvePdfLinks(const std::vector<Note> &notes)
{
	PdfLinkTargets.clear();
	if (!Archive.Render.PdfOutput) return;

	// resolution only needs the scan, not build results, so it can happen before anything compiles
	Index index(notes, BuildState{});
	std::map<std::string, std::filesystem::path> pdfOf;
	for (auto &entry : index.Entries())
	{
		pdfOf[entry.Id] = std::filesystem::path(entry.Path).replace_extension(".pdf");
	}
	for (auto &entry : index.Entries())
	{
		// relative paths are computed under a common dummy root, so a note at the top level (empty folder) works too
		auto from = ("/root" / std::filesystem::path(entry.Path)).parent_path();
		auto &targets = PdfLinkTargets[entry.Id];
		for (auto &link : entry.Links)
		{
			if (link.Target.empty()) continue;
			targets.push_back({link.Slug, ("/root" / pdfOf[link.Target]).lexically_relative(from).generic_string()});
		}
	}
}

bool CortexProject::AffectsBuild(const std::filesystem::path &relative) const
{
	auto path = relative.lexically_normal().generic_string();
	if (path == ".cortex/preamble.tex" || path == ".cortex/settings.config")
	{
		return true;
	}
	for (auto &part : relative)
	{
		// the output and .cortex are written by the build itself (watching them would rebuild forever); other hidden files are editor or VCS noise
		if (part == OutputDir.filename() || part.string().starts_with("."))
		{
			return false;
		}
	}
	return true;
}

void CortexProject::Watch()
{
	LOG(INFO) << JSL::Display::Purple(true) << "\t-- Watching Project --\t\t" << JSL::Display::ResetAll();
	LOG(INFO) << "Rebuilding whenever the project changes. Type q and Enter (or run 'cortex shutdown') to stop";

	JSL::Async::Watcher::Panopticon eye;

	// one build (incremental, or clean on request), which never takes the watcher down with it: errors are reported, and the next change tries again.
	// The settings are re-read first every time: another command may have just saved new ones, and its message can arrive before the file event
	auto build = [&](bool clean) {
		try
		{
			Archive.Reset(); // from scratch, so a setting removed from the file returns to its default
			Archive.Configure(Root / ".cortex/settings.config", " ");
			if (clean)
			{
				CleanRebuildLocked();
			}
			else
			{
				BuildLocked();
			}
		}
		catch (std::exception &e)
		{
			LOG(ERROR) << "Build failed: " << e.what() << "\nStill watching for changes";
		}
	};

	/// THE LOCK, AND MESSAGES FROM OTHER CORTEX COMMANDS
	// registering the socket binds it, which is the project lock: it fails here if another build or watch is running
	try
	{
		eye.SetSocketCallback(ProjectLock::SocketName(Root), [&](std::string message) {
			message = JSL::String::trim(message);
			if (message == "shutdown")
			{
				LOG(INFO) << "Shutdown requested";
				eye.Stop();
			}
			else if (message == "rerun")
			{
				build(false);
			}
			else if (message == "rebuild")
			{
				build(true);
			}
			else
			{
				LOG(WARN) << "Ignoring unknown message '" << message << "'";
			}
		});
	}
	catch (std::exception &e)
	{
		LOG(ERROR) << "Could not acquire a lock for this project; another build or watch is probably running";
		throw;
	}

	/// FILE CHANGES
	// callbacks run on this (the main) thread, one batch at a time: changes made during a build queue up and trigger the next one
	eye.SetFileBatchCallback(Root, true, [&](std::set<JSL::Async::Watcher::FileChange> batch) {
		for (auto &change : batch)
		{
			auto relative = change.Path.is_absolute() ? change.Path.lexically_relative(Root) : change.Path;
			if (!AffectsBuild(relative)) continue;

			LOG(INFO) << "Change detected: " << relative.generic_string();
			build(false);
			return; // one build covers the whole batch
		}
	});
	eye.SetDebounceTime(300); // editors often save as several quick writes; one build per save

	/// THE TERMINAL
	eye.SetInputCallback([&](std::string line) {
		if (JSL::String::trim(line) == "q") eye.Stop();
	});

	build(false); // bring the output up to date before waiting for changes
	eye.Start();
	LOG(INFO) << "Stopped watching";
}

std::vector<std::pair<std::string, std::string>> CortexProject::ProjectDependencies(const std::set<std::filesystem::path> &inputs) const
{
	std::vector<std::pair<std::string, std::string>> dependencies;
	for (auto &file : inputs)
	{
		// files outside the project are the TeX installation: tracking every package would be slow, and they rarely change
		auto relative = file.lexically_relative(Root);
		if (relative.empty() || *relative.begin() == "..")
		{
			continue;
		}
		// .cortex holds the generated wrapper, .aux files and cortex.cls (covered by the global hash); the output is never an input
		auto top = *relative.begin();
		if (top == ".cortex" || top == OutputDir.filename())
		{
			continue;
		}
		dependencies.push_back({relative.generic_string(), Utility::HashHex(JSL::IO::getFile(file))});
	}
	return dependencies;
}

bool CortexProject::DependenciesChanged(const NoteRecord &record) const
{
	for (auto &[path, hash] : record.Dependencies)
	{
		auto file = Root / path;
		if (!std::filesystem::is_regular_file(file) || Utility::HashHex(JSL::IO::getFile(file)) != hash)
		{
			LOG(DEBUG) << record.Path << " depends on " << path << ", which has changed";
			return true;
		}
	}
	return false;
}

void CortexProject::WarnAboutInputs(const Note &note, const std::vector<std::pair<std::string, std::string>> &dependencies) const
{
	for (auto &[path, hash] : dependencies)
	{
		auto file = Root / path;
		if (file.extension() != ".tex") continue; // packages, images, bibliographies: plain TeX's business

		if (Note::HasNoteHeader(file))
		{
			LOG(WARN) << note.RelativePath.generic_string() << " uses \\input on " << path << ", which is a CorTeX note: its header (title, separator line) will be typeset as text";
		}
		else if (!Syntax::Process(JSL::IO::getFile(file)).Links.empty())
		{
			LOG(WARN) << path << " (\\input by " << note.RelativePath.generic_string() << ") contains [[links]], which are only processed in notes: in an \\input file they appear as literal brackets";
		}
	}
}

void CortexProject::Build()
{
	// held until this function returns or throws; if another process (a watch, or another build) holds it, that process is asked to rebuild instead
	std::optional<ProjectLock> lock;
	if (!ProjectLock::AcquireOrSend(Root, lock, "rerun"))
	{
		LOG(INFO) << "Another CorTeX process (a watch or build) is working on this project; it has been asked to rebuild";
		return;
	}
	RunHoldingLock(*lock, false);
}

void CortexProject::RunHoldingLock(ProjectLock &lock, bool clean)
{
	// requests from other commands arrive during a build; act on them before releasing the lock, so none are lost
	while (true)
	{
		if (clean)
		{
			CleanRebuildLocked();
		}
		else
		{
			BuildLocked();
		}

		if (lock.ShutdownRequested())
		{
			LOG(INFO) << "Shutdown requested; stopping";
			return;
		}
		if (lock.TakeRebuild())
		{
			clean = true;
		}
		else if (lock.TakeRerun())
		{
			clean = false;
		}
		else
		{
			return;
		}
	}
}

void CortexProject::Shutdown()
{
	// no lock needed: this only talks to whoever holds it
	try
	{
		if (JSL::Async::Socket::Transmit(ProjectLock::SocketName(Root), "shutdown"))
		{
			LOG(INFO) << "Asked the running CorTeX process to stop (a watch stops at once; a build stops after its current build)";
			return;
		}
	}
	catch (std::exception &)
	{
	}
	LOG(INFO) << "No CorTeX process is running on this project";
}

void CortexProject::BuildLocked()
{
	/// visual info
	LOG(INFO) << JSL::Display::Purple(true) << "\t-- Project Build --\t\t" << JSL::Display::ResetAll();
	auto tmp = JSL::Log::Indent();

	/// READ THE PROJECT AND WHAT THE LAST BUILD DID
	auto notes = ScanNotes();
	auto state = BuildState::Load(BuildStateFile);
	auto preamble = JSL::IO::getFile(Root / ".cortex/preamble.tex");
	auto global = GlobalHash();
	bool pdf = Archive.Render.PdfOutput;
	Emitter output(OutputDir);
	ResolvePdfLinks(notes);

	bool globalChanged = (state.GlobalHash != global);
	if (globalChanged && !state.Records.empty())
	{
		LOG(INFO) << "Project-wide inputs have changed (preamble, settings or CorTeX version); every note will be recompiled";
	}

	/// DECIDE WHICH NOTES NEED COMPILING
	std::vector<Note> dirty;
	for (auto &note : notes)
	{
		auto record = state.Records.find(note.Id);
		bool isDirty = globalChanged											 // something every note depends on has changed
					   || record == state.Records.end()							 // never built
					   || record->second.Hash != NoteHash(note)					 // edited since it was last built
					   || !record->second.Succeeded								 // failed last time: try again
					   || DependenciesChanged(record->second)					 // an \input file, local package or image has changed
					   || !std::filesystem::exists(output.OutputFor(note, pdf)); // output deleted (or moved, in PDF mode)
		if (isDirty)
		{
			dirty.push_back(note);
		}
	}
	LOG(INFO) << dirty.size() << " of " << notes.size() << " notes need compiling";

	/// COMPILE THEM, STRAIGHT INTO THE OUTPUT
	// no staging: each output is replaced atomically, and a failure leaves the last good version in place
	if (!dirty.empty())
	{
		BuildFiles(dirty, preamble, OutputDir, state);
	}

	/// RECORD, INDEX AND TIDY UP
	state.GlobalHash = global;
	std::set<std::string> ids;
	for (auto &note : notes)
	{
		ids.insert(note.Id);
	}
	state.RetainOnly(ids);

	WriteIndex(notes, state, OutputDir);
	SweepOutput(notes);
	SweepScratch(notes);
	state.Save(BuildStateFile);
	LOG(INFO) << "Output written to " << (pdf ? OutputDir : OutputDir / "index.html");
}

void CortexProject::CleanRebuild()
{
	// held until this function returns or throws; if another process (a watch, or another build) holds it, that process is asked to rebuild from scratch instead
	std::optional<ProjectLock> lock;
	if (!ProjectLock::AcquireOrSend(Root, lock, "rebuild"))
	{
		LOG(INFO) << "Another CorTeX process (a watch or build) is working on this project; it has been asked to rebuild from scratch";
		return;
	}
	RunHoldingLock(*lock, true);
}

void CortexProject::CleanRebuildLocked()
{
	/// visual info
	LOG(INFO) << JSL::Display::Purple(true) << "\t-- Project Recompile --\t\t" << JSL::Display::ResetAll();
	LOG(INFO) << "The entire project will now be rebuilt from scratch";
	auto tmp = JSL::Log::Indent();

	/// Clear the old staging directory (and any output left over from an interrupted swap)
	std::filesystem::remove_all(StagingDir);
	std::filesystem::remove_all(PreviousDir);
	JSL::IO::mkdir(StagingDir, JSL::IO::Policy::Generous); // exists even if nothing is written into it, so an empty project still swaps

	auto preamble = JSL::IO::getFile(Root / ".cortex/preamble.tex");
	auto notes = ScanNotes();
	ResolvePdfLinks(notes);

	// a fresh state: nothing from earlier builds survives a rebuild
	BuildState state;
	state.GlobalHash = GlobalHash();
	size_t failures = BuildFiles(notes, preamble, StagingDir, state);
	WriteIndex(notes, state, StagingDir);

	if (notes.empty() || failures < notes.size()) // i.e. unless every note failed (which suggests a project-wide problem)
	{
		SwapStagingIntoOutput();
		SweepScratch(notes);
		state.Save(BuildStateFile);
		LOG(INFO) << "Output written to " << (Archive.Render.PdfOutput ? OutputDir : OutputDir / "index.html");
	}
	else
	{
		LOG(ERROR) << "Catastrophic compilation failure; the project could not compile\n"
				   << "The files in " << OutputDir << " are unaffected";
		throw std::runtime_error("All files failed to compile");
	}
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

size_t CortexProject::BuildFiles(const std::vector<Note> &notes, std::string_view preamble, const std::filesystem::path &targetDir, BuildState &state)
{
	bool pdf = Archive.Render.PdfOutput;
	TexRunner tex(BuildDir, preamble, pdf, Archive.Render.WidthCM, Archive.Render.InsertTitle);
	Emitter emitter(targetDir);

	size_t failures = 0;
	for (auto &note : notes)
	{
		LOG(INFO) << "Compiling " << note.RelativePath;
		NoteRecord record{note.Id, note.RelativePath.generic_string(), NoteHash(note), false, false, "", {}};

		/// COMPILE, AND CONVERT IF NEEDED
		auto targets = PdfLinkTargets.find(note.Id);
		auto result = (targets != PdfLinkTargets.end()) ? tex.Compile(note, targets->second) : tex.Compile(note);
		std::string error = result.Error;
		if (result.Output && pdf)
		{
			emitter.WritePdf(note, *result.Output);
		}
		else if (result.Output)
		{
			auto svg = Svg::Convert(*result.Output);
			if (svg)
			{
				emitter.WriteNote(note, {*svg});
			}
			else
			{
				error = "dvisvgm could not convert the compiled note (see its .dvisvgm.log in .cortex/build)";
			}
		}

		/// RECORD THE OUTCOME
		record.Succeeded = error.empty();
		record.Diagnostic = error;
		// only a successful compile's inputs are meaningful; a failed note is retried regardless
		record.Dependencies = record.Succeeded ? ProjectDependencies(result.Inputs) : std::vector<std::pair<std::string, std::string>>{};
		WarnAboutInputs(note, record.Dependencies);
		// after a failure, any existing output is the last good version, and stays in place
		record.HasOutput = std::filesystem::exists(emitter.OutputFor(note, pdf));
		if (!record.Succeeded)
		{
			++failures;
		}
		state.Records[note.Id] = record;
	}

	if (failures > 0)
	{
		LOG(WARN) << failures << " of " << notes.size() << " notes failed to build";
	}
	return failures;
}

void CortexProject::WriteIndex(const std::vector<Note> &notes, const BuildState &state, const std::filesystem::path &targetDir)
{
	Index index(notes, state);

	/// REPORT ON THE LINK NETWORK
	for (auto &[name, ids] : index.Clashes())
	{
		LOG(WARN) << "'" << name << "' is the title or alias of several notes (" << JSL::String::stitch(ids, 0, ids.size(), ", ") << "); links to it go to whichever is nearest the linking note";
	}
	if (auto unresolved = index.UnresolvedCount(); unresolved > 0)
	{
		LOG(INFO) << unresolved << (unresolved == 1 ? " link points at a note which doesn't" : " links point at notes which don't") << " exist yet";
	}

	// PDF mode produces only the PDFs themselves; the index and viewer exist only for SVG output
	if (Archive.Render.PdfOutput)
	{
		return;
	}
	Emitter::ViewerConfig viewer;
	viewer.Zoom = Archive.Render.ViewerZoom;
	if (!(viewer.Zoom > 0))
	{
		LOG(WARN) << "zoom must be a positive number (1 = true size); using 1";
		viewer.Zoom = 1;
	}
	viewer.Name = ProjectName();

	// the home note is given as a path, matched against the notes actually being built (so ignored or untitled files can't be home)
	if (!Archive.Files.HomeNote.empty())
	{
		auto wanted = std::filesystem::path(Archive.Files.HomeNote).lexically_normal().generic_string();
		for (auto &note : notes)
		{
			if (note.RelativePath.generic_string() == wanted) viewer.Home = note.Id;
		}
		if (viewer.Home.empty())
		{
			LOG(WARN) << "home '" << Archive.Files.HomeNote << "' isn't a note in this project (missing, ignored, or without a title); the viewer will open on the overview";
		}
	}
	Emitter(targetDir).WriteIndex(index.Serialise(), viewer);
}

// Removes now-empty directories under (but not including) root, deepest first
static void RemoveEmptyDirectories(const std::filesystem::path &root)
{
	std::vector<std::filesystem::path> dirs;
	for (auto &entry : std::filesystem::recursive_directory_iterator(root))
	{
		if (entry.is_directory()) dirs.push_back(entry.path());
	}
	std::sort(dirs.begin(), dirs.end(), [](auto &a, auto &b) { return a.string().size() > b.string().size(); });
	for (auto &dir : dirs)
	{
		if (std::filesystem::is_empty(dir)) std::filesystem::remove(dir);
	}
}

void CortexProject::SweepOutput(const std::vector<Note> &notes)
{
	if (!std::filesystem::exists(OutputDir)) return;

	/// LIST WHAT SHOULD BE THERE
	// the same function the writers use, so the sweep can never disagree with them about where outputs go
	bool pdf = Archive.Render.PdfOutput;
	Emitter output(OutputDir);
	std::set<std::filesystem::path> expected;
	for (auto &note : notes)
	{
		expected.insert(output.OutputFor(note, pdf));
	}
	if (!pdf)
	{
		for (auto &file : output.ViewerFiles()) expected.insert(file);
	}

	/// DELETE EVERYTHING ELSE
	// deleted or renamed notes, the other mode's files after a mode switch, temporary files from an interrupted write
	std::vector<std::filesystem::path> unexpected;
	for (auto &entry : std::filesystem::recursive_directory_iterator(OutputDir))
	{
		if (entry.is_regular_file() && !expected.contains(entry.path())) unexpected.push_back(entry.path());
	}
	for (auto &file : unexpected)
	{
		LOG(DEBUG) << "Sweeping " << file;
		std::filesystem::remove(file);
	}
	RemoveEmptyDirectories(OutputDir);
}

void CortexProject::SweepScratch(const std::vector<Note> &notes)
{
	if (!std::filesystem::exists(BuildDir)) return;

	std::set<std::string> ids;
	for (auto &note : notes)
	{
		ids.insert(note.Id);
	}

	// (cortex.cls lives in .cortex/class, outside BuildDir, so it is never swept)
	// a scratch file belongs to a note if stripping its extensions leaves that note's id: sub/note.tex, sub/note.dvisvgm.log -> sub/note
	auto belongsToNote = [&](std::string relative) {
		while (true)
		{
			auto dot = relative.rfind('.');
			auto slash = relative.rfind('/');
			if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return false;
			relative = relative.substr(0, dot);
			if (ids.contains(relative)) return true;
		}
	};

	std::vector<std::filesystem::path> unwanted;
	for (auto &entry : std::filesystem::recursive_directory_iterator(BuildDir))
	{
		if (!entry.is_regular_file()) continue;
		auto relative = entry.path().lexically_relative(BuildDir).generic_string();
		if (belongsToNote(relative)) continue;
		unwanted.push_back(entry.path());
	}
	for (auto &file : unwanted)
	{
		LOG(DEBUG) << "Sweeping scratch file " << file;
		std::filesystem::remove(file);
	}
	RemoveEmptyDirectories(BuildDir);
}
