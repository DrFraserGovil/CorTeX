#include "Warm.h"
#include "Util/Files.h"
#include <JSL.h>
#include <algorithm>

Tex::WarmPool::WarmPool() : LastUsed(std::chrono::steady_clock::now()), Reaper([this] { Reap(); })
{
}

Tex::WarmPool::~WarmPool()
{
	{
		std::lock_guard lock(Guard);
		Stopping = true;
		StopAll();
		std::error_code ignored;
		if (!Current.Folder.empty()) std::filesystem::remove_all(Current.Folder, ignored);
	}
	Wake.notify_all();
	Reaper.join();
}

std::string Tex::WarmPool::WaitLine()
{
	// \endlinechar=-1 keeps the line's end out of the name; reading the terminal needs scroll mode, which each page switches out of
	return "\\endlinechar=-1 \\read16 to \\cortexwarmpage \\endlinechar=13 \\input{\\cortexwarmpage}";
}

std::filesystem::path Tex::WarmPool::DriverFile() const
{
	return Current.Folder / ("cortexwarm-" + std::to_string(Generation) + ".tex");
}

void Tex::WarmPool::Prepare(const Spec &spec, size_t size)
{
	std::lock_guard lock(Guard);
	LastUsed = std::chrono::steady_clock::now();
	if (!(spec == Current) || Generation == 0)
	{
		StopAll();
		// everything an earlier generation left (its driver, files of workers stopped while waiting) goes with it
		if (!Current.Folder.empty()) std::filesystem::remove_all(Current.Folder);
		Current = spec;
		++Generation;
		Started = 0;
		Broken = false;
		if (Recent.empty() || !Util::IsInside(Recent, Current.Root)) Recent = Current.Root;
		std::filesystem::remove_all(Current.Folder);
		std::filesystem::create_directories(Current.Folder);
		JSL::IO::writeString(DriverFile(), Current.Driver);
	}
	Size = size;
	while (Waiting.size() > Size) Waiting.pop_back();
	TopUp();
}

std::optional<Tex::WarmPool::Worker> Tex::WarmPool::StartOne(const std::filesystem::path &in)
{
	if (Broken || Size == 0 || Current.Folder.empty() || !std::filesystem::is_directory(in)) return std::nullopt;
	Worker worker;
	worker.Name = "cortexwarm-" + std::to_string(Generation) + "-" + std::to_string(++Started);
	worker.Folder = Current.Folder;
	worker.In = in;
	Platform::Command command;
	command.Arguments = Current.Arguments;
	// scroll mode, so that it may read the page's name from its input (TeX won't in nonstop mode); the page switches to nonstop mode
	command.Arguments.push_back("-interaction=scrollmode");
	command.Arguments.push_back("-jobname=" + worker.Name);
	command.Arguments.push_back("-output-directory=" + Current.Folder.string());
	command.Arguments.push_back(DriverFile().string());
	command.WorkingDirectory = in; // where a fresh XeLaTeX for a note there would run
	command.Environment = Current.Environment;
	command.OutputFile = Current.Folder / (worker.Name + ".stdout");
	std::string error;
	worker.Run = Platform::Child::Start(command, error);
	if (!worker.Run)
	{
		Broken = true; // it won't start next time either
		return std::nullopt;
	}
	return worker;
}

void Tex::WarmPool::TopUp()
{
	// every waiting worker in the recent folder: those elsewhere (the oldest first) make way
	auto here = [&] { return static_cast<size_t>(std::count_if(Waiting.begin(), Waiting.end(), [&](auto &w) { return w.In == Recent; })); };
	while (here() < Size)
	{
		if (Waiting.size() >= Size)
		{
			auto elsewhere = std::find_if(Waiting.begin(), Waiting.end(), [&](auto &w) { return w.In != Recent; });
			Discard(*elsewhere);
			Waiting.erase(elsewhere);
		}
		auto worker = StartOne(Recent);
		if (!worker) return;
		Waiting.push_back(std::move(*worker));
	}
}

void Tex::WarmPool::Replace(const std::filesystem::path &folder)
{
	// one worker in place of the one taken, waiting where it was needed; if that makes too many, the oldest elsewhere makes way
	auto worker = StartOne(folder);
	if (!worker) return;
	Waiting.push_back(std::move(*worker));
	if (Waiting.size() <= Size) return;
	auto elsewhere = std::find_if(Waiting.begin(), Waiting.end(), [&](auto &w) { return w.In != folder; });
	if (elsewhere == Waiting.end()) elsewhere = Waiting.begin();
	Discard(*elsewhere);
	Waiting.erase(elsewhere);
}

void Tex::WarmPool::Discard(Worker &worker)
{
	worker.Run.reset(); // stopped as it is destroyed; then what it wrote while waiting (its log, its .fls) goes too
	for (auto extension : {".log", ".fls", ".stdout", ".aux"}) std::filesystem::remove(worker.Folder / (worker.Name + extension));
}

void Tex::WarmPool::StopAll()
{
	for (auto &worker : Waiting) Discard(worker);
	Waiting.clear();
}

std::optional<Tex::WarmPool::Worker> Tex::WarmPool::Take(const std::filesystem::path &folder)
{
	std::lock_guard lock(Guard);
	LastUsed = std::chrono::steady_clock::now();
	if (Broken) return std::nullopt;
	Recent = folder;
	auto found = std::find_if(Waiting.begin(), Waiting.end(), [&](auto &w) { return w.In == folder; });
	if (found != Waiting.end())
	{
		auto worker = std::move(*found);
		Waiting.erase(found);
		if (worker.Run->Running())
		{
			Replace(folder);
			return worker;
		}
		// a waiting worker never exits by itself unless the preamble stopped it (an error, or something which won't wait like this)
		LOG(INFO) << "A warm XeLaTeX exited before it was used (see " << (Current.Folder / (worker.Name + ".log")).string()
				  << "); compiling without warm workers until the preamble or settings change";
		Broken = true;
		StopAll();
		return std::nullopt;
	}
	auto worker = StartOne(folder);
	if (worker) Replace(folder);
	return worker;
}

void Tex::WarmPool::Reap()
{
	std::unique_lock lock(Guard);
	while (!Stopping)
	{
		Wake.wait_for(lock, IdleTime / 4);
		if (!Stopping && !Waiting.empty() && std::chrono::steady_clock::now() - LastUsed >= IdleTime) StopAll();
	}
}
