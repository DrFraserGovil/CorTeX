// Small builders for the page's elements, shared by every part

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

// Loads a script afresh (the ?t= stops the browser reusing an old copy after a rebuild), removing the tag once it has run
function loadScript(src, then) {
	const s = document.createElement("script");
	s.src = src + "?t=" + Date.now();
	s.onload = () => { s.remove(); if (then) then(); };
	s.onerror = () => s.remove();
	document.body.appendChild(s);
}
