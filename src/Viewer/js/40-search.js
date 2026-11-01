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
