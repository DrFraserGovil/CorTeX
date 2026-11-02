// The language server: its transport, and each feature answered from a scan of the fixture (DESIGN.md §14)
#include "Lsp/Features.h"
#include "Lsp/Protocol.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>

using Lsp::Json;

namespace
{
	const std::filesystem::path Root = std::filesystem::canonical(CORTEX_FIXTURE);

	const Build::Scan &Scan()
	{
		static auto scan = Build::Scan::Read(Root);
		return scan;
	}

	//! A document at file (relative to the fixture) holding text, the cursor where its last | is (which is removed)
	struct Cursor
	{
		Lsp::Document Document;
		Lsp::Position Position;
	};

	Cursor At(const std::string &file, std::string text)
	{
		Cursor out;
		auto bar = text.rfind('|');
		auto before = text.substr(0, bar);
		out.Position.Line = std::count(before.begin(), before.end(), '\n');
		out.Position.Character = bar - (before.rfind('\n') == std::string::npos ? 0 : before.rfind('\n') + 1);
		text.erase(bar, 1);
		out.Document = {Root / file, text};
		return out;
	}

	std::vector<std::string> Labels(const Json &items)
	{
		std::vector<std::string> out;
		for (auto &item : items) out.push_back(item["label"]);
		return out;
	}

	bool Has(const std::vector<std::string> &list, const std::string &item) { return std::find(list.begin(), list.end(), item) != list.end(); }

	constexpr auto Utf8 = Lsp::Encoding::Utf8;
} // namespace

TEST_CASE("Messages are framed by Content-Length", "[lsp]")
{
	std::istringstream in("Content-Length: 16\r\n\r\n{\"id\":1,\"a\":[2]}Content-Length: 4\r\nX-Other: y\r\n\r\nnull");
	std::ostringstream out;
	Lsp::Channel channel(in, out);
	auto first = channel.Read();
	REQUIRE(first);
	CHECK((*first)["a"][0] == 2);
	auto second = channel.Read();
	REQUIRE(second);
	CHECK(second->is_null());
	CHECK_FALSE(channel.Read());

	channel.Respond(1, "ok");
	auto sent = out.str();
	auto body = sent.substr(sent.find("\r\n\r\n") + 4);
	CHECK(sent.starts_with("Content-Length: " + std::to_string(body.size()) + "\r\n\r\n"));
	CHECK(Json::parse(body)["result"] == "ok");
}

TEST_CASE("A build's progress: begun at its first note, reported at each, ended once", "[lsp]")
{
	auto sent = [](const std::function<void(Lsp::BuildProgress &)> &build) {
		std::istringstream in;
		std::ostringstream out;
		Lsp::Channel channel(in, out);
		Lsp::BuildProgress progress(channel, "t");
		build(progress);
		std::istringstream back(out.str());
		Lsp::Channel reader(back, out);
		std::vector<Json> messages;
		while (auto message = reader.Read()) messages.push_back(*message);
		return messages;
	};

	// nothing compiled: nothing shown
	CHECK(sent([](auto &p) { p.End(0); }).empty());

	auto messages = sent([](auto &p) {
		p.Compiling("a.ctx", 1, 2);
		p.Compiling("b/c.ctx", 2, 2);
		p.End(1);
	});
	REQUIRE(messages.size() == 5);
	CHECK(messages[0]["method"] == "window/workDoneProgress/create");
	CHECK(messages[0]["params"]["token"] == "t");
	CHECK(messages[1]["params"]["value"]["kind"] == "begin");
	CHECK(messages[2]["params"]["value"]["message"] == "a.ctx (1/2)");
	CHECK(messages[3]["params"]["value"]["message"] == "b/c.ctx (2/2)");
	CHECK(messages[3]["params"]["value"]["percentage"] == 50);
	CHECK(messages[4]["params"]["value"]["kind"] == "end");
	CHECK(messages[4]["params"]["value"]["message"] == "2 notes compiled, 1 failed");
	for (size_t i = 1; i < messages.size(); ++i) CHECK(messages[i]["method"] == "$/progress");
}

