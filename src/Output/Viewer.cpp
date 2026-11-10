#include "Viewer.h"
#include "Layout.h"
#include "Notes/Header.h"
#include "Platform/Process.h"
#include "Util/Files.h"
#include "Util/Text.h"
#include "ViewerSource.h"
#include <JSL.h>
#include <chrono>

namespace
{
	using Util::Json;

	// The viewer (index.html) is assembled from src/Viewer, embedded into the binary at build time (see CMakeLists.txt): one
	// self-contained page, so it works from file:// with nothing else to install
	std::string Page()
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

	std::string Array(const std::vector<std::string> &items)
	{
		std::string out = "[";
		for (size_t i = 0; i < items.size(); ++i) out += (i > 0 ? "," : "") + Json(items[i]);
		return out + "]";
	}

	std::string IdOrNull(const std::string &id)
	{
		return id.empty() ? "null" : Json(id);
	}

	// Sidebar items as [{"label":...,"slug":...,"target":id or null}]; an item without a label shows its target's title, else its text
	std::string Sidebar(const std::vector<Network::SidebarItem> &items, const Network::Network &network)
	{
		std::string out = "[";
		for (size_t i = 0; i < items.size(); ++i)
		{
			auto &item = items[i];
			auto target = item.Target.empty() ? nullptr : network.Find(item.Target);
			auto label = Notes::PlainText(!item.Label.empty() ? item.Label : target ? target->Source->Fields.Title : item.Text);
			out += (i > 0 ? "," : "") + std::string("{\"label\":") + Json(label) + ",\"slug\":" + Json(item.Slug) + ",\"target\":" + IdOrNull(item.Target) + "}";
		}
		return out + "]";
	}

	// Sidebar groups as [{"heading":...,"items":[...]}]
	std::string Groups(const std::vector<Network::SidebarGroup> &groups, const Network::Network &network)
	{
		std::string out = "[";
		for (size_t g = 0; g < groups.size(); ++g)
		{
			out += (g > 0 ? "," : "") + std::string("{\"heading\":") + Json(groups[g].Heading) + ",\"items\":" + Sidebar(groups[g].Items, network) + "}";
		}
		return out + "]";
	}
} // namespace

