// The pages (DESIGN.md §8.1), chosen by the address after the #; following links; live refresh

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
	const label = hash.match(/^#\/l\/(.+)\/([0-9A-F]+)$/);
	if (label) {
		showNote(decodeURIComponent(label[1]), label[2]);
		return;
	}
	const route = hash.match(/^#\/([na])\/(.+)$/);
	if (!route) return;
	const value = decodeURIComponent(route[2]);
	if (route[1] === "n") showNote(value);
	else followLink(value);
}
window.addEventListener("hashchange", show);

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
	loadScript("index.js", () => { show(); if (!/^#\/[nl]\//.test(location.hash)) restoreScroll(); });
}
function restoreScroll() {
	if (pendingScroll === null) return;
	window.scrollTo(0, pendingScroll);
	pendingScroll = null;
}
let live = true; // false in an exported copy, which has no version.js (and nothing will ever rebuild it)
setInterval(() => { if (live && document.visibilityState === "visible") loadScript("version.js"); }, 1000);
