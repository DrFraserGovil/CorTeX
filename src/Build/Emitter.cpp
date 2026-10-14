#include "Emitter.h"
#include <JSL.h>

// Encodes a string as a JSON string literal (which is also a valid JS string literal)
static std::string JsonString(std::string_view input)
{
	std::string out = "\"";
	out.reserve(input.size() + 2);
	for (char c : input)
	{
		switch (c)
		{
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(c) < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					out += buf;
				}
				else
				{
					out += c;
				}
		}
	}
	return out + "\"";
}

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
main { padding: 1em; flex: 1; }
main svg { display: block; max-width: 100%; height: auto; margin-bottom: 1em; box-shadow: 0 0 4px #aaa; }
</style>
</head>
<body>
<nav><ul id="list"></ul></nav>
<main id="note">Select a note</main>
<script>
function CortexIndex(notes) {
	const list = document.getElementById("list");
	for (const n of notes) {
		const li = document.createElement("li");
		li.innerHTML = '<a href="#/n/' + encodeURIComponent(n.id) + '"></a>';
		li.firstChild.textContent = n.title;
		list.appendChild(li);
	}
}
function CortexNote(note) {
	document.getElementById("note").innerHTML = note.pages.join("");
	document.title = note.title;
}
function show() {
	const m = location.hash.match(/^#\/n\/(.+)$/);
	if (!m) return;
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

void Emitter::WriteNote(const Note &note, const std::vector<std::string> &pages)
{
	auto target = OutputDir / "notes" / (note.Id + ".js");
	JSL::IO::mkdir(target.parent_path(), JSL::IO::Policy::Generous);

	std::string js = "CortexNote({\"id\":" + JsonString(note.Id) + ",\"title\":" + JsonString(note.Title) + ",\"pages\":[";
	for (size_t i = 0; i < pages.size(); ++i)
	{
		js += (i > 0 ? "," : "") + JsonString(pages[i]);
	}
	js += "]});\n";
	JSL::IO::writeString(target, js);
}

void Emitter::WritePdf(const Note &note, const std::filesystem::path &pdf)
{
	auto target = OutputDir / note.RelativePath;
	target.replace_extension(".pdf");
	JSL::IO::mkdir(target.parent_path(), JSL::IO::Policy::Generous);
	std::filesystem::copy_file(pdf, target, std::filesystem::copy_options::overwrite_existing);
}

void Emitter::WriteIndex(const std::vector<Note> &notes)
{
	JSL::IO::mkdir(OutputDir, JSL::IO::Policy::Generous);

	std::string js = "CortexIndex([\n";
	for (size_t i = 0; i < notes.size(); ++i)
	{
		auto &n = notes[i];
		js += (i > 0 ? ",\n" : "") + std::string("{\"id\":") + JsonString(n.Id) + ",\"title\":" + JsonString(n.Title) + ",\"path\":" + JsonString(n.RelativePath.generic_string()) + "}";
	}
	js += "\n]);\n";
	JSL::IO::writeString(OutputDir / "index.js", js);
	JSL::IO::writeString(OutputDir / "index.html", StubViewer);
}
