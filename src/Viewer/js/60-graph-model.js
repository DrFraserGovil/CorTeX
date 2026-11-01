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
// Loose connections ([[~x]], or into a `graph: loose` note) are left out: they still work as links, and still count as backlinks
// Notes marked `graph: hidden` (and their edges) are left out, except for `centre`: a hidden note still has its own graph
function graphModel(centre = null) {
	const shown = n => !n.graphHidden || n.id === centre;
	const nodes = new Map();
	const edges = new Map();
	for (const n of cortexOrder) {
		if (n.status === "redirect" || !shown(n)) continue;
		const dirs = n.path.split("/").slice(0, -1);
		nodes.set(n.id, { id: n.id, title: n.title, folderPath: dirs.join("/"), dirs, status: n.status, wanted: false, todo: n.todo });
	}
	for (const n of cortexOrder) {
		if (n.status === "redirect" || !shown(n)) continue;
		const loose = new Set(n.loose || []);
		for (const [slug, target] of Object.entries(n.links)) {
			if (loose.has(slug) || (target === null && slug.startsWith("#"))) continue; // a missing label isn't a wanted note
			let to;
			if (target === null) {
				to = "wanted:" + slug;
				if (!nodes.has(to)) nodes.set(to, { id: to, title: slug, status: "wanted", wanted: true, slug });
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

// A colouring of the project's notes by folder, as a function from a graph node to its colour (DESIGN.md §9.1), in one of three schemes:
// - "top": each top-level folder has a hue, shared by everything inside it
// - "shades": each top-level folder has a hue; the folder's own notes have its middle shade, and each subfolder (at any depth) one of
//   the other shades of it
// - "every": each folder (by full path) has a hue of its own
// A name starts at the hue (or shade) it hashes to and, if another folder already has that one, steps on (a hue 7 at a time, so a clash
// lands on a clearly different colour) to the first free one: colours only repeat once there are more folders than colours. Folders
// are placed in sorted order, across the whole project, so the result is the same on every build, and a note has the same colour in
// every graph
const GRAPH_SHADES = [45, 30, 62, 37, 54, 24, 70]; // lightness, %: the first is a top-level folder's own; the rest alternate dark and light
function folderColours(scheme) {
	const pathOf = path => path.includes("/") ? path.slice(0, path.lastIndexOf("/")) : "";
	const topOf = folder => folder.includes("/") ? folder.slice(0, folder.indexOf("/")) : folder;
	const allocate = (names, count, step, reserved = []) => {
		const taken = new Set(reserved), chosen = new Map();
		for (const name of names) {
			let at = hashString(name || "(top)") % count;
			for (let tries = 0; taken.has(at) && tries < count; ++tries) at = (at + step) % count;
			taken.add(at);
			chosen.set(name, at);
		}
		return chosen;
	};
	const hsl = (hue, lightness) => `hsl(${Math.round(hue * 360 / GRAPH_HUES)}, 55%, ${lightness}%)`;
	const folders = [...new Set(cortexOrder.map(n => pathOf(n.path)))].sort();

	if (scheme === "every") {
		const hues = allocate(folders, GRAPH_HUES, 7);
		return node => hsl(hues.get(node.folderPath), 45);
	}
	const hues = allocate([...new Set(folders.map(topOf))].sort(), GRAPH_HUES, 7);
	if (scheme !== "shades") return node => hsl(hues.get(topOf(node.folderPath)), 45);

	// shades: allocated among each top-level folder's subfolders separately, leaving the first shade for the folder's own notes
	const shades = new Map();
	for (const top of hues.keys()) {
		const subfolders = folders.filter(f => f !== top && topOf(f) === top);
		for (const [folder, shade] of allocate(subfolders, GRAPH_SHADES.length, 1, top ? [0] : [])) shades.set(folder, shade);
	}
	return node => hsl(hues.get(topOf(node.folderPath)), GRAPH_SHADES[shades.get(node.folderPath) || 0]);
}
