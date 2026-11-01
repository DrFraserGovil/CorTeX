// The overview (DESIGN.md §8.3)

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
			if (target === null && !slug.startsWith("#")) (wanted[slug] = wanted[slug] || []).push(n.id);
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
