#include "Reader.h"
#include "TexReader.h"
#include <JSL.h>
#include <map>

const Notes::Reader *Notes::ReaderFor(const std::filesystem::path &file)
{
	static const TexReader tex;
	static const std::map<std::string, const Reader *> readers = {{".tex", &tex}};
	auto reader = readers.find(JSL::String::getLower(file.extension().string()));
	return reader == readers.end() ? nullptr : reader->second;
}

std::optional<Notes::Note> Notes::Read(const std::filesystem::path &root, const std::filesystem::path &source, const ReadOptions &options, Report::Log &report)
{
	auto reader = ReaderFor(source);
	if (!reader) return std::nullopt;
	return reader->Read(root, source, options, report);
}
