#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

//! Small file utilities which JSL doesn't provide
namespace Util
{
	//! Writes via a temporary file and a rename, so the target is always either the complete old version or the complete new one:
	//! a crash (or a reader mid-write) never sees a half-written file
	void WriteAtomic(const std::filesystem::path &target, std::string_view content);

	//! Copies a file the same way as WriteAtomic
	void CopyAtomic(const std::filesystem::path &from, const std::filesystem::path &to);

	//! True if path is strictly inside folder (both taken as they are: resolve symlinks first where that matters)
	bool IsInside(const std::filesystem::path &path, const std::filesystem::path &folder);

	//! The media type a browser shows an image file as, from its extension (PNG, JPEG, GIF, WebP, SVG); nullopt for anything else
	std::optional<std::string> ImageMediaType(const std::filesystem::path &file);

	//! The file as a base64 data: URI (for embedding in a page), or nullopt if it isn't an image type ImageMediaType knows
	std::optional<std::string> ImageDataUri(const std::filesystem::path &file);
} // namespace Util
