const cortexNotes = {};
const cortexOrder = [];
const cortexByKey = {}; // links and transclusions name their origin note by key
let currentId = null;

// Zoom: the project's default (from index.js), optionally overridden by this reader (remembered in browser storage, where available).
// Notes are vector images, so any zoom is sharp, and it never changes where lines break.
let projectZoom = 1;
let folderPull = 0.5; // the graph's pull between neighbouring folders, relative to within a folder (the folder-pull setting)
let readerZoom = null;
try { const saved = parseFloat(localStorage.getItem("cortex-zoom")); if (saved > 0) readerZoom = saved; } catch (e) {}
let projectName = "CorTeX";
let homeId = "";
let projectRoot = ""; // the absolute project root, which cortex:// links name
let editors = [];     // the names of the project's editors (the commands stay in the settings)
function CortexConfig(config) {
	if (config.zoom > 0) projectZoom = config.zoom;
	if (config.folderPull >= 0 && config.folderPull <= 1) folderPull = config.folderPull;
	if (config.name) projectName = config.name;
	document.getElementById("project-name").textContent = config.name || "";
	homeId = config.home || "";
	// set as a style property (never inserted as text), so the setting can't inject anything; an invalid colour is simply ignored
	if (config.background) document.body.style.backgroundColor = config.background;
	// with no home note, Home already is the overview, so a separate link would be redundant
	document.getElementById("overview-link").style.display = homeId ? "" : "none";
	projectRoot = config.root || "";
	editors = Array.isArray(config.editors) ? config.editors : [];
	const logo = document.getElementById("logo");
	if (config.logo) logo.src = config.logo; else logo.removeAttribute("src");
	logo.classList.toggle("shown", !!config.logo);
	applyZoom();
}

// The editor this reader picked for the session (remembered per project, like zoom; the archive is never changed); else the project's first
function chosenEditor() {
	let saved = null;
	try { saved = localStorage.getItem("cortex-editor:" + projectRoot); } catch (e) {}
	return editors.includes(saved) ? saved : editors[0];
}

// A link to the cortex:// handler, which does what a file:// page can't (run an editor, change a file); it names only the project,
// the note and an editor's name, and the handler checks all three against the project before acting
function cortexAction(action, params) {
	params = Object.assign({ root: projectRoot }, params);
	return "cortex://" + action + "?" + Object.entries(params).map(([k, v]) => k + "=" + encodeURIComponent(v)).join("&");
}

// The sidebar's Editor section for the open note, a list like the sidebar's groups: [editor choice] Edit (the choice only if the project
// has several editors), then the needs-work toggle. On the page of a note which doesn't exist yet (wanted = {slug, dir}), Edit becomes
// Create & edit, which writes the note into that folder first. Cleared (null) on every other page
function setNoteTools(entry, wanted = null) {
	const tools = document.getElementById("note-tools");
	tools.replaceChildren();
	if ((!entry && !wanted) || !projectRoot) return;
	const items = [];
	if (editors.length) {
		const li = document.createElement("li");
		const edit = element("a", wanted ? "Create & edit" : "Edit");
		if (wanted) edit.title = "Writes " + (wanted.dir ? wanted.dir + "/" : "") + wanted.slug + ".tex (titled, and aliased, '" + wanted.slug + "') and opens it";
		const update = () => {
			edit.href = wanted ? cortexAction("create", { dir: wanted.dir, slug: wanted.slug, editor: chosenEditor() })
				: cortexAction("edit", { note: entry.path, editor: chosenEditor() });
		};
		update();
		if (editors.length > 1) {
			const pick = document.createElement("select");
			pick.title = "Which editor Edit opens (for this browser only)";
			for (const name of editors) pick.appendChild(new Option(name, name, false, name === chosenEditor()));
			pick.onchange = () => {
				try { localStorage.setItem("cortex-editor:" + projectRoot, pick.value); } catch (e) {}
				update();
			};
			li.appendChild(pick);
		}
		li.appendChild(edit);
		items.push(li);
	}
	if (entry) {
		const flagged = entry.todo !== undefined;
		const toggle = element("a", flagged ? "Clear needs work" : "Mark needs work");
		toggle.href = cortexAction("todo", { note: entry.path, set: flagged ? "off" : "on" });
		toggle.title = "Changes the note's todo: header line (the page refreshes once the project rebuilds)";
		const li = document.createElement("li");
		li.appendChild(toggle);
		items.push(li);
	}
	if (!items.length) return;

	const list = document.createElement("ul");
	list.append(...items);
	tools.append(element("h4", "Editor"), list);
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
	let widest = 0;
	for (const svg of document.querySelectorAll("#note > svg")) {
		const natural = parseFloat(svg.getAttribute("width"));
		if (natural > 0) svg.style.width = (natural * 4 / 3 * zoom()) + "px";
		widest = Math.max(widest, natural * 4 / 3 * zoom());
	}
	// the Connections panel under a note is as wide as the note's page (never wider than the window: max-width in the stylesheet)
	for (const panel of document.querySelectorAll("#note > .local-graph")) panel.style.width = widest > 0 ? widest + "px" : "";
}
document.getElementById("zoom-in").onclick = () => setReaderZoom(Math.round(zoom() * 110) / 100);
document.getElementById("zoom-out").onclick = () => setReaderZoom(Math.round(zoom() / 1.1 * 100) / 100);
document.getElementById("zoom-reset").onclick = () => setReaderZoom(null);

