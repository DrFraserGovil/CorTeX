#pragma once
#include "Build/BuildState.h"
#include "Build/Note.h"
#include <filesystem>
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

	//! A hash of everything which affects every note: the preamble, cortex.cls, the width, the output mode and the engine version
	//! @details If this changes between builds, every note must be recompiled
	std::string GlobalHash() const;

	//! Bumped by hand whenever a change to the engine alters the TeX it generates, so existing projects recompile
	static constexpr int EngineVersion = 2; // 2: [[links]] rewritten by the syntax engine

  private:
	std::filesystem::path Root;
	std::filesystem::path BuildDir;		  //!< scratch space for tex/xdv/svg intermediates
	std::filesystem::path OutputDir;	  //!< the browser-facing product
	std::filesystem::path StagingDir;	  //!< where a clean rebuild is assembled, before being swapped into OutputDir
	std::filesystem::path PreviousDir;	  //!< where the old output is held during the swap, so it can be restored if the swap fails
	std::filesystem::path BuildStateFile; //!< the record of what the last build did with each note

	//! Replaces OutputDir with StagingDir, restoring the old output if the replacement fails
	void SwapStagingIntoOutput();

	//! Every file under Root matching the allowed patterns, skipping ignored ones, .cortex and the output directory
	std::set<std::filesystem::path> FindSources() const;

	//! Reads every source file into a Note, skipping (with a warning) any without a title
	//! @throws runtime_error If two notes share an id
	std::vector<Note> ScanNotes() const;

	//! A hash of the parts of a note which affect its compiled output (its local preamble and body; not its header)
	static std::string NoteHash(const Note &note);

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
	void WriteIndex(const std::vector<Note> &notes, const BuildState &state, const std::filesystem::path &targetDir);

	//! Deletes everything in OutputDir which isn't an output of a current note (or the viewer), then any emptied directories
	void SweepOutput(const std::vector<Note> &notes);

	//! Deletes scratch files in BuildDir which don't belong to a current note, then any emptied directories
	void SweepScratch(const std::vector<Note> &notes);
};
