#include "Emitter.h"
#include "ViewerSource.h"
#include <chrono>
#include "../Utility/Files.h"
#include <JSL.h>

// The viewer (index.html) is assembled from src/Viewer/viewer.{html,css,js}, embedded into the binary at build time
// (see CMakeLists.txt): one self-contained page, so it works from file:// with nothing else to install
static std::string ViewerPage()
{
	std::string page(Viewer::Html);
	auto place = [&](std::string_view marker, std::string_view content) {
		auto at = page.find(marker);
		if (at != std::string::npos) page.replace(at, marker.size(), "\n" + std::string(content));
	};
	place("{{CORTEX_CSS}}", Viewer::Css);
	place("{{CORTEX_JS}}", Viewer::Js);
	return page;
}

Emitter::Emitter(std::filesystem::path outputDir) : OutputDir(std::move(outputDir))
{
}

std::filesystem::path Emitter::OutputFor(const Note &note, bool pdf) const
{
	if (pdf)
	{
		auto target = OutputDir / note.RelativePath;
		target.replace_extension(".pdf");
		return target;
	}
	return OutputDir / "notes" / (note.Id + ".js");
}

std::vector<std::filesystem::path> Emitter::ViewerFiles() const
{
	return {OutputDir / "index.js", OutputDir / "index.html", OutputDir / "version.js"};
}

void Emitter::WriteNote(const Note &note, const std::vector<std::string> &pages)
{
	auto target = OutputFor(note, false);
	JSL::IO::mkdir(target.parent_path(), JSL::IO::Policy::Generous);

	std::string js = "CortexNote({\"id\":" + Utility::JsonString(note.Id) + ",\"title\":" + Utility::JsonString(note.Title) + ",\"pages\":[";
	for (size_t i = 0; i < pages.size(); ++i)
	{
		js += (i > 0 ? "," : "") + Utility::JsonString(pages[i]);
	}
	js += "]});\n";
	Utility::WriteAtomic(target, js);
}

void Emitter::WritePdf(const Note &note, const std::filesystem::path &pdf)
{
	auto target = OutputFor(note, true);
	JSL::IO::mkdir(target.parent_path(), JSL::IO::Policy::Generous);
	auto temporary = target;
	temporary += ".tmp";
	std::filesystem::copy_file(pdf, temporary, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::rename(temporary, target); // as in WriteAtomic: never a half-copied PDF in place of the old one
}

bool Emitter::WriteIndex(std::string_view indexJs, const ViewerConfig &viewer)
{
	JSL::IO::mkdir(OutputDir, JSL::IO::Policy::Generous);
	// viewer settings ride along in index.js, which is rewritten on every build: changing them never recompiles a note
	std::string config = "CortexConfig({\"zoom\":" + JSL::String::makeFrom(viewer.Zoom) + ",\"name\":" + Utility::JsonString(viewer.Name) + ",\"home\":" + Utility::JsonString(viewer.Home) + ",\"background\":" + Utility::JsonString(viewer.Background) + "});\n";
	std::string index = std::string(indexJs) + config;
	auto indexFile = OutputDir / "index.js";
	bool changed = !std::filesystem::exists(indexFile) || JSL::IO::getFile(indexFile) != index;
	if (changed)
	{
		Utility::WriteAtomic(indexFile, index);
	}
	Utility::WriteAtomic(OutputDir / "index.html", ViewerPage());
	return changed;
}

void Emitter::WriteVersion()
{
	// any value which differs from the last one will do: the time of this build, to the nanosecond
	auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
	Utility::WriteAtomic(OutputDir / "version.js", "CortexVersion(\"" + stamp + "\");\n");
}
