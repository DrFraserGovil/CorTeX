#include "Server.h"
#include "App/Session.h"
#include "Features.h"
#include "Project/Lock.h"
#include "Project/Paths.h"
#include "Protocol.h"
#include "Settings/Config.h"
#include <JSL.h>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <thread>

namespace
{
	using Lsp::Json;

	class Server
	{
	  public:
		Server(std::istream &in, std::ostream &out) : Io(in, out)
		{
		}

		int Run();

	  private:
		Lsp::Channel Io;
		std::filesystem::path Root;
		Lsp::Encoding Encoding = Lsp::Encoding::Utf16;
		bool Initialised = false;
		bool ShuttingDown = false;

		//! The project as last read: swapped whole by the watch thread, read by the requests
		std::mutex SnapshotMutex;
		std::shared_ptr<const Build::Scan> Snapshot;

		//! The documents open in the editor, by URI (only the main thread touches them)
		std::map<std::string, Lsp::Document> Documents;
		//! The files given diagnostics last time (only the watch thread touches them)
		std::set<std::string> Diagnosed;

		JSL::Async::Watcher::Panopticon Eye;
		std::thread Watcher;
		std::atomic<bool> Holder{false};
		//! Whether the editor can be asked to re-request semantic tokens (once it is ready for requests from the server)
		bool CanRefresh = false;
		//! Whether the editor shows work-done progress (each build's, as it compiles)
		bool CanShowProgress = false;
		std::atomic<int> Builds{0};
		//! Kept between builds (while holding the project), so each starts with XeLaTeX already waiting with the preamble loaded
		Tex::WarmPool Warm;
		std::atomic<bool> Ready{false};
		std::atomic<int> Requests{0};

		std::shared_ptr<const Build::Scan> Current()
		{
			std::lock_guard lock(SnapshotMutex);
			return Snapshot;
		}

		void Show(int type, const std::string &message) { Io.Notify("window/showMessage", {{"type", type}, {"message", "CorTeX: " + message}}); }

		void Publish(Build::Scan &&scan, const Report::Log &report);
		void Rescan();
		void Watch();
		void Stop();

		void Initialize(const Json &id, const Json &params);
		void Request(const std::string &method, const Json &id, const Json &params);
		void Notification(const std::string &method, const Json &params);
		Lsp::Document DocumentAt(const Json &params);
	};

	/// THE PROJECT: READ AND (WHEN HOLDING IT) BUILT ON THE WATCH THREAD; EACH RESULT REPLACES THE SNAPSHOT AND THE DIAGNOSTICS

	void Server::Publish(Build::Scan &&scan, const Report::Log &report)
	{
		{
			std::lock_guard lock(SnapshotMutex);
			Snapshot = std::make_shared<const Build::Scan>(std::move(scan));
		}
		auto diagnostics = Lsp::Diagnostics(report, Root);
		for (auto &uri : Diagnosed)
		{
			if (!diagnostics.contains(uri)) Io.Notify("textDocument/publishDiagnostics", {{"uri", uri}, {"diagnostics", Json::array()}});
		}
		Diagnosed.clear();
		for (auto &[uri, list] : diagnostics)
		{
			Io.Notify("textDocument/publishDiagnostics", {{"uri", uri}, {"diagnostics", list}});
			Diagnosed.insert(uri);
		}
		// which links resolve may have changed (a note added, renamed or re-aliased): the editor re-colours them
		if (Ready && CanRefresh)
		{
			Io.Send({{"jsonrpc", "2.0"}, {"id", "cortex-refresh-" + std::to_string(++Requests)}, {"method", "workspace/semanticTokens/refresh"}});
		}
	}

	//! Reads the project without compiling anything: what a passive server does on a change, and every server does on starting
	void Server::Rescan()
	{
		try
		{
			auto scan = Build::Scan::Read(Root);
			Report::Log report;
			report.Absorb(std::move(scan.Problems));
			Publish(std::move(scan), report);
		}
		catch (std::exception &e)
		{
			Show(1, e.what());
		}
	}

