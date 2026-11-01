#include "Files.h"
#include "Text.h"
#include <JSL.h>
#include <map>

void Util::WriteAtomic(const std::filesystem::path &target, std::string_view content)
{
	auto temporary = target;
	temporary += ".tmp";
	JSL::IO::writeString(temporary, content);
	std::filesystem::rename(temporary, target);
}

void Util::CopyAtomic(const std::filesystem::path &from, const std::filesystem::path &to)
{
	auto temporary = to;
	temporary += ".tmp";
	std::filesystem::copy_file(from, temporary, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::rename(temporary, to);
}

bool Util::IsInside(const std::filesystem::path &path, const std::filesystem::path &folder)
{
	auto relative = path.lexically_normal().lexically_relative(folder.lexically_normal());
	return !relative.empty() && relative != "." && *relative.begin() != "..";
}

std::optional<std::string> Util::ImageMediaType(const std::filesystem::path &file)
{
	static const std::map<std::string, std::string> types = {
		{".png", "image/png"},
		{".jpg", "image/jpeg"},
		{".jpeg", "image/jpeg"},
		{".gif", "image/gif"},
		{".webp", "image/webp"},
		{".svg", "image/svg+xml"},
	};
	auto type = types.find(JSL::String::getLower(file.extension().string()));
	if (type == types.end()) return std::nullopt;
	return type->second;
}

std::optional<std::string> Util::ImageDataUri(const std::filesystem::path &file)
{
	auto type = ImageMediaType(file);
	if (!type) return std::nullopt;
	return "data:" + *type + ";base64," + Base64(JSL::IO::getFile(file));
}
