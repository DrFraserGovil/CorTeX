#include "Paths.h"

Project::Paths::Paths(const std::filesystem::path &root, const std::filesystem::path &output)
	: Root(root), Cortex(root / ".cortex"), Settings(Cortex / "settings.config"), Registry(Cortex / "registry.ctx"), Preamble(Cortex / "preamble.tex"),
	  Output(output), Build(Cortex / "build"), Fragments(Cortex / "fragments"), Class(Cortex / "class"), Staging(Cortex / "staging"), Previous(Cortex / "previous"),
	  State(Cortex / "buildstate.tsv"), Log(Cortex / "build.log")
{
}

bool Project::Paths::IsProject(const std::filesystem::path &root)
{
	return std::filesystem::exists(root / ".cortex/settings.config") && std::filesystem::exists(root / ".cortex/registry.ctx");
}
