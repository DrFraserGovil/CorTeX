#include "Process.h"
#include <JSL.h>
#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
extern char **environ;
#endif

namespace
{
	//! Held while a child is being started. Builds start programs from several threads at once, and anything inheritable made for one
	//! child (a pipe, an output file) mustn't be inherited by another started in between, which would hold it open until it exits
	std::mutex Starting;
}

std::optional<std::filesystem::path> Platform::Find(const std::string &program)
{
	if (program.find('/') != std::string::npos || program.find('\\') != std::string::npos)
	{
		return std::filesystem::exists(program) ? std::optional<std::filesystem::path>(program) : std::nullopt;
	}
	const char *path = std::getenv("PATH");
	if (!path) return std::nullopt;
#if defined(_WIN32)
	const char *extensionList = std::getenv("PATHEXT");
	auto extensions = JSL::String::split(extensionList ? extensionList : ".EXE;.BAT;.CMD", ";");
	extensions.insert(extensions.begin(), "");
#else
	std::vector<std::string> extensions = {""};
#endif
	for (auto &folder : JSL::String::split(path, std::string(1, PathListSeparator)))
	{
		if (folder.empty()) continue;
		for (auto &extension : extensions)
		{
			auto candidate = std::filesystem::path(folder) / (program + extension);
			std::error_code error;
			if (!std::filesystem::is_regular_file(candidate, error)) continue;
#if !defined(_WIN32)
			if (access(candidate.c_str(), X_OK) != 0) continue;
#endif
			return candidate;
		}
	}
	return std::nullopt;
}

#if defined(_WIN32)

namespace
{
	std::wstring Wide(const std::string &text)
	{
		if (text.empty()) return {};
		int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
		std::wstring out(size, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), out.data(), size);
		return out;
	}

	// One argument, quoted as the Microsoft C runtime parses command lines
	std::wstring QuoteArgument(const std::wstring &argument)
	{
		if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
		std::wstring out = L"\"";
		for (size_t i = 0;; ++i)
		{
			size_t slashes = 0;
			while (i < argument.size() && argument[i] == L'\\') ++i, ++slashes;
			if (i == argument.size())
			{
				out.append(slashes * 2, L'\\');
				break;
			}
			if (argument[i] == L'"') out.append(slashes * 2 + 1, L'\\');
			else out.append(slashes, L'\\');
			out += argument[i];
		}
		return out + L"\"";
	}

	std::string LastError()
	{
		return "Windows error " + std::to_string(GetLastError());
	}

	// Starts the command; the caller owns (and must close) the returned handles
	std::optional<PROCESS_INFORMATION> Start(const Platform::Command &command, bool detached, std::string &error)
	{
		if (command.Arguments.empty())
		{
			error = "the command is empty";
			return std::nullopt;
		}
		auto program = Platform::Find(command.Arguments[0]);
		if (!program)
		{
			error = "'" + command.Arguments[0] + "' could not be found";
			return std::nullopt;
		}
		std::wstring line = QuoteArgument(program->wstring());
		for (size_t i = 1; i < command.Arguments.size(); ++i) line += L" " + QuoteArgument(Wide(command.Arguments[i]));

		// the environment: everything inherited, with the command's variables replaced or added
		std::vector<std::pair<std::wstring, std::wstring>> variables;
		if (auto block = GetEnvironmentStringsW())
		{
			for (auto *entry = block; *entry; entry += wcslen(entry) + 1)
			{
				std::wstring text = entry;
				auto equals = text.find(L'=', 1);
				if (equals != std::wstring::npos) variables.push_back({text.substr(0, equals), text.substr(equals + 1)});
			}
			FreeEnvironmentStringsW(block);
		}
		for (auto &[name, value] : command.Environment)
		{
			std::erase_if(variables, [&](auto &v) { return _wcsicmp(v.first.c_str(), Wide(name).c_str()) == 0; });
			variables.push_back({Wide(name), Wide(value)});
		}
		std::wstring environment;
		for (auto &[name, value] : variables) environment += name + L"=" + value + L'\0';
		environment += L'\0';

		std::lock_guard starting(Starting);
		SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
		HANDLE output = INVALID_HANDLE_VALUE;
		auto outputPath = command.OutputFile.empty() ? std::filesystem::path("NUL") : command.OutputFile;
		output = CreateFileW(outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		HANDLE input = CreateFileW(L"NUL", GENERIC_READ, 0, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = input;
		startup.hStdOutput = output;
		startup.hStdError = output;
		PROCESS_INFORMATION process{};
		DWORD flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW | (detached ? DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP : 0);
		auto directory = command.WorkingDirectory.empty() ? std::wstring() : command.WorkingDirectory.wstring();
		BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, flags, environment.data(), directory.empty() ? nullptr : directory.c_str(), &startup, &process);
		if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
		if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
		if (!started)
		{
			error = LastError();
			return std::nullopt;
		}
		return process;
	}
} // namespace