function CortexIndex(notes) {
	// may be called again when the viewer refreshes after a rebuild, so it starts from scratch
	for (const key of Object.keys(cortexNotes)) delete cortexNotes[key];
	for (const key of Object.keys(cortexByKey)) delete cortexByKey[key];
	cortexOrder.length = 0;
	for (const n of notes) {
		cortexNotes[n.id] = n;
		cortexByKey[n.key] = n;
		cortexOrder.push(n);
	}
}

// The sidebar is curated, not a list of every note (that's the overview): the project's groups from registry.ctx, then the open
// note's own entries from its sidebar: header. Everything arrives already resolved by the engine
let globalSidebar = [];
function CortexSidebar(groups) {
	globalSidebar = groups;
	renderSidebar();
}
function sidebarItem(item) {
	const li = document.createElement("li");
	const a = document.createElement("a");
	a.textContent = item.label;
	if (item.target) {
		a.href = noteRoute(item.target);
		const note = cortexNotes[item.target];
		if (note && note.status !== "ok") li.className = note.status;
		if (item.target === currentId) li.classList.add("current");
	} else {
		a.href = "#/a/" + encodeURIComponent(item.slug); // the "no note called ... yet" page
		li.className = "unresolved";
	}
	li.appendChild(a);
	return li;
}
function sidebarGroup(heading, items) {
	const frag = document.createDocumentFragment();
	if (heading) {
		const h = document.createElement("h4");
		h.textContent = heading;
		frag.appendChild(h);
	}
	const ul = document.createElement("ul");
	items.forEach(item => ul.appendChild(sidebarItem(item)));
	frag.appendChild(ul);
	return frag;
}
// Search: filters every note by title, alias and folder as you type, entirely in the browser from index.js.
// Typing / anywhere focuses it; arrows move through the results, Enter opens one, Escape clears
function slugify(text) {
	return text.toLowerCase().replace(/[^a-z0-9_-]+/g, "-").replace(/-+/g, "-").replace(/^-|-$/g, "");
}
let searchResults = [];
let searchSelected = 0;
function searchScore(note, query, querySlug) {
	const title = note.title.toLowerCase();
	if (title.startsWith(query)) return 0;
	if (title.includes(" " + query)) return 1;
	if (title.includes(query)) return 2;
	if (querySlug && note.names.some(name => name.startsWith(querySlug))) return 3;
	if (querySlug && note.names.some(name => name.includes(querySlug))) return 4;
	if (note.path.toLowerCase().includes(query)) return 5;
	return -1;
}
function runSearch() {
	const query = document.getElementById("search").value.trim().toLowerCase();
	const results = document.getElementById("results");
	const sidebar = document.getElementById("sidebar");
	if (!query) {
		results.replaceChildren();
		sidebar.style.display = "";
		searchResults = [];
		return;
	}
	const querySlug = slugify(query);
	searchResults = cortexOrder
		.map(note => [searchScore(note, query, querySlug), note])
		.filter(([score]) => score >= 0)
		.sort((a, b) => a[0] - b[0] || a[1].title.localeCompare(b[1].title))
		.slice(0, 20)
		.map(([, note]) => note);
	searchSelected = Math.min(searchSelected, Math.max(searchResults.length - 1, 0));

	sidebar.style.display = "none";
	if (!searchResults.length) {
		results.replaceChildren(element("div", "No notes match", "none"));
		return;
	}
	const ul = element("ul");
	searchResults.forEach((note, i) => {
		const li = element("li", "", (i === searchSelected ? "selected " : "") + note.status);
		li.appendChild(noteLink(note.id));
		const folder = note.path.includes("/") ? note.path.slice(0, note.path.lastIndexOf("/")) + "/" : "";
		if (folder) li.appendChild(element("span", folder, "where"));
		ul.appendChild(li);
	});
	results.replaceChildren(ul);
}
function clearSearch() {
	const box = document.getElementById("search");
	box.value = "";
	searchSelected = 0;
	runSearch();
	box.blur();
}
document.getElementById("search").addEventListener("input", () => { searchSelected = 0; runSearch(); });
document.getElementById("search").addEventListener("keydown", e => {
	if (e.key === "ArrowDown" || e.key === "ArrowUp") {
		e.preventDefault();
		const step = e.key === "ArrowDown" ? 1 : -1;
		searchSelected = Math.max(0, Math.min(searchResults.length - 1, searchSelected + step));
		runSearch();
	} else if (e.key === "Enter" && searchResults.length) {
		const id = searchResults[searchSelected].id;
		clearSearch();
		location.hash = noteRoute(id);
	} else if (e.key === "Escape") {
		clearSearch();
	}
});
// clicking a result navigates; the search then gets out of the way
document.getElementById("results").addEventListener("click", e => { if (e.target.closest("a")) clearSearch(); });
document.addEventListener("keydown", e => {
	if (e.key === "/" && document.activeElement !== document.getElementById("search")) {
		e.preventDefault();
		document.getElementById("search").focus();
	}
});

