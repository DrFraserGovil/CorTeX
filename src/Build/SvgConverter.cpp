#include "SvgConverter.h"
#include "Shell.h"
#include "../Utility/Files.h"
#include <JSL.h>
#include <regex>

namespace
{
	//! Undoes the XML escaping of an attribute value (only the five predefined entities can appear in a file path)
	std::string Unescape(std::string_view value)
	{
		static const std::pair<std::string_view, char> entities[] = {{"&amp;", '&'}, {"&apos;", '\''}, {"&quot;", '"'}, {"&lt;", '<'}, {"&gt;", '>'}};
		std::string out;
		for (size_t i = 0; i < value.size(); ++i)
		{
			bool replaced = false;
			for (auto &[entity, c] : entities)
			{
				if (value.compare(i, entity.size(), entity) == 0)
				{
					out += c;
					i += entity.size() - 1;
					replaced = true;
					break;
				}
			}
			if (!replaced) out += value[i];
		}
		return out;
	}

	//! Finds the file an <image> href names: relative to the note's directory (as TeX looked first), else the file TeX read with that path ending
	std::optional<std::filesystem::path> Locate(const std::string &href, const std::filesystem::path &sourceDir, const std::set<std::filesystem::path> &inputs)
	{
		auto direct = (sourceDir / href).lexically_normal();
		if (std::filesystem::is_regular_file(direct)) return direct;

		auto tail = "/" + std::filesystem::path(href).lexically_normal().generic_string();
		for (auto &input : inputs)
		{
			auto name = input.generic_string();
			if (name.size() >= tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0) return input;
		}
		return std::nullopt;
	}

	//! The file names of the bitmap (and SVG) images TeX placed, from its log: graphics writes "<file>" for each one it includes
	std::set<std::string> PlacedImages(const std::filesystem::path &texLog)
	{
		static const std::regex placed(R"(<([^<>\s]+)>)");
		std::set<std::string> names;
		if (!std::filesystem::exists(texLog)) return names;
		auto log = JSL::IO::getFile(texLog);
		for (std::sregex_iterator match(log.begin(), log.end(), placed), end; match != end; ++match)
		{
			std::filesystem::path file = (*match)[1].str();
			if (Utility::ImageMediaType(file)) names.insert(file.filename().string());
		}
		return names;
	}

	//! Replaces the href of every <image> pointing at a file with a data: URI holding that file
	//! @param seen Receives the file name of every <image> found (embedded or not), to check against what TeX placed
	std::string EmbedImages(const std::string &svg, const std::filesystem::path &sourceDir, const std::set<std::filesystem::path> &inputs, std::vector<std::string> &warnings, std::set<std::string> &seen)
	{
		std::string out;
		out.reserve(svg.size());
		size_t done = 0;
		for (size_t tag = svg.find("<image"); tag != std::string::npos; tag = svg.find("<image", tag + 1))
		{
			auto end = svg.find('>', tag);
			auto attribute = svg.find("xlink:href=", tag);
			if (end == std::string::npos || attribute == std::string::npos || attribute > end) continue;

			auto open = attribute + 11;
			char quote = svg[open];
			auto close = svg.find(quote, open + 1);
			if ((quote != '\'' && quote != '"') || close == std::string::npos) continue;

			auto href = Unescape(std::string_view(svg).substr(open + 1, close - open - 1));
			if (href.starts_with("data:") || href.starts_with("#") || href.find("://") != std::string::npos) continue;
			seen.insert(std::filesystem::path(href).filename().string());

			auto file = Locate(href, sourceDir, inputs);
			if (!file)
			{
				warnings.push_back("image '" + href + "' could not be found to embed; it won't show in the viewer");
				continue;
			}
			auto uri = Utility::ImageDataUri(*file);
			if (!uri)
			{
				warnings.push_back("image '" + href + "' is not a type a browser can show (use PNG, JPEG, GIF, WebP or SVG); it won't show in the viewer");
				continue;
			}

			out.append(svg, done, open + 1 - done);
			out += *uri;
			done = close;
		}
		out.append(svg, done);
		return out;
	}
} // namespace

Svg::Result Svg::Convert(const std::filesystem::path &xdv, const std::filesystem::path &sourceDir, const std::set<std::filesystem::path> &inputs)
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
	auto cmd = "dvisvgm --font-format=woff2 --page=1 --bbox=papersize --output=" + Shell::Quote(std::filesystem::absolute(svgFile)) + " " + Shell::Quote(std::filesystem::absolute(xdv));

	/// RUN CONVERTER
	// from the note's directory, where TeX ran: files dvisvgm reads itself (e.g. PDF images) are named relative to it
	auto log = dir / (stem + ".dvisvgm.log");
	int code = Shell::Run(cmd, sourceDir, log);

	/// CHECK IF IT WORKED
	if (code != 0 || !std::filesystem::exists(svgFile))
	{
		LOG(WARN) << "dvisvgm failed on " << xdv << " (exit code " << code << "); see " << log;
		return {std::nullopt, {}};
	}

	/// PASS ON DVISVGM'S WARNINGS
	Result result;
	for (auto &line : JSL::IO::getFileLines(log))
	{
		auto at = line.find("WARNING: ");
		if (at != std::string::npos) result.Warnings.push_back("dvisvgm: " + line.substr(at + 9));
	}

	/// READ SVG INTO MEMORY, WITH ITS IMAGES
	std::set<std::string> seen;
	result.Svg = EmbedImages(JSL::IO::getFile(svgFile), sourceDir, inputs, result.Warnings, seen);

	/// CHECK EVERY IMAGE TEX PLACED MADE IT INTO THE SVG
	// an image can vanish without any error: e.g. if a package swaps in XeTeX's native graphics driver, the images are written in a form dvisvgm ignores
	for (auto &name : PlacedImages(dir / (stem + ".log")))
	{
		if (!seen.contains(name)) result.Warnings.push_back("image '" + name + "' was placed by TeX but is missing from the SVG (has a package replaced the dvisvgm graphics driver?)");
	}
	return result;
}
