#pragma once
#include <filesystem>
#include <iosfwd>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

//! The language server's side of the Language Server Protocol's transport (DESIGN.md §13.7): JSON-RPC messages, each preceded by a
//! Content-Length header, over a pair of streams (the editor's pipes to and from cortex lsp)
namespace Lsp
{
	using Json = nlohmann::json;

	class Channel
	{
	  public:
		Channel(std::istream &in, std::ostream &out) : In(in), Out(out)
		{
		}

		//! The next message; nullopt once the input ends. A message which isn't valid JSON is skipped (with a log entry)
		std::optional<Json> Read();

		//! Sends a message; safe to call from any thread (whole messages never interleave)
		void Send(const Json &message);
		void Respond(const Json &id, Json result);
		void Fail(const Json &id, int code, const std::string &message);
		void Notify(const std::string &method, Json params);

	  private:
		std::istream &In;
		std::ostream &Out;
		std::mutex Writing;
	};

	//! One build's progress, shown by the editor as LSP work-done progress (window/workDoneProgress/create, then $/progress): begun at
	//! the first note compiled, so a build which compiles nothing shows nothing; reported at each note; ended with the build
	class BuildProgress
	{
	  public:
		//! token names this build's progress to the editor, so must differ from every other build's
		BuildProgress(Channel &io, std::string token) : Io(io), Token(std::move(token))
		{
		}

		//! The note about to compile, and its place (from 1) among the notes compiling this round
		void Compiling(const std::filesystem::path &note, size_t number, size_t of);
		//! The build is over: failed is how many notes failed (or, with stopped, why the build stopped). Nothing is sent if nothing began
		void End(size_t failed, const std::string &stopped = "");

	  private:
		void Progress(Json value);

		Channel &Io;
		std::string Token;
		bool Begun = false;
		size_t Compiled = 0; //!< every note compiled, across rounds
	};

	//! The standard JSON-RPC and LSP error codes the server uses
	namespace Error
	{
		constexpr int MethodNotFound = -32601;
		constexpr int InvalidParams = -32602;
		constexpr int ServerNotInitialized = -32002;
		constexpr int RequestFailed = -32803;
	} // namespace Error

	//! A file:// URI as a path (percent-escapes decoded); empty if it isn't a file URI
	std::filesystem::path PathOf(std::string_view uri);
	//! A path (absolute) as a file:// URI
	std::string UriOf(const std::filesystem::path &path);

	//! How positions count characters within a line: negotiated at initialisation (UTF-8 if the editor offers it, else LSP's default)
	enum class Encoding
	{
		Utf8,
		Utf16,
		Utf32
	};
	//! The byte offset in line of an LSP character offset (clamped to the line's end)
	size_t ByteOf(std::string_view line, size_t character, Encoding encoding);
	//! The LSP character offset of a byte offset in line
	size_t CharacterOf(std::string_view line, size_t byte, Encoding encoding);
} // namespace Lsp
