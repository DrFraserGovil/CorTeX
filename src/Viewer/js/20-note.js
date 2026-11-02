// A note's page (DESIGN.md §8.2)

function highlight(id) {
	// the sidebar shows the open note's own entries and marks the current note, so it is redrawn on every navigation
	renderSidebar();
}

// The label to scroll to once the open note has loaded (hex, as label links name it), and the note on show (once loaded)
let pendingLabel = null;
let shownNote = null;

// Scrolls to a label on the open note's page, putting it a third of the way down the window. The build marks each label's position with an empty
// element (class cortex-label) holding its coordinates within the page's SVG
function scrollToLabel(hex) {
	const mark = document.querySelector('#note .cortex-label[data-label="' + hex + '"]');
	if (!mark) return;
	const svg = mark.ownerSVGElement, x = parseFloat(mark.dataset.x), y = parseFloat(mark.dataset.y);
	if (!svg || isNaN(x) || isNaN(y)) return;
	const point = svg.createSVGPoint();
	point.x = x; point.y = y;
	const onScreen = point.matrixTransform(mark.getScreenCTM());
	window.scrollBy({ top: onScreen.y - window.innerHeight / 3 });
}

// Called by notes/<id>.js once it has loaded
function CortexNote(note) {
	if (note.id !== currentId) return; // a slower load of a note the user has already navigated away from
	shownNote = note.id;
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
	document.title = note.title;
	if (pendingScroll !== null) restoreScroll(); // after a live refresh, stay where the reader was
	else if (pendingLabel) scrollToLabel(pendingLabel);
	pendingLabel = null;
}

// Shows a note, at a label if one is given (hex, as label links name it)
function showNote(id, label = null) {
	// a label on the page already shown: just scroll (reloading would lose the reader's place for nothing)
	if (label && id === currentId && id === shownNote && pendingScroll === null) {
		setNoteTools(cortexNotes[id]);
		document.getElementById("zoom").classList.add("shown");
		scrollToLabel(label);
		return;
	}
	currentId = id;
	shownNote = null;
	pendingLabel = label;
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
