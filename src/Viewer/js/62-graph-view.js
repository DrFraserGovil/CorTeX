// Drawing a graph, and the graph pages (DESIGN.md §9): the project graph (#/graph), and a note's own graph (under the note)

/* Draws a model into a container, and runs its layout (layoutGraph). Returns an object with stop().
   options:
   - settings: the layout settings (graphSettings())
   - centre: the note a note's own graph is about: laid out freely, but the view stays centred on it (until the reader zooms or pans);
     drawn larger, with things fainter the further they are from it (options.distance, from neighbourhood) and links coloured by
     direction relative to it
   - pin: a note fixed in the middle, which changes nothing else about the drawing (the project graph's home note)
   - frame: the ids to fit in view (all of them if absent), until the reader zooms or pans
   - layoutSize: {width, height} to lay the graph out in, if not the container's own (the layout depends on it)
   - springLength, repulsion: the scale of the layout */
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

	// arrowheads for links, one per colour: grey, and (relative to the focused note) outgoing green and incoming red;
	// which one an edge uses is chosen in the stylesheet by its class. Transclusions are dashed instead
	const defs = document.createElementNS(SVG_NS, "defs");
	const arrow = (id, colour) => `<marker id="${id}" viewBox="0 0 10 10" refX="10" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="${colour}"/></marker>`;
	defs.innerHTML = arrow("graph-arrow", "#999") + arrow("graph-arrow-out", "#7f9a86") + arrow("graph-arrow-in", "#a8888a");
	svg.appendChild(defs);
	const world = document.createElementNS(SVG_NS, "g"); // panned and zoomed as a whole
	svg.appendChild(world);
	const edgeLayer = document.createElementNS(SVG_NS, "g"), nodeLayer = document.createElementNS(SVG_NS, "g");
	world.append(edgeLayer, nodeLayer);

	const layoutSize = options.layoutSize || { width, height };
	const layout = layoutGraph(model, layoutSize.width, layoutSize.height, {
		settings: options.settings, pin: options.pin, start: options.centre, springLength: options.springLength, repulsion: options.repulsion,
	});
	const { nodes, edges } = layout;

	// with a centre, things further from it are drawn fainter: neighbours full, two steps lighter, the rest (or unconnected) faint
	function farness(id) {
		if (!options.distance) return "";
		const d = options.distance.get(id);
		return d === undefined || d >= 3 ? " far" : d === 2 ? " near2" : "";
	}

	/// NODES: SIZED BY HOW CONNECTED THEY ARE, COLOURED BY TOP-LEVEL FOLDER
	const colourOf = folderColours(options.settings.colours);
	for (const n of nodes) {
		n.r = 5 + Math.sqrt(n.connections) * 2.5 + (n.id === options.centre ? 3 : 0);
		const g = document.createElementNS(SVG_NS, "g");
		g.setAttribute("class", "graph-node " + n.status + (n.id === options.centre ? " centre" : "") + farness(n.id));
		const circle = document.createElementNS(SVG_NS, "circle");
		circle.setAttribute("r", n.r);
		if (!n.wanted) circle.setAttribute("fill", colourOf(n) || "#888");
		// flagged as needing work: the usual solid node, ringed in red dashes (a wanted note is the dashed ring alone)
		if (n.todo !== undefined) {
			const ring = document.createElementNS(SVG_NS, "circle");
			ring.setAttribute("r", n.r + 3.5);
			ring.setAttribute("class", "graph-todo-ring");
			g.appendChild(ring);
		}
		const label = document.createElementNS(SVG_NS, "text");
		label.textContent = n.title;
		label.setAttribute("dy", n.r + 12);
		label.setAttribute("text-anchor", "middle");
		const tip = document.createElementNS(SVG_NS, "title");
		tip.textContent = n.wanted ? `"${n.title}" doesn't exist yet` : n.title + (n.todo !== undefined ? " (needs work" + (n.todo ? ": " + n.todo : "") + ")" : "");
		g.append(circle, label, tip);
		nodeLayer.appendChild(g);
		n.element = g;
	}

	/// EDGES: AS FAINT AS THEIR FAINTER END
	const rank = { "": 0, " near2": 1, " far": 2 };
	for (const e of edges) {
		const line = document.createElementNS(SVG_NS, "line");
		const far = rank[farness(e.from.id)] >= rank[farness(e.to.id)] ? farness(e.from.id) : farness(e.to.id);
		line.setAttribute("class", "graph-edge " + e.kind + far);
		edgeLayer.appendChild(line);
		e.element = line;
	}

	// colour each link by direction relative to a focus note: out of it green, into it red, the rest grey
	function directions(focus) {
		for (const e of edges) {
			e.element.classList.toggle("out", !!focus && e.from.id === focus);
			e.element.classList.toggle("in", !!focus && e.to.id === focus);
		}
	}
	directions(options.centre); // a note's own graph: relative to the note (hovering refocuses it temporarily)
	const neighbours = new Map(nodes.map(n => [n.id, new Set([n.id])]));
	edges.forEach(e => { neighbours.get(e.from.id).add(e.to.id); neighbours.get(e.to.id).add(e.from.id); });

	/// RUNNING THE LAYOUT: IT COOLS AND STOPS; DRAGGING A NODE WARMS IT AGAIN
	let frame = null, stopped = false;
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
		layout.tick();
		render();
		if (centreNode) fitView(false); // keep the note in the middle as the layout moves it, without changing the zoom every frame
		if (layout.alpha > 0.004) frame = requestAnimationFrame(animate);
		else { frame = null; fitView(); }
	}
	function reheat() {
		layout.alpha = Math.max(layout.alpha, 0.3);
		if (!frame && !stopped) frame = requestAnimationFrame(animate);
	}
	// settle most of the way before the first paint, so the graph appears calm rather than exploding into place
	const deadline = performance.now() + 60;
	while (layout.alpha > 0.05 && performance.now() < deadline) layout.tick();
	render();

	/// PAN AND ZOOM THE WHOLE PICTURE
	let view = { x: 0, y: 0, k: 1 }, userMoved = false;
	const applyView = () => world.setAttribute("transform", `translate(${view.x},${view.y}) scale(${view.k})`);
	// zoom to fit the framed nodes (labels included) in the box, never enlarging beyond 1.6x; done until the reader takes over. With a
	// centre, the box is centred on it, and the zoom fits whichever framed node reaches furthest from it; zoom false only re-centres
	const framed = options.frame ? nodes.filter(n => options.frame.has(n.id)) : nodes;
	const centreNode = options.centre ? nodes.find(n => n.id === options.centre) : null;
	function fitView(zoom = true) {
		if (userMoved) return;
		let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
		for (const n of framed.length ? framed : nodes) {
			minX = Math.min(minX, n.x - n.r - 40); maxX = Math.max(maxX, n.x + n.r + 40);
			minY = Math.min(minY, n.y - n.r - 10); maxY = Math.max(maxY, n.y + n.r + 22);
		}
		if (centreNode) {
			const c = centreNode;
			const halfWidth = Math.max(c.x - minX, maxX - c.x), halfHeight = Math.max(c.y - minY, maxY - c.y);
			const k = zoom ? Math.min(1.6, width / (2 * halfWidth), height / (2 * halfHeight)) : view.k;
			view = { k, x: width / 2 - c.x * k, y: height / 2 - c.y * k };
		} else {
			const k = Math.min(1.6, width / (maxX - minX), height / (maxY - minY));
			view = { k, x: (width - (minX + maxX) * k) / 2, y: (height - (minY + maxY) * k) / 2 };
		}
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
			if (dragging === centreNode) userMoved = true; // the view stops following it, or it would run from the pointer
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

// The layout settings in use (DESIGN.md §9.4): the project's, except where this reader has changed them (remembered in their browser,
// per project; the project itself never changes). Saved values are checked like the settings, so a damaged one is just ignored
function graphStorageKey() { return "cortex-graph:" + (projectRoot || projectName); }
function readerGraph() {
	let saved = null;
	try { saved = JSON.parse(localStorage.getItem(graphStorageKey())); } catch (e) {}
	if (!saved || typeof saved !== "object") return {};
	const valid = {};
	if (saved.layout === "centre" || saved.layout === "rings") valid.layout = saved.layout;
	if (saved.folderPull >= 0 && saved.folderPull <= 1) valid.folderPull = saved.folderPull;
	if (saved.folderRepulsion >= 0) valid.folderRepulsion = saved.folderRepulsion;
	if (GRAPH_COLOUR_SCHEMES.includes(saved.colours)) valid.colours = saved.colours;
	return valid;
}
function graphSettings() { return Object.assign({}, projectGraph, readerGraph()); }
function storeReaderGraph(choices) {
	try {
		if (Object.keys(choices).length) localStorage.setItem(graphStorageKey(), JSON.stringify(choices));
		else localStorage.removeItem(graphStorageKey());
	} catch (e) {}
}
// changes are added to the reader's earlier choices; null forgets them all
function setReaderGraph(changes) { storeReaderGraph(changes === null ? {} : Object.assign(readerGraph(), changes)); }
// Forgets any choice the project's settings now match, e.g. once the reader has saved them as the project's defaults (they're only
// forgotten once the project really has them, so a save that never happened loses nothing). Called whenever index.js loads
function forgetMatchingGraphChoices() {
	const choices = readerGraph();
	const kept = Object.fromEntries(Object.entries(choices).filter(([key, value]) => projectGraph[key] !== value));
	if (Object.keys(kept).length !== Object.keys(choices).length) storeReaderGraph(kept);
}

// The project graph's controls: layout, colours, folder pull and folder repulsion, a return to the project's defaults and (where the
// viewer can act on the project, DESIGN.md §10) a way to make the reader's choices the project's defaults; redraw is called after each change
function graphControls(redraw) {
	const bar = element("div", "", "graph-controls");
	const settings = graphSettings();
	const control = (text, input) => {
		const label = element("label", text);
		label.appendChild(input);
		bar.appendChild(label);
		return input;
	};

	const layout = control("Layout ", document.createElement("select"));
	layout.append(new Option("centre", "centre"), new Option("rings", "rings"));
	layout.value = settings.layout;
	layout.title = "centre draws every note towards the middle; rings gives each folder a place of its own";
	layout.onchange = () => { setReaderGraph({ layout: layout.value }); redraw(); };

	const colours = control("Colours ", document.createElement("select"));
	colours.append(new Option("by top folder", "top"), new Option("shaded subfolders", "shades"), new Option("by every folder", "every"));
	colours.value = settings.colours;
	colours.title = "How notes are coloured by folder: subfolders sharing their top-level folder's colour, each in a shade of it, or each with a colour of its own";
	colours.onchange = () => { setReaderGraph({ colours: colours.value }); redraw(); };

	// a slider, with its value beside it; the graph is redrawn when it's let go, not at every step
	const slider = (text, key, max, step, title) => {
		const input = document.createElement("input");
		Object.assign(input, { type: "range", min: 0, max: Math.max(max, settings[key]), step, value: settings[key], title });
		control(text, input);
		const shown = element("span", String(settings[key]), "graph-control-value");
		input.after(shown);
		input.oninput = () => { shown.textContent = input.value; };
		input.onchange = () => { setReaderGraph({ [key]: parseFloat(input.value) }); redraw(); };
	};
	slider("Folder pull ", "folderPull", 1, 0.05, "How strongly linked notes in different folders draw together (1 ignores folders)");
	slider("Folder repulsion ", "folderRepulsion", 5, 0.1, "How much harder notes in different folders push apart (0 treats all notes alike)");

	const reset = element("button", "Project defaults");
	reset.title = "Forget this browser's choices, and use the project's settings";
	reset.disabled = !Object.keys(readerGraph()).length;
	reset.onclick = () => { setReaderGraph(null); redraw(); };
	bar.appendChild(reset);

	if (projectRoot && Object.keys(readerGraph()).length) {
		const save = element("a", "Save as project defaults", "graph-save");
		save.href = cortexAction("graph", {
			layout: settings.layout, colours: settings.colours, "folder-pull": settings.folderPull, "folder-repulsion": settings.folderRepulsion,
		});
		save.title = "Writes these four settings into the project's settings (nothing is recompiled); the page refreshes once the project rebuilds";
		bar.appendChild(save);
	}
	return bar;
}

// The whole project: the home note (if there is one) is held in the middle, as the project's starting point
function showGraph() {
	currentId = null;
	highlight(null);
	stopGraphs();
	const page = element("div", "", "graph-page");
	page.appendChild(element("h1", projectName + ": Graph"));
	const controls = element("div");
	page.append(controls, graphLegend());
	const box = element("div", "", "graph-box large");
	page.appendChild(box);
	document.getElementById("note").replaceChildren(page);
	document.title = projectName + " (Graph)";
	function draw() {
		stopGraphs();
		controls.replaceChildren(graphControls(draw)); // the reset button's state, and sliders after a reset, follow the settings
		const settings = graphSettings();
		drawGraph(box, graphModel(), { settings, pin: homeId, layoutSize: projectGraphSize() });
	}
	draw();
}

// The size the project graph is laid out in: its box's (.graph-box.large: the page's width, 75% of the window's height). A note's graph
// at depth All is laid out at the same size, so its notes are where they are on the project graph
function projectGraphSize() {
	const note = document.getElementById("note");
	return { width: (note && note.clientWidth) || 800, height: Math.round(window.innerHeight * 0.75) || 500 };
}

// A note's own graph, under the note (DESIGN.md §9.3): the view centred on the note, everything within the chosen depth around it
// (1, 2, 3 links, or the whole project), fading with distance, opened framed on the note and its immediate neighbours. The depth is
// remembered across notes. At depths 1 to 3 only the neighbourhood is laid out, always with the centre layout (rings place folders,
// which says nothing about one note's neighbourhood). At All it is the project graph itself, laid out exactly as there, centred on the note
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
		const frame = new Set(model.nodes.filter(n => model.distance.get(n.id) <= 1).map(n => n.id));
		const drawing = { centre: id, distance: model.distance, frame };
		if (graphDepth === null) {
			// the project graph's own model, settings and size (a hidden note isn't on the project graph, so its own keeps it, and differs by it)
			const project = graphModel();
			const all = project.nodes.some(n => n.id === id) ? project : full;
			handle = drawGraph(box, all, { ...drawing, settings: graphSettings(), pin: homeId, layoutSize: projectGraphSize() });
		} else {
			handle = drawGraph(box, model, { ...drawing, settings: Object.assign(graphSettings(), { layout: "centre" }), springLength: 100, repulsion: 3000 });
		}
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
