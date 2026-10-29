#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Utility
{
	//! Writes via a temporary file and a rename, so the target is always either the complete old version or the complete new one:
	//! a crash (or a reader mid-write) never sees a half-written file
	void WriteAtomic(const std::filesystem::path &target, std::string_view content);

	//! Encodes a string as a JSON string literal (which is also a valid JS string literal), quotes included
	std::string JsonString(std::string_view input);

	//! Encodes bytes as standard base64 (with '=' padding), e.g. for a data: URI
	std::string Base64(std::string_view bytes);

	//! The media type a browser shows an image file as, from its extension (PNG, JPEG, GIF, WebP, SVG); nullopt for anything else
	std::optional<std::string> ImageMediaType(const std::filesystem::path &file);

	//! The file as a base64 data: URI (for embedding in a page), or nullopt if it isn't an image type ImageMediaType knows
	std::optional<std::string> ImageDataUri(const std::filesystem::path &file);
} // namespace Utility