Platform::Outcome Platform::Run(const Command &command)
{
	std::string error;
	auto process = Start(command, false, error);
	if (!process) return {Outcome::Status::CouldNotStart, -1, error};
	DWORD wait = command.Timeout.count() > 0 ? static_cast<DWORD>(command.Timeout.count() * 1000) : INFINITE;
	Outcome outcome{Outcome::Status::Exited};
	if (WaitForSingleObject(process->hProcess, wait) == WAIT_TIMEOUT)
	{
		TerminateProcess(process->hProcess, 1);
		WaitForSingleObject(process->hProcess, INFINITE);
		outcome.State = Outcome::Status::TimedOut;
	}
	else
	{
		DWORD code = 0;
		GetExitCodeProcess(process->hProcess, &code);
		outcome.ExitCode = static_cast<int>(code);
	}
	CloseHandle(process->hThread);
	CloseHandle(process->hProcess);
	return outcome;
}

// Warm workers aren't available on Windows: starting one always fails, so the pool never warms and every note compiles as before
std::unique_ptr<Platform::Child> Platform::Child::Start(const Command &, std::string &error)
{
	error = "not available on Windows";
	return nullptr;
}
Platform::Child::~Child() {}
bool Platform::Child::Finish(std::string_view) { return false; }
Platform::Outcome Platform::Child::Wait(std::chrono::seconds) { return {Outcome::Status::CouldNotStart, -1, "not available on Windows"}; }
bool Platform::Child::Running() { return false; }

std::string Platform::Launch(const Command &command)
{
	std::string error;
	auto process = Start(command, true, error);
	if (!process) return error;
	CloseHandle(process->hThread);
	CloseHandle(process->hProcess);
	return "";
}

std::filesystem::path Platform::Executable()
{
	std::wstring buffer(32768, L'\0');
	auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
	buffer.resize(length);
	return buffer;
}

#else

namespace
{
	// In the child, between fork and exec: only async-signal-safe calls are allowed here. A failure is written down the report pipe
	// (closed on a successful exec, so the parent reads nothing) and the child exits
	[[noreturn]] void Fail(int report, int error)
	{
		[[maybe_unused]] auto written = write(report, &error, sizeof(error));
		_exit(127);
	}