	void Server::Watch()
	{
		try
		{
			auto build = [&](bool clean) {
				Lsp::BuildProgress progress(Io, "cortex-build-" + std::to_string(++Builds));
				Build::Options options{.Warm = &Warm};
				if (CanShowProgress) options.Told = [&](const std::filesystem::path &note, size_t number, size_t of) { progress.Compiling(note, number, of); };
				try
				{
					size_t failed = 0;
					App::BuildOnce(
						Root, clean,
						[&](Build::Scan &&scan, const Report::Log &report) {
							failed = report.Count(Report::Kind::Failure);
							Publish(std::move(scan), report);
						},
						options);
					progress.End(failed);
				}
				catch (std::exception &e)
				{
					progress.End(0, e.what());
					Show(1, std::string("the build failed: ") + e.what());
				}
			};

			/// HOLD THE PROJECT IF NO ONE ELSE DOES: THEN OTHER COMMANDS' REQUESTS ARRIVE HERE, AS THEY WOULD AT A WATCH
			std::optional<Project::Lock> lock;
			try
			{
				lock.emplace(Root, Eye, [&]() {
					if (!App::Serve(Root, *lock, build))
					{
						Show(3, "asked to stop watching the project; it won't be rebuilt until the editor restarts the server");
						Eye.Stop();
					}
				});
				Holder = true;
			}
			catch (std::exception &)
			{
				Io.Notify("window/logMessage", {{"type", 3}, {"message", "CorTeX: another process (a watch, or another editor) is building the project, so this server only reads it"}});
			}

			/// FILE CHANGES: REBUILD (HOLDING) OR RE-READ (PASSIVE), ONCE PER BATCH
			Report::Log ignored;
			auto output = Settings::Config::Load(Root, ignored).Output;
			Eye.SetFileBatchCallback(Root, true, [&](std::set<JSL::Async::Watcher::FileChange> batch) {
				for (auto &change : batch)
				{
					if (!App::AffectsBuild(Root, output, change.Path)) continue;
					if (Holder) build(false);
					else Rescan();
					output = Settings::Config::Load(Root, ignored).Output;
					return;
				}
			});
			Eye.SetDebounceTime(300);

			if (Holder) build(false); // bring the output up to date, as a watch does on starting
			Eye.Start();
		}
		catch (std::exception &e)
		{
			Show(1, std::string("stopped watching the project: ") + e.what());
		}
	}

	void Server::Stop()
	{
		if (!Watcher.joinable()) return;
		Eye.Stop(); // thread-safe: queued for the watch thread, which finishes what it is doing first
		Watcher.join();
	}

	/// REQUESTS

	void Server::Initialize(const Json &id, const Json &params)
	{
		std::filesystem::path start;
		if (params.contains("rootUri") && params["rootUri"].is_string()) start = Lsp::PathOf(params["rootUri"].get<std::string>());
		else if (params.contains("rootPath") && params["rootPath"].is_string()) start = params["rootPath"].get<std::string>();
		Root = start.empty() ? std::filesystem::path() : Project::Paths::Find(start);
		if (Root.empty())
		{
			Io.Fail(id, Lsp::Error::RequestFailed, "there is no CorTeX project at " + start.string() + " (a folder with a .cortex folder in it)");
			return;
		}

		// UTF-8 positions if the editor offers them (no conversion at all), else the protocol's default, UTF-16
		auto offered = params.value("/capabilities/general/positionEncodings"_json_pointer, Json::array());
		Encoding = Lsp::Encoding::Utf16;
		for (auto &encoding : offered)
		{
			if (encoding == "utf-8") Encoding = Lsp::Encoding::Utf8;
		}
		const char *encodingName = Encoding == Lsp::Encoding::Utf8 ? "utf-8" : "utf-16";
		CanRefresh = params.value("/capabilities/workspace/semanticTokens/refreshSupport"_json_pointer, false);
		CanShowProgress = params.value("/capabilities/window/workDoneProgress"_json_pointer, false);

		Rescan(); // so the first requests have something to answer from, before any build
		Initialised = true;
		Json capabilities = {
			{"positionEncoding", encodingName},
			{"textDocumentSync", {{"openClose", true}, {"change", 1}, {"save", {{"includeText", false}}}}},
			{"completionProvider", {{"triggerCharacters", {"[", "#", ":", ","}}}},
			{"definitionProvider", true},
			{"hoverProvider", true},
			{"referencesProvider", true},
			{"documentFormattingProvider", true},
			{"semanticTokensProvider", {{"legend", Lsp::TokenLegend()}, {"full", true}}},
		};
		Io.Respond(id, {{"capabilities", capabilities}, {"serverInfo", {{"name", "cortex"}}}});
	}

