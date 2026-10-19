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
nav ul { padding-left: 1.2em; }
#home-links { margin-bottom: 0.8em; font-weight: bold; }
#home-links a { margin-right: 0.8em; }
.overview h1 { margin-top: 0; }
.overview h2 { font-size: 1.1em; border-bottom: 1px solid #ccc; padding-bottom: 0.2em; }
.overview h3 { font-size: 1em; color: #555; margin-bottom: 0.2em; }
.overview .stale a, .overview .failed a { color: #b35900; }
.overview .note-error { color: #555; font-family: monospace; font-size: 0.9em; margin-left: 0.5em; }
#zoom { display: flex; gap: 0.3em; align-items: center; margin-bottom: 1em; font-size: 0.9em; color: #555; }
#zoom button { min-width: 2em; }
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
<nav><div id="home-links"><a href="#/">Home</a> <a id="overview-link" href="#/overview">Overview</a></div><div id="zoom"><button id="zoom-out" title="Smaller">&minus;</button><span id="zoom-level"></span><button id="zoom-in" title="Larger">+</button><button id="zoom-reset" title="The project's default size">reset</button></div><ul id="list"></ul></nav>
<main id="note">Select a note</main>
<script>
const cortexNotes = {};
const cortexOrder = [];
const cortexByKey = {}; // links and transclusions name their origin note by key
let currentId = null;

// Zoom: the project's default (from index.js), optionally overridden by this reader (remembered in browser storage, where available).
// Notes are vector images, so any zoom is sharp, and it never changes where lines break.
let projectZoom = 1;
let readerZoom = null;
try { const saved = parseFloat(localStorage.getItem("cortex-zoom")); if (saved > 0) readerZoom = saved; } catch (e) {}
let projectName = "CorTeX";
let homeId = "";
function CortexConfig(config) {
	if (config.zoom > 0) projectZoom = config.zoom;
	if (config.name) projectName = config.name;
	homeId = config.home || "";
	// set as a style property (never inserted as text), so the setting can't inject anything; an invalid colour is simply ignored
	if (config.background) document.body.style.backgroundColor = config.background;
	// with no home note, Home already is the overview, so a separate link would be redundant
	document.getElementById("overview-link").style.display = homeId ? "" : "none";
	applyZoom();
}
function zoom() { return readerZoom || projectZoom; }
function setReaderZoom(value) {
	readerZoom = value;
	try { if (value) localStorage.setItem("cortex-zoom", value); else localStorage.removeItem("cortex-zoom"); } catch (e) {}
	applyZoom();
}
// draw each note at its natural size (dvisvgm gives it in pt; 1pt = 4/3 CSS px) times the zoom; max-width still shrinks it to fit a narrow window
function applyZoom() {
	document.getElementById("zoom-level").textContent = Math.round(zoom() * 100) + "%";
	for (const svg of document.querySelectorAll("#note svg")) {
		const natural = parseFloat(svg.getAttribute("width"));
		if (natural > 0) svg.style.width = (natural * 4 / 3 * zoom()) + "px";
	}
}
document.getElementById("zoom-in").onclick = () => setReaderZoom(Math.round(zoom() * 110) / 100);
document.getElementById("zoom-out").onclick = () => setReaderZoom(Math.round(zoom() / 1.1 * 100) / 100);
document.getElementById("zoom-reset").onclick = () => setReaderZoom(null);

function CortexIndex(notes) {
	const list = document.getElementById("list");
	for (const n of notes) {
		cortexNotes[n.id] = n;
		cortexByKey[n.key] = n;
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
		// mark links which point at no note, so they can be styled differently; each link is judged from the note it was written in
		// (which, inside a transcluded note, isn't the note being shown)
		for (const a of main.querySelectorAll("a.cortex-link")) {
			const href = a.getAttribute("xlink:href") || a.getAttribute("href") || "";
			const [origin, slug] = parseLink(decodeURIComponent(href.replace(/^#\/a\//, "")));
			if (resolveLink(origin, slug) === null) a.classList.add("unresolved");
		}
		main.appendChild(noteList("Linked from", entry.backlinks));
	}
	applyZoom();
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

// A link route is "<origin key>/<slug>" (from a note) or just "<slug>" (typed, or from the overview)
function parseLink(value) {
	const slash = value.indexOf("/");
	return slash < 0 ? [null, value] : [value.slice(0, slash), value.slice(slash + 1)];
}

// The engine has already resolved every link from the note it was written in, so use that note's answer;
// without one (no origin, e.g. from the overview) fall back to any note with that name, preferring the shallowest
function resolveLink(origin, slug) {
	const from = origin ? cortexByKey[origin] : null;
	if (from && slug in from.links) return from.links[slug];
	const claimants = cortexOrder.filter(n => n.names.includes(slug));
	claimants.sort((a, b) => a.path.split("/").length - b.path.split("/").length || a.id.localeCompare(b.id));
	return claimants.length ? claimants[0].id : null;
}

// Following a [[link]]
function followLink(value) {
	const [origin, slug] = parseLink(value);
	const target = resolveLink(origin, slug);

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

// The constructed overview: built entirely from index.js, so it costs no TeX
function element(tag, text, className) {
	const e = document.createElement(tag);
	if (text) e.textContent = text;
	if (className) e.className = className;
	return e;
}
function noteLink(id) {
	const a = element("a", cortexNotes[id] ? cortexNotes[id].title : id);
	a.href = noteRoute(id);
	return a;
}
function showOverview() {
	currentId = null;
	highlight(null);
	const page = element("div", "", "overview");
	page.appendChild(element("h1", projectName));

	/// every note, grouped by folder (the folder structure as a table of contents)
	page.appendChild(element("h2", "Notes"));
	const folders = {};
	for (const n of cortexOrder) {
		const folder = n.path.includes("/") ? n.path.slice(0, n.path.lastIndexOf("/")) : "";
		(folders[folder] = folders[folder] || []).push(n);
	}
	for (const folder of Object.keys(folders).sort()) {
		if (folder) page.appendChild(element("h3", folder + "/"));
		const ul = element("ul");
		for (const n of folders[folder].sort((a, b) => a.title.localeCompare(b.title))) {
			const li = element("li", "", n.status);
			li.appendChild(noteLink(n.id));
			if (n.status !== "ok") li.appendChild(document.createTextNode(" (" + n.status + ")"));
			ul.appendChild(li);
		}
		page.appendChild(ul);
	}

	/// wanted notes: link targets which no note answers to yet
	const wanted = {};
	for (const n of cortexOrder) {
		for (const [slug, target] of Object.entries(n.links)) {
			if (target === null) (wanted[slug] = wanted[slug] || []).push(n.id);
		}
	}
	if (Object.keys(wanted).length) {
		page.appendChild(element("h2", "Wanted notes"));
		const ul = element("ul");
		for (const slug of Object.keys(wanted).sort()) {
			const li = element("li");
			const a = element("a", slug);
			a.href = "#/a/" + encodeURIComponent(slug);
			li.appendChild(a);
			li.appendChild(document.createTextNode(", linked from "));
			wanted[slug].forEach((id, k) => { if (k) li.appendChild(document.createTextNode(", ")); li.appendChild(noteLink(id)); });
			ul.appendChild(li);
		}
		page.appendChild(ul);
	}

	/// build problems
	const problems = cortexOrder.filter(n => n.status !== "ok");
	if (problems.length) {
		page.appendChild(element("h2", "Build problems"));
		const ul = element("ul");
		for (const n of problems) {
			const li = element("li", "", n.status);
			li.appendChild(noteLink(n.id));
			li.appendChild(document.createTextNode(n.status === "stale" ? " (showing its last good version)" : " (never compiled)"));
			li.appendChild(element("div", n.diagnostic, "note-error"));
			ul.appendChild(li);
		}
		page.appendChild(ul);
	}

	document.getElementById("note").replaceChildren(page);
	document.title = projectName;
}

function show() {
	const hash = location.hash;
	if (hash === "" || hash === "#" || hash === "#/") {
		// the landing page: the chosen home note, or the overview if there isn't one
		if (homeId && cortexNotes[homeId]) location.replace(noteRoute(homeId));
		else showOverview();
		return;
	}
	if (hash === "#/overview") {
		showOverview();
		return;
	}
	const route = hash.match(/^#\/([na])\/(.+)$/);
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

void Emitter::WriteIndex(std::string_view indexJs, const ViewerConfig &viewer)
{
	JSL::IO::mkdir(OutputDir, JSL::IO::Policy::Generous);
	// viewer settings ride along in index.js, which is rewritten on every build: changing them never recompiles a note
	std::string config = "CortexConfig({\"zoom\":" + JSL::String::makeFrom(viewer.Zoom) + ",\"name\":" + Utility::JsonString(viewer.Name) + ",\"home\":" + Utility::JsonString(viewer.Home) + ",\"background\":" + Utility::JsonString(viewer.Background) + "});\n";
	Utility::WriteAtomic(OutputDir / "index.js", std::string(indexJs) + config);
	Utility::WriteAtomic(OutputDir / "index.html", StubViewer);
}
