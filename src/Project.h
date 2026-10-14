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
	std::filesystem::path BuildDir;	 //!< scratch space for tex/xdv/svg intermediates
	std::filesystem::path OutputDir; //!< the browser-facing product

	//! Every .tex file under Root, relative to Root, skipping hidden files and directories (including .cortex)
	std::set<std::filesystem::path> FindSources() const;

	void BuildFiles(std::vector<Note> path, std::string_view preamble);
};