	Lsp::Document Server::DocumentAt(const Json &params)
	{
		auto uri = params.at("textDocument").at("uri").get<std::string>();
		if (auto open = Documents.find(uri); open != Documents.end()) return open->second;
		Lsp::Document document{Lsp::PathOf(uri), ""};
		if (std::filesystem::is_regular_file(document.File)) document.Text = JSL::IO::getFile(document.File);
		return document;
	}

	void Server::Request(const std::string &method, const Json &id, const Json &params)
	{
		if (method == "initialize") return Initialize(id, params);
		if (!Initialised) return Io.Fail(id, Lsp::Error::ServerNotInitialized, "the server hasn't been initialised");
		if (method == "shutdown")
		{
			ShuttingDown = true;
			Stop();
			return Io.Respond(id, nullptr);
		}
		if (ShuttingDown) return Io.Fail(id, -32600, "the server is shutting down");

		auto scan = Current();
		if (!scan) return Io.Fail(id, Lsp::Error::RequestFailed, "the project couldn't be read; see the earlier message");
		auto position = [&]() { return Lsp::Position{params.at("position").at("line").get<size_t>(), params.at("position").at("character").get<size_t>()}; };
		try
		{
			if (method == "textDocument/completion") return Io.Respond(id, Lsp::Complete(*scan, DocumentAt(params), position(), Encoding));
			if (method == "textDocument/definition") return Io.Respond(id, Lsp::Definition(*scan, DocumentAt(params), position(), Encoding));
			if (method == "textDocument/hover") return Io.Respond(id, Lsp::Hover(*scan, DocumentAt(params), position(), Encoding));
			if (method == "textDocument/references") return Io.Respond(id, Lsp::References(*scan, DocumentAt(params), position(), Encoding));
			if (method == "textDocument/formatting") return Io.Respond(id, Lsp::Formatting(*scan, DocumentAt(params), Encoding));
			if (method == "textDocument/semanticTokens/full") return Io.Respond(id, Lsp::SemanticTokens(*scan, DocumentAt(params), Encoding));
		}
		catch (std::exception &e)
		{
			return Io.Fail(id, Lsp::Error::InvalidParams, e.what());
		}
		Io.Fail(id, Lsp::Error::MethodNotFound, "CorTeX doesn't offer " + method);
	}

	void Server::Notification(const std::string &method, const Json &params)
	{
		try
		{
			if (method == "initialized")
			{
				Ready = true;
				if (Initialised && !Watcher.joinable()) Watcher = std::thread([this] { Watch(); });
			}
			else if (method == "textDocument/didOpen")
			{
				auto &document = params.at("textDocument");
				auto uri = document.at("uri").get<std::string>();
				Documents[uri] = {Lsp::PathOf(uri), document.at("text").get<std::string>()};
			}
			else if (method == "textDocument/didChange")
			{
				// full synchronisation: the last change is the whole text
				auto uri = params.at("textDocument").at("uri").get<std::string>();
				auto &changes = params.at("contentChanges");
				if (!changes.empty()) Documents[uri] = {Lsp::PathOf(uri), changes.back().at("text").get<std::string>()};
			}
			else if (method == "textDocument/didClose")
			{
				Documents.erase(params.at("textDocument").at("uri").get<std::string>());
			}
			// didSave needs nothing: the file watcher sees the save, as it sees every other change to the project
		}
		catch (std::exception &e)
		{
			LOG(WARN) << "Ignored a malformed " << method << " from the editor: " << e.what();
		}
	}

	int Server::Run()
	{
		bool exited = false;
		while (auto message = Io.Read())
		{
			auto method = message->value("method", "");
			auto params = message->value("params", Json::object());
			if (method == "exit")
			{
				exited = true;
				break;
			}
			if (method.empty()) continue; // a response to a request of ours: the server sends none
			if (message->contains("id")) Request(method, (*message)["id"], params);
			else Notification(method, params);
		}
		Stop();
		return exited && ShuttingDown ? 0 : 1;
	}
} // namespace

int Lsp::Serve(std::istream &in, std::ostream &out)
{
	return Server(in, out).Run();
}