std::string Output::IndexJs(const Network::Network &network, const Build::BuildState &state, const Settings::Config &config, const ViewerOptions &options,
							Report::Log &report)
{
	/// ONE ENTRY PER NOTE
	std::string js = "CortexIndex([\n";
	for (size_t i = 0; i < network.Nodes.size(); ++i)
	{
		auto &node = network.Nodes[i];
		auto &note = *node.Source;

		// the status comes from the last time the note was compiled, whether that was this build or an earlier one
		std::string status = "failed", diagnostic = "not yet built";
		if (note.IsStub())
		{
			status = "redirect";
			diagnostic = "";
		}
		else if (auto record = state.Records.find(note.Id); record != state.Records.end())
		{
			status = record->second.Succeeded ? "ok" : record->second.HasOutput ? "stale" : "failed";
			diagnostic = record->second.Diagnostic;
		}

		js += (i > 0 ? ",\n" : "");
		js += "{\"id\":" + Json(note.Id) + ",\"key\":" + Json(note.Key) + ",\"title\":" + Json(Notes::PlainText(note.Fields.Title)) + ",\"path\":" + Json(note.Path.generic_string());
		js += ",\"status\":" + Json(status) + ",\"diagnostic\":" + Json(diagnostic);
		if (note.Fields.GraphHidden) js += ",\"graphHidden\":true";
		if (note.Fields.Todo) js += ",\"todo\":" + Json(*note.Fields.Todo);
		if (note.IsStub()) js += ",\"redirect\":" + IdOrNull(node.Redirect) + ",\"redirectText\":" + Json(note.Fields.Redirect);
		js += ",\"names\":" + Array(node.Names);

		// links as {key: target id, or null if unresolved}, so the viewer can follow a link with one lookup
		js += ",\"links\":{";
		std::vector<std::string> loose;
		for (size_t l = 0; l < node.Links.size(); ++l)
		{
			auto &link = node.Links[l];
			js += (l > 0 ? "," : "") + Json(link.Key) + ":" + IdOrNull(link.Target);
			if (link.Loose) loose.push_back(link.Key);
		}
		js += "}";
		if (!loose.empty()) js += ",\"loose\":" + Array(loose);
		js += ",\"backlinks\":" + Array(node.Backlinks) + ",\"transcludes\":" + Array(node.Transcludes);
		js += ",\"sidebar\":" + Groups(node.Sidebar, network) + "}";
	}
	js += "\n]);\n";

	/// THE GLOBAL SIDEBAR
	js += "CortexSidebar(" + Groups(network.Sidebar, network) + ");\n";

	/// THE VIEWER'S SETTINGS: THEY RIDE ALONG HERE, SO CHANGING THEM NEVER RECOMPILES A NOTE
	// the home note is given as a path, matched against the notes actually built (so ignored or untitled files can't be home)
	std::string home;
	auto &display = config.Viewer;
	if (!display.Home.empty())
	{
		auto wanted = std::filesystem::path(display.Home).lexically_normal().generic_string();
		for (auto &node : network.Nodes)
		{
			if (node.Source->Path.generic_string() == wanted) home = node.Id;
		}
		if (home.empty())
		{
			report.Add(Report::Kind::Setting, "home '" + display.Home + "' isn't a note in this project (missing, ignored, or without a title); the viewer will open on the overview",
					   ".cortex/settings.config");
		}
	}
	// the logo is embedded, like note images: the viewer never depends on a copy of a project file
	std::string logo = display.Logo.empty() ? "" : Util::ImageDataUri(display.Logo).value_or("");
	// the editor tools only work on this machine, where links are handled: a copy to share, or another platform, leaves them out entirely
	bool acts = !options.ForSharing && Platform::EditorIntegration;
	std::vector<std::string> editors;
	if (acts)
	{
		for (auto &editor : config.Editors) editors.push_back(editor.Name);
	}
	js += "CortexConfig({\"format\":" + std::to_string(Format) + ",\"name\":" + Json(options.Name) + ",\"home\":" + Json(home);
	js += ",\"zoom\":" + JSL::String::makeFrom(display.Zoom) + ",\"background\":" + Json(display.Background) + ",\"logo\":" + Json(logo);
	js += ",\"graphLayout\":" + Json(display.GraphLayout) + ",\"folderPull\":" + JSL::String::makeFrom(display.FolderPull);
	js += ",\"folderRepulsion\":" + JSL::String::makeFrom(display.FolderRepulsion) + ",\"graphColours\":" + Json(display.GraphColours);
	js += ",\"root\":" + Json(acts ? config.Root.string() : "") + ",\"editors\":" + Array(editors);
	js += std::string(",\"live\":") + (options.ForSharing ? "false" : "true") + "});\n";
	return js;
}

void Output::WriteNote(const std::filesystem::path &outputDir, const Notes::Note &note, const std::string &svg)
{
	auto target = NoteFile(outputDir, note, false);
	std::filesystem::create_directories(target.parent_path());
	Util::WriteAtomic(target, "CortexNote({\"id\":" + Json(note.Id) + ",\"title\":" + Json(Notes::PlainText(note.Fields.Title)) + ",\"pages\":[" + Json(svg) + "]});\n");
}

bool Output::WriteIndex(const std::filesystem::path &outputDir, const std::string &indexJs)
{
	std::filesystem::create_directories(outputDir);
	auto indexFile = outputDir / "index.js";
	bool changed = !std::filesystem::exists(indexFile) || JSL::IO::getFile(indexFile) != indexJs;
	if (changed) Util::WriteAtomic(indexFile, indexJs);
	Util::WriteAtomic(outputDir / "index.html", Page());
	return changed;
}

void Output::WriteVersion(const std::filesystem::path &outputDir)
{
	// any value which differs from the last one will do: the time of this build, to the nanosecond
	auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
	Util::WriteAtomic(outputDir / "version.js", "CortexVersion(\"" + stamp + "\");\n");
}
