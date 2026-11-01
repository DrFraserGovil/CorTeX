// The Editor tools (DESIGN.md §10): only in the author's own copy, where the platform supports them (index.js then gives the
// project's root; it doesn't otherwise)

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
