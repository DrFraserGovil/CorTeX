#include "Pipeline.h"
#include "Notes/Reader.h"
#include "Notes/Syntax.h"
#include "Output/Layout.h"
#include "Output/Viewer.h"
#include "Plan.h"
#include "Platform/Process.h"
#include "Sweep.h"
#include "Tex/ClassSource.h"
#include "Tex/Svg.h"
#include "Util/Files.h"
#include "Util/Text.h"
#include <JSL.h>

namespace
{
	// Everything which affects every note: a change recompiles them all
	std::string GlobalHash(const Build::Scan &scan, const std::string &preamble)
	{
		// '\0' separates the parts, so that moving text from one part to the next can't produce the same hash
		return Util::HashHex(preamble + '\0' + std::string(Tex::ClassSource()) + '\0' + scan.Config.Compilation.Key() + '\0' + std::to_string(Build::EngineVersion));
	}

	std::string ReadPreamble(const Build::Scan &scan)
	{
		return std::filesystem::exists(scan.Paths.Preamble) ? JSL::IO::getFile(scan.Paths.Preamble) : "";
	}

	// Every note's fragment, current (a transcluded note may not be compiled this build, but its fragment must be up to date). Unchanged
	// ones are left alone, keeping their timestamps
	void WriteFragments(const Build::Scan &scan)
	{
		std::filesystem::create_directories(scan.Paths.Fragments);
		for (auto &note : *scan.Notes)
		{
			if (note.IsStub()) continue;
			auto file = scan.Paths.Fragments / (Tex::Compiler::FragmentName(note) + ".tex");
			if (std::filesystem::exists(file) && JSL::IO::getFile(file) == note.Processed) continue;
			Util::WriteAtomic(file, note.Processed);
		}
	}

	bool DependenciesChanged(const std::filesystem::path &root, const Build::Record &record)
	{
		for (auto &[path, hash] : record.Dependencies)
		{
			auto file = root / path;
			if (!std::filesystem::is_regular_file(file) || Util::HashHex(JSL::IO::getFile(file)) != hash) return true;
		}
		return false;
	}

	//! Compiles notes and records what happened; shared by updates and rebuilds
	class Compiling
	{
	  public:
		Compiling(const Build::Scan &scan, const Build::Plan &plan, Build::BuildState &state, std::filesystem::path outputDir, Report::Log &report)
			: Scan(scan), Plan(plan), State(state), OutputDir(std::move(outputDir)), Report(report),
			  Compiler(scan.Paths.Build, scan.Paths.Class, scan.Paths.Fragments, ReadPreamble(scan), scan.Config.Compilation)
		{
		}

		//! Compiles each note; then, if that changed any label's number, the notes showing it compile once more
		void Run(std::vector<const Notes::Note *> notes)
		{
			size_t round = 0;
			while (!notes.empty() && round++ < 2)
			{
				for (size_t i = 0; i < notes.size(); ++i)
				{
					LOG(INFO) << "[" << i + 1 << "/" << notes.size() << "] " << notes[i]->Path.generic_string();
					Compile(*notes[i]);
				}
				// a note's page shows the numbers of the labels it refers to, as their owners last printed them: compiling may have
				// changed some, which changes the pages that show them (and so their hashes)
				notes.clear();
				for (auto &note : *Scan.Notes)
				{
					auto record = State.Records.find(note.Id);
					if (!note.IsStub() && record != State.Records.end() && record->second.Hash != Build::Plan::Hash(Plan.JobFor(note, State))) notes.push_back(&note);
				}
				if (!notes.empty()) Report.Account("Label numbers changed; compiling the notes which show them again");
			}
		}

		size_t Failures = 0;

	  private:
		const Build::Scan &Scan;
		const Build::Plan &Plan;
		Build::BuildState &State;
		std::filesystem::path OutputDir;
		Report::Log &Report;
		Tex::Compiler Compiler;