TEST_CASE("URIs and positions", "[lsp]")
{
	CHECK(Lsp::PathOf("file:///home/a%20b/n.ctx") == "/home/a b/n.ctx");
	CHECK(Lsp::UriOf("/home/a b/n.ctx") == "file:///home/a%20b/n.ctx");
	CHECK(Lsp::PathOf("https://x").empty());
	// "é" is two bytes in UTF-8, one UTF-16 unit; "𝑥" four bytes, two UTF-16 units
	std::string line = "é𝑥z";
	CHECK(Lsp::ByteOf(line, 1, Lsp::Encoding::Utf16) == 2);
	CHECK(Lsp::ByteOf(line, 3, Lsp::Encoding::Utf16) == 6);
	CHECK(Lsp::CharacterOf(line, 6, Lsp::Encoding::Utf16) == 3);
	CHECK(Lsp::CharacterOf(line, 6, Lsp::Encoding::Utf32) == 2);
	CHECK(Lsp::ByteOf(line, 99, Utf8) == line.size());
}

TEST_CASE("Completion offers names in links and header fields, and labels after #", "[lsp]")
{
	auto names = At("home.tex", "title: X\n-----\nsee [[!Al|");
	auto items = Lsp::Complete(Scan(), names.Document, names.Position, Utf8);
	CHECK(Has(Labels(items), "Alpha"));
	CHECK(Has(Labels(items), "Euler")); // an alias
	CHECK(items[0]["textEdit"]["range"]["start"]["character"] == 7); // replaces what's typed after the markers

	auto labels = At("home.tex", "title: X\n-----\n[[Euler#|]]");
	CHECK(Labels(Lsp::Complete(Scan(), labels.Document, labels.Position, Utf8)) == std::vector<std::string>{"eq:euler"});
	auto anywhere = At("home.tex", "title: X\n-----\n[[#|]]");
	CHECK(Has(Labels(Lsp::Complete(Scan(), anywhere.Document, anywhere.Position, Utf8)), "eq:euler"));

	auto text = At("home.tex", "title: X\n-----\n[[Alpha|the |]]");
	CHECK(Lsp::Complete(Scan(), text.Document, text.Position, Utf8).empty());
	auto outside = At("home.tex", "title: X\n-----\nplain |text");
	CHECK(Lsp::Complete(Scan(), outside.Document, outside.Position, Utf8).empty());

	auto redirect = At("stubs/new.tex", "title: X\nredirect: Al|\n-----\n");
	CHECK(Has(Labels(Lsp::Complete(Scan(), redirect.Document, redirect.Position, Utf8)), "Alpha"));
	auto sidebar = At("home.tex", "title: X\nsidebar: Beta, Al|\n-----\n");
	auto sidebarItems = Lsp::Complete(Scan(), sidebar.Document, sidebar.Position, Utf8);
	REQUIRE_FALSE(sidebarItems.empty());
	CHECK(sidebarItems[0]["textEdit"]["range"]["start"]["character"] == 15);
}

TEST_CASE("Definition follows links, redirects and labels", "[lsp]")
{
	auto link = At("home.tex", "x\n-----\n[[Al|pha]]");
	CHECK(Lsp::Definition(Scan(), link.Document, link.Position, Utf8)["uri"] == Lsp::UriOf(Root / "alpha.tex"));
	auto stub = At("home.tex", "x\n-----\n[[Old |Name]]");
	CHECK(Lsp::Definition(Scan(), stub.Document, stub.Position, Utf8)["uri"] == Lsp::UriOf(Root / "alpha.tex"));
	auto label = At("home.tex", "x\n-----\n[[#eq:eu|ler]]");
	auto location = Lsp::Definition(Scan(), label.Document, label.Position, Utf8);
	CHECK(location["uri"] == Lsp::UriOf(Root / "maths/euler.tex"));
	CHECK(location["range"]["start"]["line"] == 5); // the \label's line in the file
	CHECK(location["range"]["start"]["character"] == 17);
	auto wanted = At("home.tex", "x\n-----\n[[Nowh|ere]]");
	CHECK(Lsp::Definition(Scan(), wanted.Document, wanted.Position, Utf8).is_null());
}

