#include "SvgConverter.h"
#include "Shell.h"
#include <JSL.h>

std::optional<std::string> Svg::Convert(const std::filesystem::path &xdv)
{
	/// NAME THE OUTPUT
	auto dir = xdv.parent_path();
	auto stem = xdv.stem().string();
	auto svgFile = dir / (stem + ".svg");

	// Remove the previous output (if it exists) so a failed run can't leave a stale result looking like a success
	JSL::IO::remove(svgFile, JSL::IO::Policy::Generous);

	/// CONSTRUCT CONVERSION INSTRUCTIONS
	// --font-format=woff2 embeds the fonts compactly, so the SVG renders identically without the fonts installed
	// --page=1 converts only the first (and only) page
	// --bbox=papersize keeps the page size set by standalone (i.e. the full cortexbody width); the default crops to the visible ink, so every note would have a different width
	auto cmd = "dvisvgm --font-format=woff2 --page=1 --bbox=papersize --output=" + Shell::Quote(svgFile.filename()) + " " + Shell::Quote(xdv.filename());

	/// RUN CONVERTER
	auto log = dir / (stem + ".dvisvgm.log");
	int code = Shell::Run(cmd, dir, log);

	/// CHECK IF IT WORKED
	if (code != 0 || !std::filesystem::exists(svgFile))
	{
		LOG(WARN) << "dvisvgm failed on " << xdv << " (exit code " << code << "); see " << log;
		return std::nullopt;
	}

	/// READ SVG INTO MEMORY
	return JSL::IO::getFile(svgFile);
}
