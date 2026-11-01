// The viewer (DESIGN.md §8), in parts joined in name order into one script at build time (ES modules don't load over file://).
// This part: the project's data and settings, as index.js hands them over, and the zoom

const cortexNotes = {};
const cortexOrder = [];
const cortexByKey = {}; // links and transclusions name their origin note by key
let currentId = null;

// Zoom: the project's default (from index.js), optionally overridden by this reader (remembered in browser storage, where available).
// Notes are vector images, so any zoom is sharp, and it never changes where lines break.
let projectZoom = 1;
// The project's graph settings (DESIGN.md §9.4), which a reader can override in their browser (see graphSettings):
// layout "centre" or "rings"; folderPull 0 to 1; folderRepulsion 0 or more; colours "top", "shades" or "every"
const GRAPH_COLOUR_SCHEMES = ["top", "shades", "every"];
let projectGraph = { layout: "centre", folderPull: 0.5, folderRepulsion: 0, colours: "top" };
let readerZoom = null;
try { const saved = parseFloat(localStorage.getItem("cortex-zoom")); if (saved > 0) readerZoom = saved; } catch (e) {}
let projectName = "CorTeX";
let homeId = "";
let projectRoot = ""; // the absolute project root, which cortex:// links name
let editors = [];     // the names of the project's editors (the commands stay in the settings)
// The layout of index.js this viewer reads (Output/Viewer.h); the two are always written together, so a mismatch means a damaged copy
const VIEWER_FORMAT = 1;
function CortexConfig(config) {
	if (config.format !== VIEWER_FORMAT && !document.getElementById("format-warning")) {
		const warning = banner("This viewer doesn't match its project data (format " + config.format + ", expected " + VIEWER_FORMAT + "). Rebuild the project.");
		warning.id = "format-warning";
		document.querySelector("nav").prepend(warning);
	}
	if (config.zoom > 0) projectZoom = config.zoom;
	projectGraph = {
		layout: config.graphLayout === "rings" ? "rings" : "centre",
		folderPull: config.folderPull >= 0 && config.folderPull <= 1 ? config.folderPull : 0.5,
		folderRepulsion: config.folderRepulsion >= 0 ? config.folderRepulsion : 0,
		colours: GRAPH_COLOUR_SCHEMES.includes(config.graphColours) ? config.graphColours : "top",
	};
	if (config.name) projectName = config.name;
	document.getElementById("project-name").textContent = config.name || "";
	homeId = config.home || "";
	// set as a style property (never inserted as text), so the setting can't inject anything; an invalid colour is simply ignored
	if (config.background) document.body.style.backgroundColor = config.background;
	// with no home note, Home already is the overview, so a separate link would be redundant
	document.getElementById("overview-link").style.display = homeId ? "" : "none";
	projectRoot = config.root || "";
	live = config.live !== false;
	editors = Array.isArray(config.editors) ? config.editors : [];
	const logo = document.getElementById("logo");
	if (config.logo) logo.src = config.logo; else logo.removeAttribute("src");
	logo.classList.toggle("shown", !!config.logo);
	forgetMatchingGraphChoices(); // after the name and root, which say where this project's choices are stored
	applyZoom();
}

function zoom() { return readerZoom || projectZoom; }
function setReaderZoom(value) {
	readerZoom = value;
	try { if (value) localStorage.setItem("cortex-zoom", value); else localStorage.removeItem("cortex-zoom"); } catch (e) {}
	applyZoom();
}
// draw each note at its natural size (dvisvgm gives it in pt; 1pt = 4/3 CSS px) times the zoom; max-width still shrinks it to fit a narrow window
function applyZoom() {
	document.getElementById("zoom-level").textContent = Math.round(zoom() * 100) + "%";
	let widest = 0;
	for (const svg of document.querySelectorAll("#note > svg")) {
		const natural = parseFloat(svg.getAttribute("width"));
		if (natural > 0) svg.style.width = (natural * 4 / 3 * zoom()) + "px";
		widest = Math.max(widest, natural * 4 / 3 * zoom());
	}
	// the Connections panel under a note is as wide as the note's page (never wider than the window: max-width in the stylesheet)
	for (const panel of document.querySelectorAll("#note > .local-graph")) panel.style.width = widest > 0 ? widest + "px" : "";
}
document.getElementById("zoom-in").onclick = () => setReaderZoom(Math.round(zoom() * 110) / 100);
document.getElementById("zoom-out").onclick = () => setReaderZoom(Math.round(zoom() / 1.1 * 100) / 100);
document.getElementById("zoom-reset").onclick = () => setReaderZoom(null);

function CortexIndex(notes) {
	// may be called again when the viewer refreshes after a rebuild, so it starts from scratch
	for (const key of Object.keys(cortexNotes)) delete cortexNotes[key];
	for (const key of Object.keys(cortexByKey)) delete cortexByKey[key];
	cortexOrder.length = 0;
	for (const n of notes) {
		cortexNotes[n.id] = n;
		cortexByKey[n.key] = n;
		cortexOrder.push(n);
	}
}
