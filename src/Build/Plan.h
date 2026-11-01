#pragma once
#include "BuildState.h"
#include "Network/Network.h"
#include "Report/Report.h"
#include "Settings/Config.h"
#include "Tex/Compiler.h"
#include <map>
#include <string>

namespace Build
{
	//! Everything about compiling each note which depends on other notes (DESIGN.md §4.4, §5.2): what its page shows by transclusion,
	//! where each transclusion and label reference on the page goes, and so what its compiled output depends on. Pure: no files
	class Plan
	{
	  public:
		//! Finds the transclusion cycles, reporting each circular transclusion once, where it was written
		Plan(const Network::Network &network, const Settings::Config &config, Report::Log &report);

		//! The compilation of a note's page. The label numbers shown from other notes come from state, so they are as of the last time
		//! their owners compiled
		Tex::Job JobFor(const Notes::Note &note, const BuildState &state) const;

		//! Everything a note's compiled output depends on besides the project-wide inputs: its own and its transcluded notes' text, its
		//! title (if typeset) and every declaration on its page. A note recompiles when this changes. The header isn't included (a new
		//! alias or sidebar changes the index, not the compiled note)
		static std::string Hash(const Tex::Job &job);

	  private:
		const Network::Network &Net;
		const Settings::Config &Config;
		//! Each note's strongly connected component of the transclusion graph: a transclusion within one is circular
		std::map<std::string, int> Component;

		//! Where a transclusion written in from lands: through any stubs; empty if nowhere
		std::string Target(const Notes::Note &from, const std::string &slug) const;
		bool Circular(const std::string &from, const std::string &to) const;
	};
} // namespace Build
