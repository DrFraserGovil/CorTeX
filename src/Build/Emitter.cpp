#include "Emitter.h"
#include "../Utility/Files.h"
#include <JSL.h>

// A deliberately minimal viewer: lists notes, and shows one when its link is clicked.
// The real shell (navigation, backlinks, link resolution) replaces this later.
static const char *StubViewer = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>CorTeX</title>
<style>
body { font-family: sans-serif; margin: 0; display: flex; }
nav { width: 16em; padding: 1em; border-right: 1px solid #ccc; min-height: 100vh; }
nav li.current a { font-weight: bold; }
nav .stale a, nav .failed a { color: #b35900; }
.banner { background: #fff3cd; border: 1px solid #e0b252; padding: 0.5em 1em; margin-bottom: 1em; }
.banner pre { white-space: pre-wrap; margin: 0.5em 0 0 0; }
main { padding: 1em; flex: 1; }
main svg { display: block; max-width: 100%; height: auto; margin-bottom: 1em; box-shadow: 0 0 4px #aaa; }
/* links inside notes: the SVG text is coloured here, so resolved and unresolved links can differ */
a.cortex-link text, a.cortex-link tspan { fill: #1a5fb4; }
a.cortex-link.unresolved text, a.cortex-link.unresolved tspan { fill: #c01c28; }
a.cortex-link:hover text, a.cortex-link:hover tspan { fill: #0b3d75; }
.backlinks { border-top: 1px solid #ccc; padding-top: 0.5em; color: #555; }
.backlinks h3 { margin: 0.2em 0; font-size: 1em; }
</style>
</head>
<body>
<nav><ul id="list"></ul></nav>
<main id="note">Select a note</main>
<script>
const cortexNotes = {};
const cortexOrder = [];
let currentId = null;

function CortexIndex(notes) {
	const list = document.getElementById("list");
	for (const n of notes) {
		cortexNotes[n.id] = n;
		cortexOrder.push(n);
		const li = document.createElement("li");
		li.dataset.id = n.id;
		li.className = n.status;
		const a = document.createElement("a");
		a.href = noteRoute(n.id);
		a.textContent = n.title + (n.status === "ok" ? "" : " (" + n.status + ")");
		li.appendChild(a);
		list.appendChild(li);
	}
}

function noteRoute(id) { return "#/n/" + encodeURIComponent(id); }

// A warning box; the diagnostic is set as text, never parsed as HTML
function banner(message, diagnostic) {
	const div = document.createElement("div");
	div.className = "banner";
	div.textContent = message;
	if (diagnostic) {
		const pre = document.createElement("pre");
		pre.textContent = diagnostic;
		div.appendChild(pre);
	}
	return div;
}

// A list of links to notes, under a heading (empty lists produce nothing)
function noteList(heading, ids) {
	const div = document.createElement("div");
	div.className = "backlinks";
	if (ids.length === 0) return div;
	const h = document.createElement("h3");
	h.textContent = heading;
	div.appendChild(h);
	const ul = document.createElement("ul");
	for (const id of ids) {
		const li = document.createElement("li");
		const a = document.createElement("a");
		a.href = noteRoute(id);
		a.textContent = cortexNotes[id] ? cortexNotes[id].title : id;
		li.appendChild(a);
		ul.appendChild(li);
	}
	div.appendChild(ul);
	return div;
}

function highlight(id) {
	for (const li of document.querySelectorAll("#list li")) li.classList.toggle("current", li.dataset.id === id);
}

// Called by notes/<id>.js once it has loaded
function CortexNote(note) {
	if (note.id !== currentId) return; // a slower load of a note the user has already navigated away from
	const main = document.getElementById("note");
	main.innerHTML = note.pages.join("");
	const entry = cortexNotes[note.id];
	if (entry) {
		if (entry.status === "stale") {
			main.prepend(banner("This note failed to rebuild; showing the last version that compiled.", entry.diagnostic));
		}
		// mark links which point at no note, so they can be styled differently
		for (const a of main.querySelectorAll("a.cortex-link")) {
			const href = a.getAttribute("xlink:href") || a.getAttribute("href") || "";
			const slug = decodeURIComponent(href.replace(/^#\/a\//, ""));
			if (entry.links[slug] === null) a.classList.add("unresolved");
		}
		main.appendChild(noteList("Linked from", entry.backlinks));
	}
	document.title = note.title;
}

function showNote(id) {
	currentId = id;
	highlight(id);
	const entry = cortexNotes[id];
	const main = document.getElementById("note");
	if (!entry) {
		main.replaceChildren(banner("There is no note with the id '" + id + "'."));
		return;
	}
	if (entry.status === "failed") {
		main.replaceChildren(banner("This note has not compiled successfully yet.", entry.diagnostic), noteList("Linked from", entry.backlinks));
		document.title = entry.title;
		return;
	}
	const s = document.createElement("script");
	s.src = "notes/" + id + ".js";
	s.onload = () => s.remove();
	document.body.appendChild(s);
}

// Following a [[link]]: the engine has already resolved every link from every note, so use the current note's answer;
// without one (e.g. the URL was opened directly) fall back to any note with that name, preferring the shallowest
function followLink(slug) {
	let target;
	const from = currentId ? cortexNotes[currentId] : null;
	if (from && slug in from.links) {
		target = from.links[slug];
	} else {
		const claimants = cortexOrder.filter(n => n.names.includes(slug));
		claimants.sort((a, b) => a.path.split("/").length - b.path.split("/").length || a.id.localeCompare(b.id));
		target = claimants.length ? claimants[0].id : null;
	}

	if (target) {
		location.replace(noteRoute(target)); // replace, so Back returns to the linking note rather than this redirect
		return;
	}

	currentId = null;
	highlight(null);
	const linkers = cortexOrder.filter(n => n.links[slug] === null).map(n => n.id);
	document.getElementById("note").replaceChildren(banner("No note is called '" + slug + "' yet."), noteList("Linked from", linkers));
	document.title = slug + " (missing)";
}

function show() {
	const route = location.hash.match(/^#\/([na])\/(.+)$/);
	if (!route) return;
	const value = decodeURIComponent(route[2]);
	if (route[1] === "n") showNote(value);
	else followLink(value);
}
window.addEventListener("hashchange", show);
</script>
<script src="index.js"></script>
<script>show();</script>
</body>
</html>
)HTML";

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
	return {OutputDir / "index.js", OutputDir / "index.html"};
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

void Emitter::WriteIndex(std::string_view indexJs)
{
	JSL::IO::mkdir(OutputDir, JSL::IO::Policy::Generous);
	Utility::WriteAtomic(OutputDir / "index.js", indexJs);
	Utility::WriteAtomic(OutputDir / "index.html", StubViewer);
}
