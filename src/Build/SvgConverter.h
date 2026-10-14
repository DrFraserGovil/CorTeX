#pragma once
#include <filesystem>
#include <string>
#include <vector>

//! Converts an XDV file into one SVG string per page using dvisvgm
class SvgConverter
{
  public:
	//! @returns The SVG text of each page, in order; empty if conversion failed (the reason is logged)
	std::vector<std::string> Convert(const std::filesystem::path &xdv);
};
