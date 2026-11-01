#pragma once
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Tex
{
	//! The outcome of converting one note
	struct SvgResult
	{
		//! The SVG text, or nullopt if conversion failed
		std::optional<std::string> Svg;
		//! Why it failed; empty on success
		std::string Error;
		//! Problems which didn't stop the conversion (an image dvisvgm couldn't find, or one which couldn't be embedded), one line each
		std::vector<std::string> Warnings;
	};

	//! Converts a single-page XDV file into SVG with dvisvgm, then embeds every bitmap image it references (DESIGN.md §6.5)
	//! @details dvisvgm runs in the note's folder, where TeX ran, so relative image paths resolve as they did for TeX. The graphics
	//! driver writes bitmaps (and SVG images) as <image> elements pointing at the file as TeX found it; each is replaced by a data: URI,
	//! so a note's output is self-contained and the viewer never needs copies of the project's images
	//! @param inputs Every file TeX read (absolute): images TeX found on its search path rather than by relative path are looked up here
	SvgResult ConvertToSvg(const std::filesystem::path &xdv, const std::filesystem::path &sourceDir, const std::set<std::filesystem::path> &inputs);
} // namespace Tex
