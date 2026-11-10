#include "Paths.h"

Project::Paths::Paths(const std::filesystem::path &root, const std::filesystem::path &output)
	: Root(root), Cortex(root / ".cortex"), Settings(Cortex / "settings.config"), Registry(Cortex / "registry.config"), Preamble(Cortex / "preamble.tex"),
	  Output(output), Build(Cortex / "build"), Fragments(Cortex / "fragments"), Class(Cortex / "class"), Warm(Cortex / "warm"), Staging(Cortex / "staging"), Previous(Cortex / "previous"),
	  State(Cortex / "buildstate.tsv"), Log(Cortex / "build.log")
{
}

bool Project::Paths::IsProject(const std::filesystem::path &root)
{
	// registry.ctx is the registry's old name, renamed the next time it is read (Registry::Read)
	return std::filesystem::exists(root / ".cortex/settings.config") &&
		   (std::filesystem::exists(root / ".cortex/registry.config") || std::filesystem::exists(root / ".cortex/registry.ctx"));
}

std::filesystem::path Project::Paths::Find(const std::filesystem::path &start)
{
	auto folder = std::filesystem::weakly_canonical(std::filesystem::absolute(start));
	if (!std::filesystem::is_directory(folder)) folder = folder.parent_path();
	for (; !folder.empty(); folder = folder.parent_path())
	{
		if (IsProject(folder)) return folder;
		if (folder == folder.root_path()) break;
	}
	return {};
}
