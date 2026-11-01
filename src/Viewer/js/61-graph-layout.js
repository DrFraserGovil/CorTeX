// The graph's layout: where each node goes, and nothing about drawing it

// The rings layout's point for each folder (by path; "" for the project's top level), as in a sunburst chart: the top-level notes
// at the centre, each top-level folder on the first ring and each subfolder on the next ring out, inside its parent's wedge (so near
// it in angle). A wedge is sized by the room its folder and subfolders need, and each ring is far enough out for every folder on it
// to fit beside its neighbours, and clear of the rings either side. Also returns each folder's cluster radius, to start its notes in
function folderPoints(nodes, spring, cx, cy) {
	const own = new Map(), children = new Map([["", []]]);
	for (const n of nodes) {
		const dirs = n.dirs || [];
		own.set(dirs.join("/"), (own.get(dirs.join("/")) || 0) + 1);
		for (let i = 1; i <= dirs.length; ++i) {
			const path = dirs.slice(0, i).join("/"), parent = dirs.slice(0, i - 1).join("/");
			if (!children.has(path)) { children.set(path, []); children.get(parent).push(path); }
		}
	}
	// a folder of n notes settles into a cluster about spring * sqrt(n) across
	const radius = path => 0.5 * spring * Math.sqrt(own.get(path) || 0);
	const across = path => 2 * radius(path) + spring;
	const room = new Map(); // the arc a folder's wedge needs: its own cluster, or its subfolders' (further out), whichever is wider
	(function measure(path) {
		let sum = 0;
		for (const child of children.get(path)) sum += measure(child);
		room.set(path, Math.max(across(path), sum));
		return room.get(path);
	})("");

	/// SPLIT THE CIRCLE INTO WEDGES, BY FOLDER
	const place = new Map(), depths = [];
	(function split(path, from, to, depth) {
		place.set(path, { angle: (from + to) / 2, wedge: to - from, depth });
		(depths[depth] = depths[depth] || []).push(path);
		const kids = children.get(path).sort(), total = kids.reduce((sum, k) => sum + room.get(k), 0);
		let at = from;
		for (const k of kids) {
			const share = (to - from) * room.get(k) / total;
			split(k, at, at + share, depth + 1);
			at += share;
		}
	})("", -Math.PI / 2, 1.5 * Math.PI, 0);

	/// PLACE THE RINGS
	const rings = [0];
	const widest = d => Math.max(...depths[d].map(radius));
	for (let d = 1; d < depths.length; ++d) {
		let r = rings[d - 1] + widest(d - 1) + widest(d) + spring;
		for (const path of depths[d]) r = Math.max(r, across(path) / Math.min(place.get(path).wedge, Math.PI));
		rings.push(r);
	}
	const points = new Map();
	for (const [path, { angle, depth }] of place) {
		points.set(path, { x: cx + rings[depth] * Math.cos(angle), y: cy + rings[depth] * Math.sin(angle), radius: Math.max(spring / 2, radius(path)) });
	}
	return points;
}

/* Lays out a model ({nodes, edges}) in a width × height box. Returns {nodes, edges, alpha, tick()}: each node gains x, y, degree (its
   number of connections, at least 1) and pinned; each edge's from and to become the node objects. tick() moves everything one step and
   cools alpha; the layout has settled once alpha is small. Reheating (raising alpha) and pinning (setting pinned) are up to the caller.
   options:
   - settings: {layout, folderPull, folderRepulsion}, as graphSettings() gives them; rings places each folder at its own point
     (folderPoints), instead of pulling every note towards the middle
   - pin: a note held fixed in the middle
   - springLength, repulsion: the scale of the layout
   The same model and options always give the same layout */