		void Compile(const Notes::Note &note)
		{
			Report.Account("Compiling " + note.Path.generic_string());
			auto job = Plan.JobFor(note, State);
			auto previous = State.Records.find(note.Id);
			Build::Record record{note.Id, Build::Plan::Hash(job), false, false, "", {}, {}};
			bool pdf = Scan.Config.Compilation.Pdf;

			/// COMPILE, AND CONVERT IF NEEDED
			auto result = Compiler.Compile(job);
			std::string error = result.Error;
			if (result.Output && pdf)
			{
				Output::WritePdf(OutputDir, note, *result.Output);
			}
			else if (result.Output)
			{
				auto svg = Tex::ConvertToSvg(*result.Output, note.Source.parent_path(), result.Inputs);
				for (auto &warning : svg.Warnings) Report.Add(Report::Kind::Note, warning, note.Path);
				if (svg.Svg) Output::WriteNote(OutputDir, note, *svg.Svg);
				else error = svg.Error;
			}

			/// RECORD THE OUTCOME
			record.Succeeded = error.empty();
			record.Diagnostic = error;
			// after a failure, any existing output is the last good version, and stays in place: so do the label numbers it shows
			record.HasOutput = std::filesystem::exists(Output::NoteFile(OutputDir, note, pdf));
			if (record.Succeeded)
			{
				record.Dependencies = Dependencies(result.Inputs);
				WarnAboutInputs(note, record.Dependencies);
				// only the labels the note owns: those of notes transcluded into it belong to them
				for (auto &label : note.Labels)
				{
					if (auto text = result.Labels.find(label.Name); text != result.Labels.end()) record.Labels[label.Name] = text->second;
				}
				for (auto &warning : result.LabelWarnings) Report.Add(Report::Kind::Label, "LaTeX: " + warning, note.Path);
			}
			else
			{
				if (previous != State.Records.end()) record.Labels = previous->second.Labels;
				Report.Add(Report::Kind::Failure, error, note.Path);
				Report.Account("  failed: " + error);
				++Failures;
			}
			State.Records[note.Id] = record;
		}

		// The project's own files TeX read (\input files, local packages, images), with their hashes. Files outside the project are the
		// TeX installation (tracking every package would be slow, and they rarely change); .cortex and the output are CorTeX's own
		std::vector<std::pair<std::string, std::string>> Dependencies(const std::set<std::filesystem::path> &inputs) const
		{
			std::vector<std::pair<std::string, std::string>> dependencies;
			for (auto &file : inputs)
			{
				if (!Util::IsInside(file, Scan.Paths.Root) || Util::IsInside(file, Scan.Paths.Cortex) || Util::IsInside(file, Scan.Paths.Output)) continue;
				dependencies.push_back({file.lexically_relative(Scan.Paths.Root).generic_string(), Util::HashHex(JSL::IO::getFile(file))});
			}
			return dependencies;
		}

		// \input of a note, or of a file with [[links]], is almost certainly a mistake: say so
		void WarnAboutInputs(const Notes::Note &note, const std::vector<std::pair<std::string, std::string>> &dependencies)
		{
			for (auto &[path, hash] : dependencies)
			{
				auto file = Scan.Paths.Root / path;
				if (!Notes::ReaderFor(file)) continue; // packages, images, bibliographies: plain TeX's business
				auto text = JSL::IO::getFile(file);
				auto sections = Notes::Split(text);
				std::vector<Notes::HeaderProblem> ignored;
				if (sections.HasHeader && !Notes::Header::Parse(sections.Header, ignored).Title.empty())
				{
					Report.Add(Report::Kind::Note, "uses \\input on " + path + ", which is a CorTeX note: its header will be typeset as text. To include a note, transclude it with [[!...]]",
							   note.Path);
				}
				else if (!Notes::Syntax::Process(text).References.empty())
				{
					Report.Add(Report::Kind::Note, path + " (\\input by " + note.Path.generic_string() + ") contains [[links]], which only work in notes: in an \\input file they appear as written",
							   path);
				}
			}
		}
	};

	// The counts the author should know about after every build
	void Tally(const Build::Scan &scan, Report::Log &report)
	{
		auto todo = std::count_if(scan.Notes->begin(), scan.Notes->end(), [](auto &n) { return n.Fields.Todo.has_value(); });
		if (todo > 0) report.Account(std::to_string(todo) + (todo == 1 ? " note is" : " notes are") + " flagged todo");
		if (auto unresolved = scan.Net.Unresolved(); unresolved > 0)
		{
			report.Account(std::to_string(unresolved) + (unresolved == 1 ? " link points at a note which doesn't" : " links point at notes which don't") + " exist yet");
		}
	}

	std::vector<const Notes::Note *> Compilable(const Build::Scan &scan)
	{
		std::vector<const Notes::Note *> notes;
		for (auto &note : *scan.Notes)
		{
			if (!note.IsStub()) notes.push_back(&note); // a stub is never compiled
		}
		return notes;
	}
} // namespace

void Build::CheckPrerequisites(bool pdf)
{
	std::vector<std::string> missing;
	for (std::string program : {"xelatex", "dvisvgm"})
	{
		if (program == "dvisvgm" && pdf) continue;
		if (!Platform::Find(program)) missing.push_back(program);
	}
	if (missing.empty()) return;
	throw std::runtime_error("CorTeX needs " + JSL::String::stitch(missing, 0, missing.size(), " and ") + ", which can't be found: install " +
							 (missing.size() == 1 ? "it" : "them") + " (TeX Live has both), or add " + (missing.size() == 1 ? "it" : "them") + " to PATH");
}

std::string Build::IndexJs(const Scan &scan, const BuildState &state, bool forSharing, Report::Log &report)
{
	return Output::IndexJs(scan.Net, state, scan.Config, {scan.Registry.Name, forSharing}, report);
}

