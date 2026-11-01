#include "Sweep.h"
#include "Output/Layout.h"
#include "Tex/Compiler.h"
#include <algorithm>
#include <set>

void Build::RemoveEmptyFolders(const std::filesystem::path &root)
{
	std::vector<std::filesystem::path> folders;
	for (auto &entry : std::filesystem::recursive_directory_iterator(root))
	{
		if (entry.is_directory() && !entry.is_symlink()) folders.push_back(entry.path());
	}
	std::sort(folders.begin(), folders.end(), [](auto &a, auto &b) { return a.string().size() > b.string().size(); });
	for (auto &folder : folders)
	{
		if (std::filesystem::is_empty(folder)) std::filesystem::remove(folder);
	}
}

void Build::SweepOutput(const std::filesystem::path &outputDir, const std::vector<Notes::Note> &notes, bool pdf)
{
	if (!std::filesystem::exists(outputDir)) return;
	std::set<std::filesystem::path> expected;
	for (auto &note : notes)
	{
		if (!note.IsStub()) expected.insert(Output::NoteFile(outputDir, note, pdf)); // a note turned into a stub loses its old output
	}
	if (!pdf)
	{
		for (auto &file : Output::ViewerFiles(outputDir)) expected.insert(file);
	}

	std::vector<std::filesystem::path> unexpected;
	for (auto it = std::filesystem::recursive_directory_iterator(outputDir); it != std::filesystem::recursive_directory_iterator(); ++it)
	{
		if (it->is_symlink() || (!it->is_directory() && !expected.contains(it->path())))
		{
			unexpected.push_back(it->path());
			if (it->is_directory()) it.disable_recursion_pending(); // a link to a folder is removed, never followed
		}
	}
	for (auto &file : unexpected) std::filesystem::remove(file);
	RemoveEmptyFolders(outputDir);
}

void Build::SweepScratch(const std::filesystem::path &buildDir, const std::filesystem::path &fragmentDir, const std::vector<Notes::Note> &notes)
{
	std::set<std::string> ids, fragments;
	for (auto &note : notes)
	{
		ids.insert(note.Id);
		if (!note.IsStub()) fragments.insert(Tex::Compiler::FragmentName(note) + ".tex");
	}

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
	if (std::filesystem::exists(buildDir))
	{
		std::vector<std::filesystem::path> unwanted;
		for (auto &entry : std::filesystem::recursive_directory_iterator(buildDir))
		{
			if (entry.is_regular_file() && !belongsToNote(entry.path().lexically_relative(buildDir).generic_string())) unwanted.push_back(entry.path());
		}
		for (auto &file : unwanted) std::filesystem::remove(file);
		RemoveEmptyFolders(buildDir);
	}
	if (std::filesystem::exists(fragmentDir))
	{
		std::vector<std::filesystem::path> stale;
		for (auto &entry : std::filesystem::directory_iterator(fragmentDir))
		{
			if (!fragments.contains(entry.path().filename().string())) stale.push_back(entry.path());
		}
		for (auto &file : stale) std::filesystem::remove(file);
	}
}
