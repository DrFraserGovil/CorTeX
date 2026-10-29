#include "Project.h"
#include "Build/Emitter.h"
#include "Build/Index.h"
#include "Build/ProjectLock.h"
#include "Build/SvgConverter.h"
#include "Build/TexRunner.h"
#include "Link.h"
#include "Settings/ArchiveSettings.h"
#include "Utility/Hash.h"
#include "Utility/Files.h"
#include <JSL.h>
#include <functional>
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
	FragmentDir = Root / ".cortex/fragments";
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

std::vector<Index::SidebarGroup> CortexProject::ReadSidebar() const
{
	std::vector<Index::SidebarGroup> groups;
	auto registry = Root / ".cortex/registry.ctx";
	if (!std::filesystem::exists(registry)) return groups;

	bool inSidebar = false;
	for (auto &raw : JSL::IO::getFileLines(registry))
	{
		auto line = JSL::String::trim(raw);
		if (!inSidebar)
		{
			inSidebar = JSL::String::iEquals(line, "sidebar:");
			continue;
		}
		if (line.empty() || line.starts_with("%")) continue;
		if (line.starts_with("[") && line.ends_with("]"))
		{
			groups.push_back({JSL::String::trim(std::string_view(line).substr(1, line.size() - 2)), {}});
			continue;
		}
		if (groups.empty()) groups.push_back({"", {}}); // entries before any heading form an untitled first group
		groups.back().Items.push_back(Index::SidebarItem::Parse(line));
	}
	return groups;
}

