#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace Svg
{
	//! Converts a single-page XDV file into SVG using dvisvgm
	//! @returns The SVG text, or nullopt if conversion failed (the reason is logged)
	std::optional<std::string> Convert(const std::filesystem::path &xdv);
} // namespace Svg
