#include "Note.h"
#include "Util/Text.h"

std::vector<std::string> Notes::Note::Names() const
{
	std::vector<std::string> names = {Fields.Title};
	names.insert(names.end(), Fields.Aliases.begin(), Fields.Aliases.end());
	return names;
}

std::string Notes::IdOf(const std::filesystem::path &relativePath)
{
	auto stem = relativePath;
	stem.replace_extension();
	// '/' survives so that ids mirror the folder structure; lowercase so ids don't depend on how a file name was capitalised
	return Util::Sanitise(stem.generic_string(), "/-_", true);
}