std::string CortexProject::ProjectName() const
{
	// registry.ctx holds "name = <name>", written by cortex new (and, after a "sidebar:" line, the global sidebar)
	auto registry = Root / ".cortex/registry.ctx";
	if (std::filesystem::exists(registry))
	{
		for (auto &line : JSL::IO::getFileLines(registry))
		{
			if (JSL::String::iEquals(JSL::String::trim(line), "sidebar:")) break;
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
	key += '\0' + JSL::String::makeFrom(Archive.Render.MarginPT);
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
	// the header isn't included (a new alias or sidebar changes the index, not the compiled note), except the title when it's
	// typeset: whether it is (the project setting, or the note's show-title override) and, if so, its text
	bool titled = note.ShowTitle.value_or(Archive.Render.InsertTitle);
	std::string key = note.LocalPreamble + '\0' + note.Body + '\0' + (titled ? "title:" + note.Title : std::string("untitled"));
	if (auto plan = Plans.find(note.Id); plan != Plans.end())
	{
		for (auto *hoisted : plan->second.Hoisted)
		{
			key += '\0' + hoisted->Key + '\0' + hoisted->LocalPreamble + '\0' + hoisted->Body;
		}
		for (auto &[origin, slug, target] : plan->second.Transclusions)
		{
			key += '\0' + origin + '\0' + slug + '\0' + target;
		}
		for (auto &[origin, slug, problem] : plan->second.TransclusionProblems)
		{
			key += '\0' + origin + '\0' + slug + '\0' + problem;
		}
		for (auto &[origin, slug, path] : plan->second.LinkTargets)
		{
			key += '\0' + origin + '\0' + slug + '\0' + path;
		}
	}
	return Utility::HashHex(key);
}

bool CortexProject::IsWanted(const std::string &slug) const
{
	Index index(ScanNotes(), BuildState{});
	for (auto &entry : index.Entries())
	{
		for (auto &link : entry.Links)
		{
			if (link.Slug == slug && link.Target.empty()) return true;
		}
	}
	return false;
}

void CortexProject::PlanCompilations(const std::vector<Note> &notes)
{
	Plans.clear();

	/// RESOLVE EVERYTHING FROM THE SCAN ALONE (BUILD RESULTS DON'T AFFECT WHERE ANYTHING POINTS)
	Index index(notes, BuildState{});
	std::map<std::string, const Note *> byId;
	std::map<std::string, std::map<std::string, std::string>> resolved; // note id -> slug -> target id (empty if unresolved)
	for (size_t n = 0; n < notes.size(); ++n)
	{
		byId[notes[n].Id] = &notes[n];
		for (auto &link : index.Entries()[n].Links)
		{
			resolved[notes[n].Id][link.Slug] = link.Target;
		}
	}
	// where a link or transclusion finally lands: through any redirect stubs (there's no viewer to forward a PDF link, and a stub has no content to transclude)
	auto target = [&](const Note &from, const std::string &slug) {
		auto id = resolved[from.Id][slug];
		return id.empty() ? id : index.FollowRedirects(id);
	};

	/// FIND CYCLES: STRONGLY CONNECTED COMPONENTS OF THE TRANSCLUSION GRAPH (TARJAN)
	// an edge is circular if it stays within one component (or points at its own note), wherever the compilation starts
	std::map<std::string, int> component, order, low;
	std::vector<std::string> stack;
	std::set<std::string> onStack;
	int counter = 0, components = 0;
	std::function<void(const std::string &)> connect = [&](const std::string &id) {
		order[id] = low[id] = counter++;
		stack.push_back(id);
		onStack.insert(id);
		for (auto &t : byId[id]->Transclusions)
		{
			auto next = target(*byId[id], t.Slug);
			if (next.empty()) continue;
			if (!order.contains(next))
			{
				connect(next);
				low[id] = std::min(low[id], low[next]);
			}
			else if (onStack.contains(next))
			{
				low[id] = std::min(low[id], order[next]);
			}
		}
		if (low[id] == order[id])
		{
			std::string member;
			do
			{
				member = stack.back();
				stack.pop_back();
				onStack.erase(member);
				component[member] = components;
			} while (member != id);
			++components;
		}
	};
	for (auto &note : notes)
	{
		if (!order.contains(note.Id)) connect(note.Id);
	}
	auto circular = [&](const std::string &from, const std::string &to) { return from == to || component[from] == component[to]; };

	// each circular transclusion is reported once, at the place it was written
	for (auto &note : notes)
	{
		for (auto &t : note.Transclusions)
		{
			auto next = target(note, t.Slug);
			if (!next.empty() && circular(note.Id, next))
			{
				LOG(WARN) << note.RelativePath.generic_string() << ":" << note.BodyLine + t.Line - 1 << ": transcluding '" << t.Target << "' here would loop back on itself, so it is shown as an error instead";
			}
		}
	}

	/// ONE PLAN PER NOTE
	bool pdf = Archive.Render.PdfOutput;
	for (auto &root : notes)
	{
		CompilePlan plan;

		// everything reachable through (non-circular) transclusions, in first-visit order: their preambles are hoisted in this order
		std::set<std::string> visited{root.Id};
		std::function<void(const Note &)> visit = [&](const Note &from) {
			for (auto &t : from.Transclusions)
			{
				auto next = target(from, t.Slug);
				if (next.empty() || circular(from.Id, next) || !visited.insert(next).second) continue;
				plan.Hoisted.push_back(byId[next]);
				visit(*byId[next]);
			}
		};
		visit(root);

		// declarations for the note and every note it shows: each resolved from its own origin
		std::vector<const Note *> shown{&root};
		shown.insert(shown.end(), plan.Hoisted.begin(), plan.Hoisted.end());
		auto rootDir = ("/root" / root.RelativePath).parent_path(); // a dummy root, so a top-level note (empty folder) works too
		for (auto *from : shown)
		{
			std::set<std::string> declared;
			for (auto &t : from->Transclusions)
			{
				if (!declared.insert(t.Slug).second) continue;
				auto next = target(*from, t.Slug);
				if (next.empty())
				{
					plan.TransclusionProblems.push_back({from->Key, t.Slug, "missing note: " + t.Slug});
				}
				else if (circular(from->Id, next))
				{
					plan.TransclusionProblems.push_back({from->Key, t.Slug, "circular transclusion: " + t.Slug});
				}
				else
				{
					plan.Transclusions.push_back({from->Key, t.Slug, byId[next]->Key});
				}
			}
			if (!pdf) continue;

			// a PDF link must name the target's file relative to the PDF actually being read: this note's, not the origin's
			std::set<std::string> linked;
			for (auto &link : from->Links)
			{
				auto next = target(*from, link.Slug);
				if (next.empty() || !linked.insert(link.Slug).second) continue;
				auto pdfPath = ("/root" / byId[next]->RelativePath).replace_extension(".pdf");
				plan.LinkTargets.push_back({from->Key, link.Slug, pdfPath.lexically_relative(rootDir).generic_string()});
			}
		}
		Plans[root.Id] = std::move(plan);
	}
}

void CortexProject::WriteFragments(const std::vector<Note> &notes)
{
	JSL::IO::mkdir(FragmentDir, JSL::IO::Policy::Generous);

	// every note's fragment, current: a transcluded note may not be compiled this run, but its fragment must still be up to date
	std::set<std::filesystem::path> expected;
	for (auto &note : notes)
	{
		auto file = FragmentDir / (TexRunner::FragmentName(note) + ".tex");
		expected.insert(file);
		if (std::filesystem::exists(file) && JSL::IO::getFile(file) == note.ProcessedBody) continue; // unchanged: leave its timestamp alone
		Utility::WriteAtomic(file, note.ProcessedBody);
	}

	// and nothing else: fragments of notes which no longer exist
	std::vector<std::filesystem::path> stale;
	for (auto &entry : std::filesystem::directory_iterator(FragmentDir))
	{
		if (!expected.contains(entry.path())) stale.push_back(entry.path());
	}
	for (auto &file : stale)
	{
		std::filesystem::remove(file);
	}
}

bool CortexProject::AffectsBuild(const std::filesystem::path &relative) const
{
	auto path = relative.lexically_normal().generic_string();
	if (path == ".cortex/preamble.tex" || path == ".cortex/settings.config" || path == ".cortex/registry.ctx")
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
			LOG(WARN) << note.RelativePath.generic_string() << " uses \\input on " << path << ", which is a CorTeX note: its header (title, separator line) will be typeset as text. To include a note, transclude it with ![[...]]";
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
	PlanCompilations(notes);
	WriteFragments(notes);

	bool globalChanged = (state.GlobalHash != global);
	if (globalChanged && !state.Records.empty())
	{
		LOG(INFO) << "Project-wide inputs have changed (preamble, settings or CorTeX version); every note will be recompiled";
	}

	/// DECIDE WHICH NOTES NEED COMPILING
	std::vector<Note> dirty;
	for (auto &note : notes)
	{
		if (!note.Redirect.empty()) continue; // a redirect stub is never compiled
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

	bool indexChanged = WriteIndex(notes, state, OutputDir);
	SweepOutput(notes);
	SweepScratch(notes);
	state.Save(BuildStateFile);

	// tell any open viewer to refresh, but only if something it shows changed; written last, once everything is in place
	if (!pdf && (indexChanged || !dirty.empty()))
	{
		output.WriteVersion();
	}
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
	PlanCompilations(notes);
	WriteFragments(notes);

	// a fresh state: nothing from earlier builds survives a rebuild
	BuildState state;
	state.GlobalHash = GlobalHash();
	std::vector<Note> compiled; // everything except redirect stubs, which are never compiled
	std::copy_if(notes.begin(), notes.end(), std::back_inserter(compiled), [](auto &note) { return note.Redirect.empty(); });
	size_t failures = BuildFiles(compiled, preamble, StagingDir, state);
	WriteIndex(notes, state, StagingDir);
	if (!Archive.Render.PdfOutput)
	{
		Emitter(StagingDir).WriteVersion(); // a rebuild always counts as a change, for any viewer open on the old output
	}

	if (compiled.empty() || failures < compiled.size()) // i.e. unless every note failed (which suggests a project-wide problem)
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
	TexRunner tex(BuildDir, preamble, pdf, Archive.Render.WidthCM, Archive.Render.MarginPT, Archive.Render.InsertTitle);
	Emitter emitter(targetDir);

	size_t failures = 0;
	for (auto &note : notes)
	{
		LOG(INFO) << "Compiling " << note.RelativePath;
		NoteRecord record{note.Id, note.RelativePath.generic_string(), NoteHash(note), false, false, "", {}};

		/// COMPILE, AND CONVERT IF NEEDED
		auto result = tex.Compile(note, Plans.at(note.Id));
		std::string error = result.Error;
		if (result.Output && pdf)
		{
			emitter.WritePdf(note, *result.Output);
		}
		else if (result.Output)
		{
			auto svg = Svg::Convert(*result.Output, note.SourceAbsolutePath.parent_path(), result.Inputs);
			for (auto &warning : svg.Warnings)
			{
				LOG(WARN) << note.RelativePath.generic_string() << ": " << warning;
			}
			if (svg.Svg)
			{
				emitter.WriteNote(note, {*svg.Svg});
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

bool CortexProject::WriteIndex(const std::vector<Note> &notes, const BuildState &state, const std::filesystem::path &targetDir)
{
	Index index(notes, state, ReadSidebar());

	/// REPORT ON THE LINK NETWORK
	for (auto &[name, ids] : index.Clashes())
	{
		LOG(WARN) << "'" << name << "' is the title or alias of several notes (" << JSL::String::stitch(ids, 0, ids.size(), ", ") << "); links to it go to whichever is nearest the linking note";
	}
	for (auto &entry : index.Entries())
	{
		if (!entry.RedirectText.empty() && index.FollowRedirects(entry.Id).empty())
		{
			LOG(WARN) << entry.Path << ": redirects to '" << entry.RedirectText << "', which " << (entry.RedirectTarget.empty() ? "matches no note" : "leads into a loop of redirects");
		}
	}
	for (auto &group : index.Sidebar())
	{
		for (auto &item : group.Items)
		{
			if (item.Target.empty())
			{
				LOG(WARN) << "Sidebar entry '" << item.Text << "' (in .cortex/registry.ctx) matches no note";
			}
		}
	}
	for (auto &entry : index.Entries())
	{
		for (auto &item : entry.Sidebar)
		{
			if (item.Target.empty())
			{
				LOG(WARN) << entry.Path << ": sidebar entry '" << item.Text << "' matches no note";
			}
		}
	}
	if (auto todo = std::count_if(index.Entries().begin(), index.Entries().end(), [](auto &e) { return e.Todo; }); todo > 0)
	{
		LOG(INFO) << todo << (todo == 1 ? " note is" : " notes are") << " flagged todo";
	}
	if (auto unresolved = index.UnresolvedCount(); unresolved > 0)
	{
		LOG(INFO) << unresolved << (unresolved == 1 ? " link points at a note which doesn't" : " links point at notes which don't") << " exist yet";
	}

	// PDF mode produces only the PDFs themselves; the index and viewer exist only for SVG output
	if (Archive.Render.PdfOutput)
	{
		return false;
	}
	Emitter::ViewerConfig viewer;
	viewer.Zoom = Archive.Render.ViewerZoom;
	if (!(viewer.Zoom > 0))
	{
		LOG(WARN) << "zoom must be a positive number (1 = true size); using 1";
		viewer.Zoom = 1;
	}
	viewer.Name = ProjectName();
	viewer.Background = Archive.Render.BackgroundColour;
	viewer.FolderPull = Archive.Render.FolderPull;
	if (!(viewer.FolderPull >= 0 && viewer.FolderPull <= 1))
	{
		LOG(WARN) << "folder-pull must be between 0 and 1 (1 ignores folders); using 0.5";
		viewer.FolderPull = 0.5;
	}

	viewer.Root = Root.string();
	std::vector<std::string> problems;
	for (auto &editor : Link::ParseEditors(Archive.Files.Editors, problems))
	{
		viewer.Editors.push_back(editor.Name);
	}
	for (auto &problem : problems)
	{
		LOG(WARN) << "editors setting: " << problem;
	}

	// the logo is embedded, like note images: the viewer never depends on a copy of a project file
	if (!Archive.Render.Logo.empty())
	{
		auto logo = Root / Archive.Render.Logo;
		auto uri = std::filesystem::is_regular_file(logo) ? Utility::ImageDataUri(logo) : std::nullopt;
		if (uri)
		{
			viewer.Logo = *uri;
		}
		else
		{
			LOG(WARN) << "logo '" << Archive.Render.Logo << "' isn't an image file in the project (PNG, JPEG, GIF, WebP or SVG); the sidebar will have no logo";
		}
	}

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
	return Emitter(targetDir).WriteIndex(index.Serialise(), viewer);
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
		if (!note.Redirect.empty()) continue; // stubs have no output (a note turned into a stub loses its old one)
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
