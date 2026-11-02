#include "Protocol.h"
#include <JSL.h>
#include <cctype>
#include <istream>
#include <ostream>

std::optional<Lsp::Json> Lsp::Channel::Read()
{
	while (true)
	{
		/// THE HEADER: LINES UNTIL A BLANK ONE; ONLY Content-Length MATTERS
		size_t length = 0;
		bool sized = false;
		std::string line;
		while (std::getline(In, line))
		{
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty()) break;
			auto colon = line.find(':');
			if (colon != std::string::npos && JSL::String::iEquals(JSL::String::trim(std::string_view(line).substr(0, colon)), "content-length"))
			{
				try
				{
					length = std::stoul(JSL::String::trim(std::string_view(line).substr(colon + 1)));
					sized = true;
				}
				catch (std::exception &)
				{
					sized = false;
				}
			}
		}
		if (!In) return std::nullopt;
		if (!sized) continue; // a header without a length: nothing can be read for it

		/// THE CONTENT
		std::string content(length, '\0');
		In.read(content.data(), static_cast<std::streamsize>(length));
		if (static_cast<size_t>(In.gcount()) != length) return std::nullopt;
		try
		{
			return Json::parse(content);
		}
		catch (std::exception &e)
		{
			LOG(WARN) << "Ignored a message from the editor which isn't valid JSON: " << e.what();
		}
	}
}

void Lsp::Channel::Send(const Json &message)
{
	auto content = message.dump(-1, ' ', false, Json::error_handler_t::replace);
	std::lock_guard lock(Writing);
	Out << "Content-Length: " << content.size() << "\r\n\r\n" << content << std::flush;
}

void Lsp::Channel::Respond(const Json &id, Json result)
{
	Send({{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}});
}

void Lsp::Channel::Fail(const Json &id, int code, const std::string &message)
{
	Send({{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}});
}

void Lsp::Channel::Notify(const std::string &method, Json params)
{
	Send({{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}});
}

namespace
{
	int Hex(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	//! The length in bytes of the UTF-8 sequence starting with lead
	size_t SequenceLength(unsigned char lead)
	{
		if (lead < 0x80) return 1;
		if ((lead >> 5) == 0x6) return 2;
		if ((lead >> 4) == 0xE) return 3;
		if ((lead >> 3) == 0x1E) return 4;
		return 1; // not valid UTF-8: one byte at a time
	}

	//! How many units of the encoding a UTF-8 sequence of this length is
	size_t Units(size_t bytes, Lsp::Encoding encoding)
	{
		switch (encoding)
		{
			case Lsp::Encoding::Utf8:
				return bytes;
			case Lsp::Encoding::Utf16:
				return bytes == 4 ? 2 : 1;
			default:
				return 1;
		}
	}
} // namespace

std::filesystem::path Lsp::PathOf(std::string_view uri)
{
	constexpr std::string_view scheme = "file://";
	if (uri.substr(0, scheme.size()) != scheme) return {};
	auto rest = uri.substr(scheme.size());
	std::string decoded;
	for (size_t i = 0; i < rest.size(); ++i)
	{
		if (rest[i] == '%' && i + 2 < rest.size() && Hex(rest[i + 1]) >= 0 && Hex(rest[i + 2]) >= 0)
		{
			decoded += static_cast<char>(Hex(rest[i + 1]) * 16 + Hex(rest[i + 2]));
			i += 2;
		}
		else decoded += rest[i];
	}
#ifdef _WIN32
	// file:///C:/notes/a.ctx: the drive follows the third slash
	if (decoded.size() > 2 && decoded[0] == '/' && decoded[2] == ':') decoded.erase(0, 1);
#endif
	return std::filesystem::path(decoded).lexically_normal();
}

std::string Lsp::UriOf(const std::filesystem::path &path)
{
	auto text = path.generic_string();
	std::string uri = "file://";
	if (!text.empty() && text[0] != '/') uri += '/'; // a Windows drive
	static const char *hex = "0123456789ABCDEF";
	for (unsigned char c : text)
	{
		if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' || c == ':') uri += static_cast<char>(c);
		else
		{
			uri += '%';
			uri += hex[c >> 4];
			uri += hex[c & 0xF];
		}
	}
	return uri;
}

size_t Lsp::ByteOf(std::string_view line, size_t character, Encoding encoding)
{
	size_t byte = 0, units = 0;
	while (byte < line.size() && units < character)
	{
		auto length = std::min(SequenceLength(static_cast<unsigned char>(line[byte])), line.size() - byte);
		units += Units(length, encoding);
		byte += length;
	}
	return byte;
}

size_t Lsp::CharacterOf(std::string_view line, size_t byte, Encoding encoding)
{
	size_t at = 0, units = 0;
	byte = std::min(byte, line.size());
	while (at < byte)
	{
		auto length = std::min(SequenceLength(static_cast<unsigned char>(line[at])), line.size() - at);
		units += Units(length, encoding);
		at += length;
	}
	return units;
}

void Lsp::BuildProgress::Progress(Json value)
{
	Io.Notify("$/progress", {{"token", Token}, {"value", std::move(value)}});
}

void Lsp::BuildProgress::Compiling(const std::filesystem::path &note, size_t number, size_t of)
{
	if (!Begun)
	{
		Begun = true;
		// a request, whose answer the server doesn't wait for: the editor handles messages in order, so the token exists by the first $/progress
		Io.Send({{"jsonrpc", "2.0"}, {"id", "cortex-progress-" + Token}, {"method", "window/workDoneProgress/create"}, {"params", {{"token", Token}}}});
		Progress({{"kind", "begin"}, {"title", "Building"}, {"cancellable", false}, {"percentage", 0}});
	}
	++Compiled;
	auto percentage = of ? static_cast<int>((number - 1) * 100 / of) : 0;
	Progress({{"kind", "report"}, {"message", note.generic_string() + " (" + std::to_string(number) + "/" + std::to_string(of) + ")"}, {"percentage", percentage}});
}

void Lsp::BuildProgress::End(size_t failed, const std::string &stopped)
{
	if (!Begun) return;
	Begun = false;
	std::string message = std::to_string(Compiled) + (Compiled == 1 ? " note" : " notes") + " compiled";
	if (failed) message += ", " + std::to_string(failed) + " failed";
	if (!stopped.empty()) message = "stopped: " + stopped;
	Progress({{"kind", "end"}, {"message", message}});
}