void Build::Update(const Scan &scan, Report::Log &report)
{
	bool pdf = scan.Config.Compilation.Pdf;
	CheckPrerequisites(pdf);
	auto &paths = scan.Paths;
	Plan plan(scan.Net, scan.Config, report);
	WriteFragments(scan);

	/// DECIDE WHICH NOTES NEED COMPILING
	auto state = BuildState::Load(paths.State);
	auto global = GlobalHash(scan, ReadPreamble(scan));
	bool globalChanged = state.GlobalHash != global;
	if (globalChanged && !state.Records.empty()) report.Account("Project-wide inputs have changed (preamble, settings or CorTeX version); every note will be recompiled");
	std::vector<const Notes::Note *> dirty;
	auto notes = Compilable(scan);
	for (auto *note : notes)
	{
		auto record = state.Records.find(note->Id);
		bool isDirty = globalChanged													// something every note depends on has changed
					   || record == state.Records.end()									// never built
					   || record->second.Hash != Plan::Hash(plan.JobFor(*note, state))	// edited (or something it shows has) since
					   || !record->second.Succeeded										// failed last time: try again
					   || DependenciesChanged(paths.Root, record->second)				// an \input file, package or image has changed
					   || !std::filesystem::exists(Output::NoteFile(paths.Output, *note, pdf)); // output deleted (or moved, in PDF mode)
		if (isDirty) dirty.push_back(note);
	}
	LOG(INFO) << dirty.size() << " of " << notes.size() << " notes need compiling";
	report.Account(std::to_string(dirty.size()) + " of " + std::to_string(notes.size()) + " notes need compiling");

	/// COMPILE THEM, STRAIGHT INTO THE OUTPUT
	state.GlobalHash = global;
	Compiling compiling(scan, plan, state, paths.Output, report);
	compiling.Run(dirty);

	/// RECORD, INDEX AND TIDY UP
	std::set<std::string> ids;
	for (auto &note : *scan.Notes) ids.insert(note.Id);
	state.RetainOnly(ids);
	bool indexChanged = !pdf && Output::WriteIndex(paths.Output, IndexJs(scan, state, false, report));
	SweepOutput(paths.Output, *scan.Notes, pdf);
	SweepScratch(paths.Build, paths.Fragments, *scan.Notes);
	state.Save(paths.State);
	Tally(scan, report);

	// tell any open viewer to refresh, but only if something it shows changed; written last, once everything is in place
	if (!pdf && (indexChanged || !dirty.empty())) Output::WriteVersion(paths.Output);
}

void Build::Rebuild(const Scan &scan, Report::Log &report)
{
	bool pdf = scan.Config.Compilation.Pdf;
	CheckPrerequisites(pdf);
	auto &paths = scan.Paths;

	// the staging folder, cleared (with any output left over from an interrupted swap); it exists even if nothing is written into it
	std::filesystem::remove_all(paths.Staging);
	std::filesystem::remove_all(paths.Previous);
	std::filesystem::create_directories(paths.Staging);
	Plan plan(scan.Net, scan.Config, report);
	WriteFragments(scan);

	// a fresh state: nothing from earlier builds survives a rebuild
	BuildState state;
	state.GlobalHash = GlobalHash(scan, ReadPreamble(scan));
	auto notes = Compilable(scan);
	report.Account("Rebuilding all " + std::to_string(notes.size()) + " notes");
	Compiling compiling(scan, plan, state, paths.Staging, report);
	compiling.Run(notes);
	if (!notes.empty() && compiling.Failures == notes.size())
	{
		throw std::runtime_error("every note failed to compile, which suggests a problem with the whole project (the preamble?); the output is unchanged");
	}
	if (!pdf)
	{
		Output::WriteIndex(paths.Staging, IndexJs(scan, state, false, report));
		Output::WriteVersion(paths.Staging); // a rebuild always counts as a change, for any viewer open on the old output
	}

	/// SWAP THE NEW OUTPUT IN: THE OLD ONE IS KEPT ASIDE UNTIL THE NEW ONE IS IN PLACE, SO A FAILED RENAME CAN BE UNDONE
	bool hadOutput = std::filesystem::exists(paths.Output);
	if (hadOutput) std::filesystem::rename(paths.Output, paths.Previous);
	try
	{
		std::filesystem::create_directories(paths.Output.parent_path());
		std::filesystem::rename(paths.Staging, paths.Output);
	}
	catch (std::exception &)
	{
		if (hadOutput) std::filesystem::rename(paths.Previous, paths.Output);
		throw;
	}
	std::filesystem::remove_all(paths.Previous);
	SweepScratch(paths.Build, paths.Fragments, *scan.Notes);
	state.Save(paths.State);
	Tally(scan, report);
}
