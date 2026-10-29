#pragma once
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Svg
{
	//! The outcome of converting one note
	struct Result
	{
		//! The SVG text, or nullopt if conversion failed (the reason is logged)
		std::optional<std::string> Svg;
		//! Problems which didn't stop the conversion (an image dvisvgm couldn't find, or one which couldn't be embedded), one line each
		std::vector<std::string> Warnings;
	};

	//! Converts a single-page XDV file into SVG using dvisvgm, then embeds every bitmap image it references
	//! @details dvisvgm runs in the note's source directory, where TeX ran, so relative image paths resolve as they did for TeX.
	//! The graphics driver writes bitmaps (and SVG images) as <image> elements pointing at the file as TeX found it; each is replaced by
	//! a data: URI, so a note's output is self-contained and the viewer never needs copies of the project's images.
	//! @param xdv The compiled note
	//! @param sourceDir The note's source directory (TeX's working directory)
	//! @param inputs Every file TeX read (absolute): images TeX found on its search path rather than by relative path are looked up here
	Result Convert(const std::filesystem::path &xdv, const std::filesystem::path &sourceDir, const std::set<std::filesystem::path> &inputs);
} // namespace Svg