function layoutGraph(model, width, height, options) {
	const settings = options.settings, folderPull = settings.folderPull, folderRepulsion = settings.folderRepulsion;
	const rings = settings.layout === "rings";
	const spring = options.springLength || 90, repulsion = options.repulsion || 2600;
	const degree = new Map(model.nodes.map(n => [n.id, 0]));
	model.edges.forEach(e => { degree.set(e.from, degree.get(e.from) + 1); degree.set(e.to, degree.get(e.to) + 1); });

	/// STARTING POSITIONS
	const spread = Math.min(width, height) / 3;
	// without rings, start each folder in its own sector of the circle (folders sorted by path, so neighbouring folders start side by
	// side): the layout cools quickly, so a tangled start stays tangled, and folder damping alone can't separate them
	const folders = [...new Set(model.nodes.map(n => n.dirs ? n.dirs.join("/") : ""))].sort();
	const sector = 2 * Math.PI / folders.length;
	// with rings, a wanted note (which has no folder) belongs to the folder most of its linkers are in, so it is placed with them,
	// and its springs to other folders are weakened like any other; otherwise one linked from several folders would tie them together
	const homes = new Map();
	if (rings) {
		const dirsOf = new Map(model.nodes.map(n => [n.id, n.dirs]));
		const votes = new Map();
		for (const e of model.edges) {
			if (!e.to.startsWith("wanted:") || !dirsOf.get(e.from)) continue;
			const tally = votes.get(e.to) || votes.set(e.to, new Map()).get(e.to), path = dirsOf.get(e.from).join("/");
			tally.set(path, (tally.get(path) || 0) + 1);
		}
		for (const [id, tally] of votes) homes.set(id, [...tally].sort((a, b) => b[1] - a[1])[0][0]);
	}
	const dirsOf = n => homes.has(n.id) ? (homes.get(n.id) ? homes.get(n.id).split("/") : []) : n.dirs;
	const points = rings ? folderPoints(model.nodes.map(n => ({ dirs: dirsOf(n) })), spring, width / 2, height / 2) : null;
	const nodes = model.nodes.map(n => {
		const h = hashString(n.id), dirs = dirsOf(n);
		let x, y, point = null;
		if (points) {
			// start in a disc around the folder's own point
			point = points.get(dirs ? dirs.join("/") : "");
			const angle = 2 * Math.PI * (h % 1009) / 1009, radius = point.radius * Math.sqrt((h % 997) / 997);
			x = point.x + Math.cos(angle) * radius; y = point.y + Math.sin(angle) * radius;
		} else {
			const radius = spread * (0.3 + (h % 997) / 997 * 0.7);
			const angle = (folders.indexOf(n.dirs ? n.dirs.join("/") : "") + 0.15 + 0.7 * (h % 1009) / 1009) * sector;
			x = width / 2 + Math.cos(angle) * radius; y = height / 2 + Math.sin(angle) * radius;
		}
		const node = { ...n, dirs, x, y, vx: 0, vy: 0, pinned: false, connections: degree.get(n.id), degree: Math.max(1, degree.get(n.id)) };
		// where the layout pulls the note: the middle, or (with rings) its folder's point, moved towards the middle by folder-pull
		node.homeX = point ? point.x + (width / 2 - point.x) * folderPull : width / 2;
		node.homeY = point ? point.y + (height / 2 - point.y) * folderPull : height / 2;
		if (n.id === options.pin) { node.x = width / 2; node.y = height / 2; node.pinned = true; }
		return node;
	});
	const byId = new Map(nodes.map(n => [n.id, n]));
	const edges = model.edges.map(e => {
		const from = byId.get(e.from), to = byId.get(e.to);
		return { from, to, kind: e.kind, folderDistance: folderDistance(from, to) };
	});

	/// THE FORCES: REPULSION BETWEEN ALL NODES, SPRINGS ALONG EDGES, A PULL TOWARDS THE CENTRE (OR, WITH RINGS, THE FOLDER)
	// degree repulsion: two nodes push apart in proportion to the product of their connectedness (as ForceAtlas2 does), relative to a
	// typical (median) node, never less than the plain repulsion and capped. Well-connected notes then push each other apart and clear
	// space for their neighbours, instead of sinking together into the middle (their springs are damped, so little else holds them apart)
	const sorted = nodes.map(n => n.degree).sort((a, b) => a - b);
	const typical = (1 + (sorted[Math.floor(sorted.length / 2)] || 1)) ** 2;
	// folder repulsion: notes in different folders push harder still, by 1 + r d for folders d steps apart (folder-repulsion r), which
	// opens up space between the folders' clusters. The distances are worked out once per pair of folders, not per pair of notes
	const folderIndex = new Map(), folderDirs = [];
	for (const n of nodes) {
		const path = n.dirs ? n.dirs.join("/") : null; // a wanted note with no folder is 0 steps from everything
		if (!folderIndex.has(path)) { folderIndex.set(path, folderDirs.length); folderDirs.push({ dirs: n.dirs }); }
		n.folderIndex = folderIndex.get(path);
	}
	const apartness = folderDirs.map(a => folderDirs.map(b => 1 + folderRepulsion * folderDistance(a, b)));
	const push = (a, b) => Math.min(25, Math.max(1, (1 + a.degree) * (1 + b.degree) / typical)) * apartness[a.folderIndex][b.folderIndex];

	const layout = { nodes, edges, alpha: 1 };
	layout.tick = () => {
		const alpha = layout.alpha;
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
			// degree damping: a spring is weaker, and longer, the better connected its busier end is, so a note that everything
			// links to doesn't drag the whole graph into a star around itself
			const busiest = Math.max(e.from.degree, e.to.degree);
			// folder damping: with folder-pull p, a spring between folders d steps apart has strength 1 / (1 + k d), where
			// k = (1 - p) / p: neighbouring folders (d = 1) pull with exactly p, further ones less; p = 1 switches folders off, p = 0 cuts
			// the springs between them. It is also longer, so folders settle into their own clusters
			const apart = e.folderDistance;
			const across = apart === 0 ? 1 : folderPull <= 0 ? 0 : 1 / (1 + (1 - folderPull) / folderPull * apart);
			const rest = spring * (1 + 0.25 * Math.log(1 + busiest)) * (1 + 0.3 * (1 - folderPull) * apart);
			const f = (d - rest) * 0.04 * alpha * (e.kind === "transclude" ? 1.5 : 1) / Math.sqrt(busiest) * across;
			e.from.vx += dx / d * f; e.from.vy += dy / d * f;
			e.to.vx -= dx / d * f; e.to.vy -= dy / d * f;
		}
		for (const n of nodes) {
			n.vx += (n.homeX - n.x) * 0.006 * alpha;
			n.vy += (n.homeY - n.y) * 0.006 * alpha;
			if (n.pinned) { n.vx = n.vy = 0; continue; }
			n.vx *= 0.82; n.vy *= 0.82;
			n.x += n.vx; n.y += n.vy;
		}
		layout.alpha *= 0.985;
	};
	return layout;
}
