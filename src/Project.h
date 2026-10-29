#pragma once
#include "Build/BuildState.h"
#include "Build/Index.h"
#include "Build/Note.h"
#include "Build/TexRunner.h"
#include "Build/ProjectLock.h"
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

//! Owns everything needed to turn an archive into its output folder
class CortexProject
{
  public:
	explicit CortexProject(std::filesystem::path root);

	//! Compiles only the notes which have changed since the last build (or which failed, or whose output is missing)
	//! @details If something project-wide has changed (preamble, settings, class, engine version) every note is recompiled, but in place: notes which fail keep their last good output
	void Build();

	//! Compiles every note in the archive from scratch, into a fresh output folder
	void CleanRebuild();

	//! Builds, then keeps watching the project and rebuilds (incrementally) whenever a source, preamble or setting changes, until told to stop
	//! @details Holds the project lock throughout, through the same socket other commands would use to reach it. Stops on "q" (+Enter)
	//! at the terminal, or a "shutdown" message on the socket; a "rerun" message triggers a build, as a file change would.
	void Watch();

	//! Asks whichever CorTeX process holds this project's lock (a watch or a build) to stop
	void Shutdown();

	//! True if some note links to this slug and no note has it as a title or alias (i.e. the project wants a note by that name).
	//! Worked out from a fresh scan of the sources, not from the last build's output
	bool IsWanted(const std::string &slug) const;

	//! The project's name from .cortex/registry.ctx (falling back to the root folder's name)
	std::string ProjectName() const;

	//! The project's global sidebar from .cortex/registry.ctx: every line after "sidebar:" is an entry ("target" or "target|Label"),
	//! except "[Heading]" lines, which start a new group; blank lines and lines starting % are ignored
	std::vector<Index::SidebarGroup> ReadSidebar() const;

	//! A hash of everything which affects every note: the preamble, cortex.cls, the width, the output mode and the engine version
	//! @details If this changes between builds, every note must be recompiled
	std::string GlobalHash() const;

	//! Bumped by hand whenever a change to the engine alters the TeX it generates, so existing projects recompile
	static constexpr int EngineVersion = 11; // 2: [[links]]; 3: extended syntax, title; 4: formatting never opens after a letter/digit/backslash; 5: PDF links; 6: fragments, transclusion, origin-keyed links; 7: # headings; 8: page margin setting (default 4pt -> 8pt); 9: maths environments, no format opener after '}'; 10: images embedded in SVG output, dvisvgm runs in the note's directory; 11: dvisvgm as a global class option

  private:
	//! Runs a build (clean or incremental) while holding the lock, then any builds other commands requested meanwhile, until none are pending
	void RunHoldingLock(ProjectLock &lock, bool clean);

	//! The body of Build(), for callers which already hold the lock (Watch)
	void BuildLocked();

	//! The body of CleanRebuild(), for callers which already hold the lock
	void CleanRebuildLocked();

	//! True if a changed path (relative to Root) could affect the build: anything except the output, the engine's own scratch space,
	//! and hidden files; within .cortex, only the preamble and settings count
	bool AffectsBuild(const std::filesystem::path &relative) const;
	std::filesystem::path Root;
	std::filesystem::path BuildDir;		  //!< scratch space for tex/xdv/svg intermediates
	std::filesystem::path OutputDir;	  //!< the browser-facing product
	std::filesystem::path StagingDir;	  //!< where a clean rebuild is assembled, before being swapped into OutputDir
	std::filesystem::path PreviousDir;	  //!< where the old output is held during the swap, so it can be restored if the swap fails
	std::filesystem::path BuildStateFile; //!< the record of what the last build did with each note
	std::filesystem::path FragmentDir;	  //!< every note's processed body, under key-based names, for \input-ing (by the note itself, and by transclusions)

	//! Replaces OutputDir with StagingDir, restoring the old output if the replacement fails
	void SwapStagingIntoOutput();

	//! Every file under Root matching the allowed patterns, skipping ignored ones, .cortex and the output directory
	std::set<std::filesystem::path> FindSources() const;

	//! Reads every source file into a Note, skipping (with a warning) any without a title
	//! @throws runtime_error If two notes share an id
	std::vector<Note> ScanNotes() const;

	//! A hash of everything specific to a note which affects its compiled output: its local preamble and body (not its header),
	//! and everything in its CompilePlan: the bodies and local preambles of every note it transcludes (so editing a transcluded note
	//! recompiles the notes showing it), how its transclusions resolve, and in PDF mode where its links point
	std::string NoteHash(const Note &note) const;

	//! The CompilePlan of every note, by id: filled by PlanCompilations before anything is compiled
	std::map<std::string, CompilePlan> Plans;

	//! Resolves every note's transclusions and (PDF mode) links from the scan alone, finds transclusion cycles, and fills Plans.
	//! Warns about each cycle; the notes must outlive Plans (the plans point into them)
	void PlanCompilations(const std::vector<Note> &notes);

	//! Writes every note's fragment (its processed body) where TeX can \input it, skipping unchanged ones and removing stale ones
	void WriteFragments(const std::vector<Note> &notes);

	//! The syntax options chosen in the settings (an unknown list type falls back to itemize, with a warning)
	static Syntax::Options SyntaxOptions();

	//! Reduces the files TeX read to the user's own project files (not the TeX installation, not .cortex or the output), with their content hashes
	std::vector<std::pair<std::string, std::string>> ProjectDependencies(const std::set<std::filesystem::path> &inputs) const;

	//! True if any file the note depended on last time has changed or disappeared
	bool DependenciesChanged(const NoteRecord &record) const;

	//! Warns about \input files which won't behave as the author expects: ones containing [[links]] (which are only processed in notes)
	//! and ones which are themselves notes (whose header would be typeset)
	void WarnAboutInputs(const Note &note, const std::vector<std::pair<std::string, std::string>> &dependencies) const;

	//! Compiles each of the notes, writing their outputs into targetDir and recording each outcome in state
	//! @returns The number of notes which failed
	size_t BuildFiles(const std::vector<Note> &notes, std::string_view preamble, const std::filesystem::path &targetDir, BuildState &state);

	//! Writes index.js and the viewer into targetDir (SVG mode only: PDF mode produces nothing but the PDFs)
	//! @returns true if index.js changed (false in PDF mode, which has no index)
	bool WriteIndex(const std::vector<Note> &notes, const BuildState &state, const std::filesystem::path &targetDir);

	//! Deletes everything in OutputDir which isn't an output of a current note (or the viewer), then any emptied directories
	void SweepOutput(const std::vector<Note> &notes);

	//! Deletes scratch files in BuildDir which don't belong to a current note, then any emptied directories
	void SweepScratch(const std::vector<Note> &notes);
};