	[[noreturn]] void Become(const Platform::Command &command, const std::vector<char *> &argv, char **environment, int report, int piped, bool detached)
	{
		auto fail = [&](int error) { Fail(report, error); };
		if (detached) setsid(); // its own session: closing whatever started cortex doesn't take it with it
		else setpgid(0, 0);		// its own process group, so a timeout can stop everything it started
		int input = piped >= 0 ? piped : open("/dev/null", O_RDONLY);
		int output = command.OutputFile.empty() ? open("/dev/null", O_WRONLY) : open(command.OutputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (input < 0 || output < 0) fail(errno);
		dup2(input, STDIN_FILENO);
		dup2(output, STDOUT_FILENO);
		dup2(output, STDERR_FILENO);
		if (!command.WorkingDirectory.empty() && chdir(command.WorkingDirectory.c_str()) != 0) fail(errno);
		environ = environment; // execvp passes environ on (and setenv isn't safe here: it allocates, and another thread may hold the allocator)
		execvp(argv[0], argv.data());
		Fail(report, errno);
	}

	// Forks and execs the command; on success returns the child's pid, else sets error. With input, the child's input is a pipe, whose
	// writing end is returned there (else its input is empty)
	pid_t Start(const Platform::Command &command, bool detached, std::string &error, int *input = nullptr)
	{
		if (command.Arguments.empty())
		{
			error = "the command is empty";
			return -1;
		}
		// everything the child needs is prepared before the fork (allocating after it isn't safe)
		std::vector<char *> argv;
		for (auto &argument : command.Arguments) argv.push_back(const_cast<char *>(argument.c_str()));
		argv.push_back(nullptr);
		// the environment: everything inherited, with the command's variables replaced or added
		std::vector<std::string> variables;
		for (char **entry = environ; *entry; ++entry)
		{
			std::string_view text = *entry;
			auto name = text.substr(0, text.find('='));
			if (std::none_of(command.Environment.begin(), command.Environment.end(), [&](auto &v) { return v.first == name; })) variables.emplace_back(text);
		}
		for (auto &[name, value] : command.Environment) variables.push_back(name + "=" + value);
		std::vector<char *> environment;
		for (auto &variable : variables) environment.push_back(variable.data());
		environment.push_back(nullptr);

		std::unique_lock starting(Starting);
		int report[2];
		if (pipe(report) != 0)
		{
			error = std::strerror(errno);
			return -1;
		}
		fcntl(report[1], F_SETFD, FD_CLOEXEC);
		// the input pipe: both ends close on exec (the child's copy, made by dup2, doesn't), so no other child can inherit either
		int piped[2] = {-1, -1};
		if (input)
		{
			if (pipe(piped) != 0)
			{
				error = std::strerror(errno);
				close(report[0]);
				close(report[1]);
				return -1;
			}
			fcntl(piped[0], F_SETFD, FD_CLOEXEC);
			fcntl(piped[1], F_SETFD, FD_CLOEXEC);
		}

		// the environment is applied in a child of our own before the exec, so the parent's is never touched
		pid_t child = fork();
		if (child < 0)
		{
			error = std::strerror(errno);
			close(report[0]);
			close(report[1]);
			if (input)
			{
				close(piped[0]);
				close(piped[1]);
			}
			return -1;
		}
		if (child == 0)
		{
			close(report[0]);
			Become(command, argv, environment.data(), report[1], piped[0], detached);
		}
		close(report[1]);
		if (input)
		{
			close(piped[0]);
			*input = piped[1];
		}
		starting.unlock(); // the child's end of the pipe is closed here, so no other child can inherit it
		int childError = 0;
		auto got = read(report[0], &childError, sizeof(childError));
		close(report[0]);
		if (got == sizeof(childError))
		{
			if (input) close(*input);
			waitpid(child, nullptr, 0);
			error = childError == ENOENT ? "'" + command.Arguments[0] + "' could not be found" : std::strerror(childError);
			return -1;
		}
		return child;
	}
} // namespace

namespace
{
	// Waits for a started child to exit, stopping it (and its process group) after timeout (zero: no limit); status is its wait status
	Platform::Outcome WaitFor(pid_t child, std::chrono::seconds timeout, int &status)
	{
		using Platform::Outcome;
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (true)
		{
			pid_t done = waitpid(child, &status, timeout.count() > 0 ? WNOHANG : 0);
			if (done == child) break;
			if (done < 0 && errno != EINTR) return {Outcome::Status::CouldNotStart, -1, std::strerror(errno)};
			if (timeout.count() > 0 && std::chrono::steady_clock::now() >= deadline)
			{
				kill(-child, SIGKILL); // the whole group: anything the program itself started stops too
				waitpid(child, &status, 0);
				return {Outcome::Status::TimedOut, -1, ""};
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		if (WIFEXITED(status)) return {Outcome::Status::Exited, WEXITSTATUS(status), ""};
		return {Outcome::Status::Exited, 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0), ""};
	}
} // namespace

Platform::Outcome Platform::Run(const Command &command)
{
	std::string error;
	pid_t child = Start(command, false, error);
	if (child < 0) return {Outcome::Status::CouldNotStart, -1, error};
	int status = 0;
	return WaitFor(child, command.Timeout, status);
}

std::unique_ptr<Platform::Child> Platform::Child::Start(const Command &command, std::string &error)
{
	// writing to a child which has exited must fail, not kill CorTeX with SIGPIPE
	static std::once_flag ignoring;
	std::call_once(ignoring, [] { signal(SIGPIPE, SIG_IGN); });
	int input = -1;
	pid_t id = ::Start(command, false, error, &input);
	if (id < 0) return nullptr;
	std::unique_ptr<Child> child(new Child());
	child->Id = id;
	child->Input = input;
	return child;
}

Platform::Child::~Child()
{
	if (Input >= 0) close(Input);
	if (!Done)
	{
		kill(-static_cast<pid_t>(Id), SIGKILL);
		waitpid(static_cast<pid_t>(Id), &Status, 0);
	}
}

bool Platform::Child::Finish(std::string_view text)
{
	if (Input < 0) return false;
	bool written = true;
	while (!text.empty())
	{
		auto sent = write(Input, text.data(), text.size());
		if (sent < 0 && errno == EINTR) continue;
		if (sent <= 0)
		{
			written = false;
			break;
		}
		text.remove_prefix(static_cast<size_t>(sent));
	}
	close(Input);
	Input = -1;
	return written;
}

Platform::Outcome Platform::Child::Wait(std::chrono::seconds timeout)
{
	if (Input >= 0)
	{
		close(Input); // nothing more is coming: a program waiting for input gets end-of-file, rather than waiting forever
		Input = -1;
	}
	if (Done) return {Outcome::Status::Exited, WIFEXITED(Status) ? WEXITSTATUS(Status) : 128, ""};
	auto outcome = WaitFor(static_cast<pid_t>(Id), timeout, Status);
	Done = true;
	return outcome;
}

bool Platform::Child::Running()
{
	if (Done) return false;
	pid_t done = waitpid(static_cast<pid_t>(Id), &Status, WNOHANG);
	if (done == static_cast<pid_t>(Id)) Done = true;
	return !Done;
}

std::string Platform::Launch(const Command &command)
{
	std::string error;
	pid_t child = Start(command, true, error);
	return child < 0 ? error : "";
}

std::filesystem::path Platform::Executable()
{
#if defined(__APPLE__)
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::string buffer(size, '\0');
	_NSGetExecutablePath(buffer.data(), &size);
	return std::filesystem::weakly_canonical(buffer.c_str());
#else
	return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

#endif
