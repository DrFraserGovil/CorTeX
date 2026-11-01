#include "Layout.h"
#include "Util/Files.h"

std::filesystem::path Output::NoteFile(const std::filesystem::path &outputDir, const Notes::Note &note, bool pdf)
{
	if (pdf) return (outputDir / note.Path).replace_extension(".pdf");
	return outputDir / "notes" / (note.Id + ".js");
}

std::vector<std::filesystem::path> Output::ViewerFiles(const std::filesystem::path &outputDir)
{
	return {outputDir / "index.html", outputDir / "index.js", outputDir / "version.js"};
}

void Output::WritePdf(const std::filesystem::path &outputDir, const Notes::Note &note, const std::filesystem::path &pdf)
{
	auto target = NoteFile(outputDir, note, true);
	std::filesystem::create_directories(target.parent_path());
	Util::CopyAtomic(pdf, target);
}
