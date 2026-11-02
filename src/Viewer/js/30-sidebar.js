// The sidebar is curated, not a list of every note (that's the overview): the project's groups from registry.config, then the open
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