TEST_CASE("Hover describes the target", "[lsp]")
{
	auto link = At("home.tex", "x\n-----\n[[~Eul|er]]");
	auto hover = Lsp::Hover(Scan(), link.Document, link.Position, Utf8)["contents"]["value"].get<std::string>();
	CHECK(hover.find("Euler's Identity") != std::string::npos);
	CHECK(hover.find("needs a proof") != std::string::npos);
	CHECK(hover.find("loose") != std::string::npos);
	auto wanted = At("home.tex", "x\n-----\n[[Nowh|ere]]");
	CHECK(Lsp::Hover(Scan(), wanted.Document, wanted.Position, Utf8)["contents"]["value"].get<std::string>().find("wanted") != std::string::npos);
}

TEST_CASE("References are the backlinks, at each link", "[lsp]")
{
	// on a link: references to its target, wherever they are
	auto link = At("home.tex", "x\n-----\n[[Al|pha]]");
	auto locations = Lsp::References(Scan(), link.Document, link.Position, Utf8);
	bool fromHome = false;
	for (auto &location : locations)
	{
		if (location["uri"] != Lsp::UriOf(Root / "home.tex")) continue;
		fromHome = true;
		CHECK(location["range"]["start"]["line"] == 4);
		CHECK(location["range"]["start"]["character"] == 9);
		CHECK(location["range"]["end"]["character"] == 18);
	}
	CHECK(fromHome);
	// elsewhere: references to the document's own note
	auto own = At("alpha.tex", "title: Alpha\n-----\n|text");
	CHECK(Lsp::References(Scan(), own.Document, own.Position, Utf8).size() == locations.size());
}

TEST_CASE("Formatting and diagnostics", "[lsp]")
{
	auto document = Lsp::Document{Root / "home.tex", "t: x\n-----\n# A\n  text\n"};
	auto edits = Lsp::Formatting(Scan(), document, Utf8);
	REQUIRE(edits.size() == 1);
	CHECK(edits[0]["newText"] == "t: x\n-----\n# A\n\ttext\n");
	CHECK(edits[0]["range"]["end"]["line"] == 4);
	CHECK(Lsp::Formatting(Scan(), {Root / "home.tex", edits[0]["newText"]}, Utf8).empty());

	Report::Log report;
	report.Add(Report::Kind::Syntax, "bad link", "home.tex", 3);
	report.Add(Report::Kind::Failure, "alpha.tex:2: Undefined control sequence.", "home.tex");
	report.Add(Report::Kind::Skipped, "skipped", "fragment.tex");
	auto diagnostics = Lsp::Diagnostics(report, Root);
	CHECK(diagnostics.size() == 2);
	auto &home = diagnostics[Lsp::UriOf(Root / "home.tex")];
	REQUIRE(home.size() == 1);
	CHECK(home[0]["range"]["start"]["line"] == 2);
	CHECK(home[0]["severity"] == 2);
	auto &alpha = diagnostics[Lsp::UriOf(Root / "alpha.tex")]; // where LaTeX stopped, in the note transcluded into home
	REQUIRE(alpha.size() == 1);
	CHECK(alpha[0]["severity"] == 1);
	CHECK(alpha[0]["range"]["start"]["line"] == 1);
}

TEST_CASE("Semantic tokens mark each link resolved or not", "[lsp]")
{
	// header and cortex-off links are left alone; the token covers the target and label, not the markers or |text
	Lsp::Document document{Root / "home.tex", "title: [[Alpha]]\n-----\n[[~Alpha|x]] [[Nowhere]]\n% cortex off\n[[Alpha]]\n% cortex on\n[[Euler#eq:euler]] [[!Beta#x]]\n"};
	auto data = Lsp::SemanticTokens(Scan(), document, Utf8)["data"];
	REQUIRE(data.size() == 4 * 5);
	auto token = [&](size_t i) { return std::vector<int>(data.begin() + 5 * i, data.begin() + 5 * i + 5); };
	constexpr int resolved = 1, unresolved = 2, transclusion = 4, label = 8, loose = 16;
	CHECK(token(0) == std::vector<int>{2, 3, 5, 0, resolved | loose});
	CHECK(token(1) == std::vector<int>{0, 12, 7, 0, unresolved});
	CHECK(token(2) == std::vector<int>{4, 2, 14, 0, resolved | label});
	CHECK(token(3) == std::vector<int>{0, 20, 6, 0, unresolved | transclusion | label}); // a transclusion can't name a label
}
