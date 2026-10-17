#include "Emitter.h"
#include "../Utility/Files.h"
#include <JSL.h>

// A deliberately minimal viewer: lists notes, and shows one when its link is clicked.
// The real shell (navigation, backlinks, link resolution) replaces this later.
static const char *StubViewer = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>CorTeX</title>
<style>
body { font-family: sans-serif; margin: 0; display: flex; }
nav { width: 16em; padding: 1em; border-right: 1px solid #ccc; min-height: 100vh; }
nav .stale, nav .failed { color: #b35900; }
.banner { background: #fff3cd; border: 1px solid #e0b252; padding: 0.5em 1em; margin-bottom: 1em; }
.banner pre { white-space: pre-wrap; margin: 0.5em 0 0 0; }
main { padding: 1em; flex: 1; }
main svg { display: block; max-width: 100%; height: auto; margin-bottom: 1em; box-shadow: 0 0 4px #aaa; }
</style>
</head>
<body>
<nav><ul id="list"></ul></nav>
<main id="note">Select a note</main>
<script>
const cortexNotes = {};
function CortexIndex(notes) {
	const list = document.getElementById("list");
	for (const n of notes) {
		cortexNotes[n.id] = n;
		const li = document.createElement("li");
		li.innerHTML = '<a href="#/n/' + encodeURIComponent(n.id) + '"></a>';
		li.firstChild.textContent = n.title + (n.status === "ok" ? "" : " (" + n.status + ")");
		li.className = n.status;
		list.appendChild(li);
	}
}
// A warning box; the diagnostic is set as text, never parsed as HTML
function banner(message, diagnostic) {
	const div = document.createElement("div");
	div.className = "banner";
	div.textContent = message;
	const pre = document.createElement("pre");
	pre.textContent = diagnostic;
	div.appendChild(pre);
	return div;
}
function CortexNote(note) {
	const main = document.getElementById("note");
	main.innerHTML = note.pages.join("");
	const entry = cortexNotes[note.id];
	if (entry && entry.status === "stale") {
		main.prepend(banner("This note failed to rebuild; showing the last version that compiled.", entry.diagnostic));
	}
	document.title = note.title;
}
function show() {
	const m = location.hash.match(/^#\/n\/(.+)$/);
	if (!m) return;
	const entry = cortexNotes[decodeURIComponent(m[1])];
	if (entry && entry.status === "failed") {
		document.getElementById("note").replaceChildren(banner("This note has not compiled successfully yet.", entry.diagnostic));
		return;
	}
	const s = document.createElement("script");
	s.src = "notes/" + decodeURIComponent(m[1]) + ".js";
	document.body.appendChild(s);
}
window.addEventListener("hashchange", show);
</script>
<script src="index.js"></script>
<script>show();</script>
</body>
</html>
)HTML";

Emitter::Emitter(std::filesystem::path outputDir) : OutputDir(std::move(outputDir))
{
}

std::filesystem::path Emitter::OutputFor(const Note &note, bool pdf) const
{
	if (pdf)
	{
		auto target = OutputDir / note.RelativePath;
		target.replace_extension(".pdf");
		return target;
	}
	return OutputDir / "notes" / (note.Id + ".js");
}

std::vector<std::filesystem::path> Emitter::ViewerFiles() const
{
	return {OutputDir / "index.js", OutputDir / "index.html"};
}

void Emitter::WriteNote(const Note &note, const std::vector<std::string> &pages)
{
	auto target = OutputFor(note, false);
	JSL::IO::mkdir(target.parent_path(), JSL::IO::Policy::Generous);

	std::string js = "CortexNote({\"id\":" + Utility::JsonString(note.Id) + ",\"title\":" + Utility::JsonString(note.Title) + ",\"pages\":[";
	for (size_t i = 0; i < pages.size(); ++i)
	{
		js += (i > 0 ? "," : "") + Utility::JsonString(pages[i]);
	}
	js += "]});\n";
	Utility::WriteAtomic(target, js);
}

void Emitter::WritePdf(const Note &note, const std::filesystem::path &pdf)
{
	auto target = OutputFor(note, true);
	JSL::IO::mkdir(target.parent_path(), JSL::IO::Policy::Generous);
	auto temporary = target;
	temporary += ".tmp";
	std::filesystem::copy_file(pdf, temporary, std::filesystem::copy_options::overwrite_existing);
	std::filesystem::rename(temporary, target); // as in WriteAtomic: never a half-copied PDF in place of the old one
}

void Emitter::WriteIndex(std::string_view indexJs)
{
	JSL::IO::mkdir(OutputDir, JSL::IO::Policy::Generous);
	Utility::WriteAtomic(OutputDir / "index.js", indexJs);
	Utility::WriteAtomic(OutputDir / "index.html", StubViewer);
}