function renderSidebar() {
	const sidebar = document.getElementById("sidebar");
	sidebar.replaceChildren();
	const shown = new Set();
	for (const group of globalSidebar) {
		sidebar.appendChild(sidebarGroup(group.heading, group.items));
		group.items.forEach(item => shown.add(item.target || item.slug));
	}
	// the open note's own entries, beneath, without repeating any already in the global list
	const entry = currentId ? cortexNotes[currentId] : null;
	const local = entry ? entry.sidebar.filter(item => !shown.has(item.target || item.slug)) : [];
	if (local.length) sidebar.appendChild(sidebarGroup("This note", local));
	if (document.getElementById("search").value.trim()) runSearch(); // e.g. after a live refresh brought in new notes
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
	// the sidebar shows the open note's own entries and marks the current note, so it is redrawn on every navigation
	renderSidebar();
}

// Called by notes/<id>.js once it has loaded
function CortexNote(note) {
	if (note.id !== currentId) return; // a slower load of a note the user has already navigated away from
	const main = document.getElementById("note");
	main.innerHTML = note.pages.join("");
	const entry = cortexNotes[note.id];
	if (entry) {
		if (entry.todo !== undefined) {
			const flag = banner("Flagged as needing work" + (entry.todo ? ": " + entry.todo : "."));
			flag.classList.add("todo");
			main.prepend(flag);
		}
		if (entry.status === "stale") {
			main.prepend(banner("This note failed to rebuild; showing the last version that compiled.", entry.diagnostic));
		}
		setNoteTools(entry);
		// mark links which point at no note, so they can be styled differently; each link is judged from the note it was written in
		// (which, inside a transcluded note, isn't the note being shown)
		for (const a of main.querySelectorAll("a.cortex-link")) {
			const href = a.getAttribute("xlink:href") || a.getAttribute("href") || "";
			const [origin, slug] = parseLink(decodeURIComponent(href.replace(/^#\/a\//, "")));
			if (resolveLink(origin, slug) === null) a.classList.add("unresolved");
		}
		// backlinks appear as one line under the graph (a note with backlinks always has connections, so always has the graph)
		const local = localGraph(note.id);
		if (local) {
			main.appendChild(local);
			applyZoom(); // sizes the panel to match the page before the graph measures its box
			local.draw();
		}
	}
	document.getElementById("zoom").classList.add("shown");
	applyZoom();
	restoreScroll(); // after a live refresh, stay where the reader was
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
	if (entry.status === "redirect") {
		followRedirect(entry);
		return;
	}
	if (entry.status === "failed") {
		setNoteTools(entry);
		main.replaceChildren(banner("This note has not compiled successfully yet.", entry.diagnostic), noteList("Linked from", entry.backlinks));
		document.title = entry.title;
		return;
	}
	loadScript("notes/" + id + ".js");
}

// A redirect stub has no page of its own: go straight on to the note it points at (replacing the URL, so Back skips the stub),
// following a chain of stubs if need be. A broken or looping redirect gets an explanation instead
function followRedirect(entry) {
	const seen = new Set();
	let current = entry;
	while (current.status === "redirect") {
		if (seen.has(current.id)) {
			document.getElementById("note").replaceChildren(banner("'" + entry.title + "' redirects in a loop, so it can't be shown."));
			document.title = entry.title;
			return;
		}
		seen.add(current.id);
		if (!current.redirect || !cortexNotes[current.redirect]) {
			const page = banner("'" + current.title + "' redirects to '" + current.redirectText + "', which doesn't exist yet.");
			document.getElementById("note").replaceChildren(page, noteList("Linked from", current.backlinks));
			document.title = entry.title;
			return;
		}
		current = cortexNotes[current.redirect];
	}
	location.replace(noteRoute(current.id));
}

// Loads a script afresh (the ?t= stops the browser reusing an old copy after a rebuild), removing the tag once it has run
function loadScript(src, then) {
	const s = document.createElement("script");
	s.src = src + "?t=" + Date.now();
	s.onload = () => { s.remove(); if (then) then(); };
	s.onerror = () => s.remove();
	document.body.appendChild(s);
}

// Live refresh: every build that changes anything writes a new stamp to version.js; while this tab is visible, it is checked
// every second, and a new stamp reloads the index and the page on show, keeping the scroll position
let cortexVersion = null;
let pendingScroll = null;
function CortexVersion(stamp) {
	if (cortexVersion === null) { cortexVersion = stamp; return; }
	if (stamp === cortexVersion) return;
	cortexVersion = stamp;
	pendingScroll = window.scrollY;
	// a note restores the scroll position itself once it has re-rendered (it loads asynchronously); other pages draw at once
	loadScript("index.js", () => { show(); if (!location.hash.startsWith("#/n/")) restoreScroll(); });
}
function restoreScroll() {
	if (pendingScroll === null) return;
	window.scrollTo(0, pendingScroll);
	pendingScroll = null;
}
setInterval(() => { if (document.visibilityState === "visible") loadScript("version.js"); }, 1000);

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
	// a new note goes beside the note whose link led here (else the first note wanting it): that's almost always where it belongs
	const beside = (origin && cortexByKey[origin]) || cortexNotes[linkers[0]];
	if (beside) setNoteTools(null, { slug: slug, dir: beside.path.split("/").slice(0, -1).join("/") });
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

	/// every note, grouped by folder (the folder structure as a table of contents); redirect stubs are left out
	page.appendChild(element("h2", "Notes"));
	const folders = {};
	for (const n of cortexOrder) {
		if (n.status === "redirect") continue;
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

	/// notes flagged as needing work (they're listed above too)
	const todo = cortexOrder.filter(n => n.todo !== undefined).sort((a, b) => a.title.localeCompare(b.title));
	if (todo.length) {
		page.appendChild(element("h2", "Needs work"));
		const ul = element("ul");
		for (const n of todo) {
			const li = element("li", "", "todo");
			li.appendChild(noteLink(n.id));
			if (n.todo) li.appendChild(document.createTextNode(": " + n.todo));
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
	const problems = cortexOrder.filter(n => n.status === "stale" || n.status === "failed");
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
	setNoteTools(null); // a note's own page puts them back once it has loaded
	document.getElementById("zoom").classList.remove("shown"); // likewise the zoom control, which only means anything on a note
	const hash = location.hash;
	if (hash === "" || hash === "#" || hash === "#/") {
		// the landing page: the chosen home note, or the overview if there isn't one
		if (homeId && cortexNotes[homeId]) location.replace(noteRoute(homeId));
		else showOverview();
		return;
	}
	if (hash === "#/overview") {
		stopGraphs();
		showOverview();
		return;
	}
	if (hash === "#/graph") {
		showGraph();
		return;
	}
	stopGraphs();
	const route = hash.match(/^#\/([na])\/(.+)$/);
	if (!route) return;
	const value = decodeURIComponent(route[2]);
	if (route[1] === "n") showNote(value);
	else followLink(value);
}
window.addEventListener("hashchange", show);

// ============================================================================================================
// THE GRAPH: how notes link together, drawn with a small force layout (no library: the viewer must work offline
// from file://). Used for the whole project (#/graph) and for a note's neighbourhood (under each note)
// ============================================================================================================

const SVG_NS = "http://www.w3.org/2000/svg";
// Folder colours: hues evenly spaced around the colour wheel, all at the same saturation and lightness (so equally readable)
const GRAPH_HUES = 24;
let activeGraphs = []; // running simulations, stopped whenever the page changes

function stopGraphs() {
	activeGraphs.forEach(g => g.stop());
	activeGraphs = [];
}

// A deterministic number from a string (so a note's colour and starting position are the same every time)
function hashString(text) {
	let h = 2166136261;
	for (let i = 0; i < text.length; ++i) h = Math.imul(h ^ text.charCodeAt(i), 16777619);
	return h >>> 0;
}

// Where a note finally lands, through any redirect stubs (null if the chain is broken or loops)
function finalNote(id) {
	const seen = new Set();
	while (id && cortexNotes[id] && cortexNotes[id].status === "redirect") {
		if (seen.has(id)) return null;
		seen.add(id);
		id = cortexNotes[id].redirect;
	}
	return id && cortexNotes[id] ? id : null;
}

// Steps through the folder tree between two notes' folders (same folder 0; a/x ↔ a/y 2; a ↔ b/c 3).
// Wanted notes have no folder, and count as 0 from anything
function folderDistance(a, b) {
	if (!a.dirs || !b.dirs) return 0;
	let shared = 0;
	while (shared < a.dirs.length && shared < b.dirs.length && a.dirs[shared] === b.dirs[shared]) ++shared;
	return (a.dirs.length - shared) + (b.dirs.length - shared);
}

// Nodes and edges for the whole project: notes (stubs folded into their targets), plus "wanted" nodes for link targets
// which don't exist yet. Each edge is a link or a transclusion (if a note does both, it counts as a transclusion).
// Notes marked `graph: hidden` (and their edges) are left out, except for `centre`: a hidden note still has its own graph
function graphModel(centre = null) {
	const shown = n => !n.graphHidden || n.id === centre;
	const nodes = new Map();
	const edges = new Map();
	const folderOf = path => path.includes("/") ? path.slice(0, path.indexOf("/")) : "";
	for (const n of cortexOrder) {
		if (n.status === "redirect" || !shown(n)) continue;
		nodes.set(n.id, { id: n.id, title: n.title, folder: folderOf(n.path), dirs: n.path.split("/").slice(0, -1), status: n.status, wanted: false, todo: n.todo });
	}
	for (const n of cortexOrder) {
		if (n.status === "redirect" || !shown(n)) continue;
		for (const [slug, target] of Object.entries(n.links)) {
			let to;
			if (target === null) {
				to = "wanted:" + slug;
				if (!nodes.has(to)) nodes.set(to, { id: to, title: slug, folder: null, status: "wanted", wanted: true, slug });
			} else {
				to = finalNote(target);
				if (!to || to === n.id || !nodes.has(to)) continue; // (a hidden note isn't in the model)
			}
			const kind = n.transcludes.includes(target) ? "transclude" : "link";
			const key = n.id + "\u0000" + to;
			if (!edges.has(key) || kind === "transclude") edges.set(key, { from: n.id, to, kind });
		}
	}
	return { nodes: [...nodes.values()], edges: [...edges.values()] };
}

// Every node's distance from a note, counting links in either direction (unreachable nodes are absent)
function distancesFrom(model, centre) {
	const adjacent = new Map(model.nodes.map(n => [n.id, []]));
	for (const e of model.edges) {
		adjacent.get(e.from).push(e.to);
		adjacent.get(e.to).push(e.from);
	}
	const distance = new Map([[centre, 0]]);
	const queue = [centre];
	while (queue.length) {
		const id = queue.shift();
		for (const next of adjacent.get(id) || []) {
			if (!distance.has(next)) { distance.set(next, distance.get(id) + 1); queue.push(next); }
		}
	}
	return distance;
}

// The part of the model within `depth` links (either direction) of a note; depth null keeps everything
function neighbourhood(model, centre, depth) {
	const distance = distancesFrom(model, centre);
	const keep = id => depth === null || (distance.has(id) && distance.get(id) <= depth);
	return {
		nodes: model.nodes.filter(n => keep(n.id)),
		edges: model.edges.filter(e => keep(e.from) && keep(e.to)),
		distance,
	};
}

// The colour of each top-level folder in the project. Each folder starts at the hue its name hashes to and, if another folder
// already has that one, steps on (7 hues at a time, so a clash lands on a clearly different colour) to the first free hue: colours
// only repeat once there are more folders than hues. Folders are placed in sorted order, so the result is the same on every build,
// and across the whole project, so a note has the same colour in every graph
function folderColours() {
	const folders = [...new Set(cortexOrder.map(n => n.path.includes("/") ? n.path.slice(0, n.path.indexOf("/")) : ""))].sort();
	const taken = new Set(), colours = new Map();
	for (const folder of folders) {
		let hue = hashString(folder || "(top)") % GRAPH_HUES;
		for (let tries = 0; taken.has(hue) && tries < GRAPH_HUES; ++tries) hue = (hue + 7) % GRAPH_HUES;
		taken.add(hue);
		colours.set(folder, `hsl(${Math.round(hue * 360 / GRAPH_HUES)}, 55%, 45%)`);
	}
	return colours;
}

// Draws a model into a container and runs the layout. Returns an object with stop()
function drawGraph(container, model, options = {}) {
	const width = container.clientWidth || 800, height = container.clientHeight || 500;
	const svg = document.createElementNS(SVG_NS, "svg");
	svg.setAttribute("class", "graph");
	svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
	container.replaceChildren(svg);
	if (!model.nodes.length) {
		container.replaceChildren(element("div", "No notes to show", "graph-empty"));
		return { stop() {} };
	}

	// an arrowhead for links (transclusions are dashed instead)
	const defs = document.createElementNS(SVG_NS, "defs");
	// arrowheads for links, one per colour: grey, and (relative to the focused note) outgoing green and incoming red;
	// which one an edge uses is chosen in the stylesheet by its class. Transclusions are drawn heavier instead
	const arrow = (id, colour) => `<marker id="${id}" viewBox="0 0 10 10" refX="10" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="${colour}"/></marker>`;
	defs.innerHTML = arrow("graph-arrow", "#999") + arrow("graph-arrow-out", "#7f9a86") + arrow("graph-arrow-in", "#a8888a");
	svg.appendChild(defs);
	const world = document.createElementNS(SVG_NS, "g"); // panned and zoomed as a whole
	svg.appendChild(world);
	const edgeLayer = document.createElementNS(SVG_NS, "g"), nodeLayer = document.createElementNS(SVG_NS, "g");
	world.append(edgeLayer, nodeLayer);

	// with a centre, things further from it are drawn fainter: neighbours full, two steps lighter, the rest (or unconnected) faint
	function farness(id) {
		if (!options.distance) return "";
		const d = options.distance.get(id);
		return d === undefined || d >= 3 ? " far" : d === 2 ? " near2" : "";
	}

	/// NODES: SIZED BY HOW CONNECTED THEY ARE, COLOURED BY TOP-LEVEL FOLDER
	const degree = new Map(model.nodes.map(n => [n.id, 0]));
	model.edges.forEach(e => { degree.set(e.from, degree.get(e.from) + 1); degree.set(e.to, degree.get(e.to) + 1); });
	const hubDegree = Math.max(6, 0.25 * model.nodes.length);
	const colours = folderColours(); // connected to at least a quarter of the graph (and to 6+ notes)
	const byId = new Map();
	const spread = Math.min(width, height) / 3;
	// start each folder in its own sector of the circle (folders sorted by path, so neighbouring folders start side by side):
	// the layout cools quickly, so a tangled start stays tangled, and folder damping alone can't separate them
	const folders = [...new Set(model.nodes.map(n => n.dirs ? n.dirs.join("/") : ""))].sort();
	const sector = 2 * Math.PI / folders.length;
	for (const n of model.nodes) {
		const h = hashString(n.id), radius = spread * (0.3 + (h % 997) / 997 * 0.7);
		const angle = (folders.indexOf(n.dirs ? n.dirs.join("/") : "") + 0.15 + 0.7 * (h % 1009) / 1009) * sector;
		const node = { ...n, x: width / 2 + Math.cos(angle) * radius, y: height / 2 + Math.sin(angle) * radius, vx: 0, vy: 0, pinned: false, degree: Math.max(1, degree.get(n.id)) };
		node.r = 5 + Math.sqrt(degree.get(n.id)) * 2.5 + (n.id === options.centre ? 3 : 0);
		if (n.id === options.centre) { node.x = width / 2; node.y = height / 2; node.pinned = true; }
		const g = document.createElementNS(SVG_NS, "g");
		g.setAttribute("class", "graph-node " + n.status + (n.id === options.centre ? " centre" : "") + farness(n.id));
		const circle = document.createElementNS(SVG_NS, "circle");
		circle.setAttribute("r", node.r);
		if (!n.wanted) circle.setAttribute("fill", colours.get(n.folder) || "#888");
		// flagged as needing work: the usual solid node, ringed in red dashes (a wanted note is the dashed ring alone)
		if (n.todo !== undefined) {
			const ring = document.createElementNS(SVG_NS, "circle");
			ring.setAttribute("r", node.r + 3.5);
			ring.setAttribute("class", "graph-todo-ring");
			g.appendChild(ring);
		}
		const label = document.createElementNS(SVG_NS, "text");
		label.textContent = n.title;
		label.setAttribute("dy", node.r + 12);
		label.setAttribute("text-anchor", "middle");
		const tip = document.createElementNS(SVG_NS, "title");
		tip.textContent = n.wanted ? `"${n.title}" doesn't exist yet` : n.title + (n.todo !== undefined ? " (needs work" + (n.todo ? ": " + n.todo : "") + ")" : "");
		g.append(circle, label, tip);
		nodeLayer.appendChild(g);
		node.element = g;
		byId.set(n.id, node);
	}

	/// EDGES
	const edges = model.edges.map(e => {
		const line = document.createElementNS(SVG_NS, "line");
		// an edge is as faint as its fainter end
		const rank = { "": 0, " near2": 1, " far": 2 };
		const far = rank[farness(e.from)] >= rank[farness(e.to)] ? farness(e.from) : farness(e.to);
		// edges touching a hub (a note connected to a large share of the graph) are drawn faintly: they say little
		const hub = Math.max(degree.get(e.from), degree.get(e.to)) >= hubDegree ? " hub" : "";
		line.setAttribute("class", "graph-edge " + e.kind + far + hub);
		edgeLayer.appendChild(line);
		const from = byId.get(e.from), to = byId.get(e.to);
		return { from, to, kind: e.kind, element: line, folderDistance: folderDistance(from, to) };
	});

	// colour each link by direction relative to a focus note: out of it green, into it red, the rest grey
	function directions(focus) {
		for (const e of edges) {
			e.element.classList.toggle("out", !!focus && e.from.id === focus);
			e.element.classList.toggle("in", !!focus && e.to.id === focus);
		}
	}
	directions(options.centre); // a note's own graph: relative to the note (hovering refocuses it temporarily)
	const neighbours = new Map(model.nodes.map(n => [n.id, new Set([n.id])]));
	edges.forEach(e => { neighbours.get(e.from.id).add(e.to.id); neighbours.get(e.to.id).add(e.from.id); });

	/// THE LAYOUT: REPULSION BETWEEN ALL NODES, SPRINGS ALONG EDGES, A PULL TOWARDS THE CENTRE; COOLS AND STOPS
	const nodes = [...byId.values()];
	const spring = options.springLength || 90, repulsion = options.repulsion || 2600;
	let alpha = 1, frame = null, stopped = false;
	// hub repulsion: two nodes push apart in proportion to the product of their connectedness (as ForceAtlas2 does), relative to a
	// typical (median) node, never less than the plain repulsion and capped. Hubs then push each other apart and clear a halo for their
	// leaves, instead of sinking together into the middle (their springs are damped, so little else holds them apart)
	const sorted = nodes.map(n => n.degree).sort((a, b) => a - b);
	const typical = (1 + (sorted[Math.floor(sorted.length / 2)] || 1)) ** 2;
	const push = (a, b) => Math.min(25, Math.max(1, (1 + a.degree) * (1 + b.degree) / typical));
	function tick() {
		for (let i = 0; i < nodes.length; ++i) {
			const a = nodes[i];
			for (let j = i + 1; j < nodes.length; ++j) {
				const b = nodes[j];
				let dx = a.x - b.x, dy = a.y - b.y, d2 = dx * dx + dy * dy;
				if (d2 < 1) { dx = Math.random() - 0.5; dy = Math.random() - 0.5; d2 = 1; }
				const f = repulsion * push(a, b) / d2 * alpha, d = Math.sqrt(d2);
				a.vx += dx / d * f; a.vy += dy / d * f;
				b.vx -= dx / d * f; b.vy -= dy / d * f;
			}
		}
		for (const e of edges) {
			const dx = e.to.x - e.from.x, dy = e.to.y - e.from.y, d = Math.sqrt(dx * dx + dy * dy) || 1;
			// hub damping: a spring is weaker, and longer, the better connected its busier end is, so a note that everything
			// links to doesn't drag the whole graph into a star around itself
			const busiest = Math.max(e.from.degree, e.to.degree);
			// folder damping: the further apart two notes' folders are, the weaker and longer the spring, so folders settle
			// into their own clusters (edges are still drawn as usual)
			// with folder-pull p, a spring between folders d steps apart has strength 1 / (1 + k d), where k = (1 - p) / p: neighbouring
			// folders (d = 1) pull with exactly p, further ones less; p = 1 switches folders off, p = 0 cuts the springs between them
			const apart = e.folderDistance;
			const across = apart === 0 ? 1 : folderPull <= 0 ? 0 : 1 / (1 + (1 - folderPull) / folderPull * apart);
			const rest = spring * (1 + 0.25 * Math.log(1 + busiest)) * (1 + 0.3 * (1 - folderPull) * apart);
			const f = (d - rest) * 0.04 * alpha * (e.kind === "transclude" ? 1.5 : 1) / Math.sqrt(busiest) * across;
			e.from.vx += dx / d * f; e.from.vy += dy / d * f;
			e.to.vx -= dx / d * f; e.to.vy -= dy / d * f;
		}
		for (const n of nodes) {
			n.vx += (width / 2 - n.x) * 0.006 * alpha;
			n.vy += (height / 2 - n.y) * 0.006 * alpha;
			if (n.pinned) { n.vx = n.vy = 0; continue; }
			n.vx *= 0.82; n.vy *= 0.82;
			n.x += n.vx; n.y += n.vy;
		}
		alpha *= 0.985;
	}
	function render() {
		for (const n of nodes) n.element.setAttribute("transform", `translate(${n.x},${n.y})`);
		for (const e of edges) {
			// stop the line at the target's edge, so arrowheads aren't hidden under the circle
			const dx = e.to.x - e.from.x, dy = e.to.y - e.from.y, d = Math.sqrt(dx * dx + dy * dy) || 1;
			e.element.setAttribute("x1", e.from.x + dx / d * e.from.r);
			e.element.setAttribute("y1", e.from.y + dy / d * e.from.r);
			e.element.setAttribute("x2", e.to.x - dx / d * (e.to.r + 2));
			e.element.setAttribute("y2", e.to.y - dy / d * (e.to.r + 2));
		}
	}
	function animate() {
		if (stopped) return;
		tick();
		render();
		if (alpha > 0.004) frame = requestAnimationFrame(animate);
		else { frame = null; fitView(); }
	}
	function reheat() {
		alpha = Math.max(alpha, 0.3);
		if (!frame && !stopped) frame = requestAnimationFrame(animate);
	}
	// settle most of the way before the first paint, so the graph appears calm rather than exploding into place
	const deadline = performance.now() + 60;
	while (alpha > 0.05 && performance.now() < deadline) tick();
	render();

	/// PAN AND ZOOM THE WHOLE PICTURE
	let view = { x: 0, y: 0, k: 1 }, userMoved = false;
	const applyView = () => world.setAttribute("transform", `translate(${view.x},${view.y}) scale(${view.k})`);
	// zoom to fit the whole layout (labels included) in the box, never enlarging beyond 1.6x; done until the reader takes over
	function fitView() {
		if (userMoved) return;
		let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
		for (const n of nodes) {
			minX = Math.min(minX, n.x - n.r - 40); maxX = Math.max(maxX, n.x + n.r + 40);
			minY = Math.min(minY, n.y - n.r - 10); maxY = Math.max(maxY, n.y + n.r + 22);
		}
		const k = Math.min(1.6, width / (maxX - minX), height / (maxY - minY));
		view = { k, x: (width - (minX + maxX) * k) / 2, y: (height - (minY + maxY) * k) / 2 };
		applyView();
	}
	const toWorld = (event) => {
		const box = svg.getBoundingClientRect();
		const sx = (event.clientX - box.left) * width / box.width, sy = (event.clientY - box.top) * height / box.height;
		return { x: (sx - view.x) / view.k, y: (sy - view.y) / view.k, sx, sy };
	};
	svg.addEventListener("wheel", event => {
		event.preventDefault();
		const p = toWorld(event), factor = event.deltaY < 0 ? 1.15 : 1 / 1.15;
		userMoved = true;
		view.k = Math.min(4, Math.max(0.25, view.k * factor));
		view.x = p.sx - p.x * view.k;
		view.y = p.sy - p.y * view.k;
		applyView();
	}, { passive: false });

	/// HOVER HIGHLIGHTS A NODE AND ITS NEIGHBOURS; DRAG MOVES (AND PINS) A NODE; CLICK OPENS IT
	let dragging = null, panning = null, moved = false;
	for (const n of nodes) {
		n.element.addEventListener("pointerenter", () => {
			svg.classList.add("focused");
			const near = neighbours.get(n.id);
			nodes.forEach(m => m.element.classList.toggle("near", near.has(m.id)));
			edges.forEach(e => e.element.classList.toggle("near", e.from === n || e.to === n));
			directions(n.id);
		});
		n.element.addEventListener("pointerleave", () => {
			svg.classList.remove("focused");
			directions(options.centre);
		});
		n.element.addEventListener("pointerdown", event => {
			event.stopPropagation();
			dragging = n; moved = false;
			svg.setPointerCapture(event.pointerId);
		});
	}
	svg.addEventListener("pointerdown", event => {
		panning = { x: event.clientX, y: event.clientY, vx: view.x, vy: view.y };
		svg.setPointerCapture(event.pointerId);
	});
	svg.addEventListener("pointermove", event => {
		if (dragging) {
			const p = toWorld(event);
			dragging.x = p.x; dragging.y = p.y; dragging.pinned = true; moved = true;
			reheat();
		} else if (panning) {
			userMoved = true;
			const box = svg.getBoundingClientRect();
			view.x = panning.vx + (event.clientX - panning.x) * width / box.width;
			view.y = panning.vy + (event.clientY - panning.y) * height / box.height;
			applyView();
		}
	});
	svg.addEventListener("pointerup", () => {
		if (dragging && !moved) {
			const n = dragging;
			location.hash = n.wanted ? "#/a/" + encodeURIComponent(n.slug) : noteRoute(n.id);
		}
		dragging = null; panning = null;
	});

	fitView();
	frame = requestAnimationFrame(animate);

	const handle = { stop() { stopped = true; if (frame) cancelAnimationFrame(frame); } };
	activeGraphs.push(handle);
	return handle;
}

function graphLegend() {
	const legend = element("div", "", "graph-legend");
	legend.innerHTML = '<span><svg width="30" height="10"><line x1="0" y1="5" x2="30" y2="5" class="graph-edge link"/></svg> link</span>' +
		'<span><svg width="30" height="10"><line x1="0" y1="5" x2="30" y2="5" class="graph-edge link out"/></svg> out of / <svg width="30" height="10"><line x1="0" y1="5" x2="30" y2="5" class="graph-edge link in"/></svg> into the note under the pointer</span>' +
		'<span><svg width="30" height="10"><line x1="0" y1="5" x2="30" y2="5" class="graph-edge transclude"/></svg> transclusion</span>' +
		'<span><svg width="14" height="14"><circle cx="7" cy="7" r="5" class="graph-wanted-key"/></svg> wanted (doesn’t exist yet)</span>' +
		'<span><svg width="18" height="18"><circle cx="9" cy="9" r="4" fill="#999"/><circle cx="9" cy="9" r="7.5" class="graph-todo-ring"/></svg> needs work</span>' +
		'<span class="graph-hint">Scroll to zoom, drag the background to pan, drag a note to pin it, click to open</span>';
	return legend;
}

// The whole project
function showGraph() {
	currentId = null;
	highlight(null);
	stopGraphs();
	const page = element("div", "", "graph-page");
	page.appendChild(element("h1", projectName + ": graph"));
	page.appendChild(graphLegend());
	const box = element("div", "", "graph-box large");
	page.appendChild(box);
	document.getElementById("note").replaceChildren(page);
	document.title = projectName + " (graph)";
	drawGraph(box, graphModel());
}

// A note's place in the graph, under the note: the note pinned at the centre, everything within the chosen depth around it
// (1, 2, 3 links, or the whole project), fading with distance. The depth is remembered across notes
const GRAPH_DEPTHS = [["1", 1], ["2", 2], ["3", 3], ["All", null]];
let graphDepth = null;
try { const saved = localStorage.getItem("cortex-graph-depth"); if (saved) graphDepth = saved === "all" ? null : parseInt(saved); } catch (e) {}
function localGraph(id) {
	const full = graphModel(id);
	if (!full.edges.some(e => e.from === id || e.to === id)) return null; // a note with no connections has nothing to show
	// the heading above the graph, the depth choices below it
	const wrapper = element("div", "", "local-graph");
	wrapper.appendChild(element("h3", "Connections"));
	const box = element("div", "", "graph-box");
	wrapper.appendChild(box);
	const choices = element("div", "", "graph-depth");
	choices.appendChild(document.createTextNode("Depth: "));
	wrapper.appendChild(choices);

	// what links here, exactly: the graph shows neighbourhoods, this answers "what refers to this note?"
	const backlinks = cortexNotes[id] ? cortexNotes[id].backlinks : [];
	if (backlinks.length) {
		const line = element("div", "", "linked-from");
		line.appendChild(element("span", "Linked from: ", "linked-from-label"));
		backlinks.forEach((from, i) => {
			if (i) line.appendChild(document.createTextNode(", "));
			line.appendChild(noteLink(from));
		});
		wrapper.appendChild(line);
	}

	let handle = null;
	function draw() {
		if (handle) handle.stop();
		choices.querySelectorAll("button").forEach(b => b.classList.toggle("selected", b.depth === graphDepth));
		const model = neighbourhood(full, id, graphDepth);
		handle = drawGraph(box, model, { centre: id, distance: model.distance, springLength: 100, repulsion: 3000 });
	}
	for (const [label, depth] of GRAPH_DEPTHS) {
		const button = element("button", label);
		button.depth = depth;
		button.onclick = () => {
			graphDepth = depth;
			try { localStorage.setItem("cortex-graph-depth", depth === null ? "all" : String(depth)); } catch (e) {}
			draw();
		};
		choices.appendChild(button);
	}
	wrapper.draw = draw; // called by whoever puts it in the page: it needs its size, which it only has once it's there
	return wrapper;
}
