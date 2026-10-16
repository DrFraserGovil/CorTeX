#pragma once
#include "Build/Note.h"
#include <filesystem>
#include <set>
//! Owns everything needed to turn an archive into its output folder
class CortexProject
{
  public:
	explicit CortexProject(std::filesystem::path root);

	//! Compiles every note in the archive and writes the output folder
	void CleanRebuild();

  private:
	std::filesystem::path Root;
	std::filesystem::path BuildDir;	   //!< scratch space for tex/xdv/svg intermediates
	std::filesystem::path OutputDir;   //!< the browser-facing product
	std::filesystem::path StagingDir;  //!< the browser-facing product
	std::filesystem::path PreviousDir; //!< where the old output is held during the swap, so it can be restored if the swap fails

	//! Replaces OutputDir with StagingDir, restoring the old output if the replacement fails
	void SwapStagingIntoOutput();

	//! Every .tex file under Root, relative to Root, skipping hidden files and directories (including .cortex)
	std::set<std::filesystem::path> FindSources() const;

	size_t BuildFiles(std::vector<Note> path, std::string_view preamble, bool stageOuput);
};
