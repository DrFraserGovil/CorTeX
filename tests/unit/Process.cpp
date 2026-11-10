// Platform::Process: programs run without a shell, in a folder, with extra environment, output to a file, an exit code and a timeout
#include "Platform/Process.h"
#include <JSL.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>

namespace
{
	std::filesystem::path Scratch(const std::string &name)
	{
		auto folder = std::filesystem::path(CORTEX_TEST_SCRATCH) / "process" / name;
		std::filesystem::remove_all(folder);
		std::filesystem::create_directories(folder);
		return folder;
	}
} // namespace

TEST_CASE("Run reports the exit code", "[process]")
{
	CHECK(Platform::Run({.Arguments = {"true"}}).Succeeded());
	auto failed = Platform::Run({.Arguments = {"sh", "-c", "exit 3"}});
	CHECK(failed.State == Platform::Outcome::Status::Exited);
	CHECK(failed.ExitCode == 3);
}

TEST_CASE("Arguments reach the program untouched, with no shell between", "[process]")
{
	auto folder = Scratch("arguments");
	auto log = folder / "out.txt";
	std::string awkward = "it's \"quoted\" $HOME `x` ; a b";
	REQUIRE(Platform::Run({.Arguments = {"printf", "%s", awkward}, .OutputFile = log}).Succeeded());
	CHECK(JSL::IO::getFile(log) == awkward);
}

TEST_CASE("The working directory, environment and output file are applied", "[process]")
{
	auto folder = Scratch("context");
	auto log = folder / "out.txt";
	REQUIRE(Platform::Run({.Arguments = {"sh", "-c", "pwd; echo $CORTEX_TEST_VALUE; echo oops >&2"},
						   .WorkingDirectory = folder,
						   .Environment = {{"CORTEX_TEST_VALUE", "hello there"}},
						   .OutputFile = log})
				.Succeeded());
	auto text = JSL::IO::getFile(log);
	CHECK(text == std::filesystem::canonical(folder).string() + "\nhello there\noops\n");
	CHECK(std::getenv("CORTEX_TEST_VALUE") == nullptr); // set only for the child
}

TEST_CASE("A program that can't be found is reported, not run", "[process]")
{
	auto outcome = Platform::Run({.Arguments = {"cortex-no-such-program"}});
	CHECK(outcome.State == Platform::Outcome::Status::CouldNotStart);
	CHECK(outcome.Error.find("could not be found") != std::string::npos);
	CHECK_FALSE(Platform::Run({.Arguments = {"true"}, .WorkingDirectory = "/cortex/no/such/folder"}).Succeeded());
	CHECK(Platform::Run({}).State == Platform::Outcome::Status::CouldNotStart);
}

TEST_CASE("A program past its timeout is stopped, with everything it started", "[process]")
{
	auto folder = Scratch("timeout");
	auto marker = folder / "late";
	auto start = std::chrono::steady_clock::now();
	auto outcome = Platform::Run({.Arguments = {"sh", "-c", "(sleep 2; touch late) & sleep 30"}, .WorkingDirectory = folder, .Timeout = std::chrono::seconds(1)});
	CHECK(outcome.State == Platform::Outcome::Status::TimedOut);
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
	std::this_thread::sleep_for(std::chrono::milliseconds(2500));
	CHECK_FALSE(std::filesystem::exists(marker)); // the background child was stopped too
}

TEST_CASE("Launch leaves the program running on its own", "[process]")
{
	auto folder = Scratch("launch");
	REQUIRE(Platform::Launch({.Arguments = {"sh", "-c", "sleep 0.2; touch done"}, .WorkingDirectory = folder}) == "");
	CHECK(Platform::Launch({.Arguments = {"cortex-no-such-program"}}).find("could not be found") != std::string::npos);
	for (int i = 0; i < 50 && !std::filesystem::exists(folder / "done"); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
	CHECK(std::filesystem::exists(folder / "done"));
}

TEST_CASE("Find looks along PATH", "[process]")
{
	auto sh = Platform::Find("sh");
	REQUIRE(sh);
	CHECK(sh->filename() == "sh");
	CHECK_FALSE(Platform::Find("cortex-no-such-program"));
	CHECK(Platform::Find("/bin/sh"));
	CHECK(std::filesystem::exists(Platform::Executable()));
}

#if !defined(_WIN32)
TEST_CASE("A child reads what it is sent, then end-of-file", "[process]")
{
	auto folder = Scratch("child");
	auto log = folder / "out.txt";
	std::string error;
	// reads one line, then counts what's left: nothing, as the input is closed after it
	auto child = Platform::Child::Start({.Arguments = {"sh", "-c", "read line; echo \"got $line\"; cat | wc -c"}, .OutputFile = log}, error);
	REQUIRE(child);
	CHECK(child->Running());
	CHECK(child->Finish("hello\n"));
	CHECK(child->Wait(std::chrono::seconds(10)).Succeeded());
	CHECK_FALSE(child->Running());
	CHECK(JSL::String::trim(JSL::IO::getFile(log)) == "got hello\n0");
}

TEST_CASE("A child waiting for input ends when the input closes, or is stopped when let go", "[process]")
{
	auto folder = Scratch("child-ends");
	std::string error;
	// nothing sent: waiting for it ends at once, with the end of its input
	auto waiting = Platform::Child::Start({.Arguments = {"sh", "-c", "read line || exit 7"}}, error);
	REQUIRE(waiting);
	auto outcome = waiting->Wait(std::chrono::seconds(10));
	CHECK(outcome.State == Platform::Outcome::Status::Exited);
	CHECK(outcome.ExitCode == 7);

	// destroyed while still waiting: stopped, with what it started
	auto marker = folder / "marker";
	{
		auto held = Platform::Child::Start({.Arguments = {"sh", "-c", "(sleep 2; touch " + marker.string() + ") & read line"}}, error);
		REQUIRE(held);
		CHECK(held->Running());
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(2500));
	CHECK_FALSE(std::filesystem::exists(marker));

	// writing to one which has already exited fails, rather than killing CorTeX
	auto gone = Platform::Child::Start({.Arguments = {"true"}}, error);
	REQUIRE(gone);
	REQUIRE(gone->Wait(std::chrono::seconds(10)).Succeeded());
	CHECK_FALSE(gone->Finish("too late\n"));
}

TEST_CASE("Children started from many threads at once each get their own input", "[process]")
{
	auto folder = Scratch("child-threads");
	std::vector<std::thread> threads;
	std::vector<int> ok(16, 0);
	for (int i = 0; i < 16; ++i)
	{
		threads.emplace_back([&, i] {
			std::string error;
			auto out = folder / (std::to_string(i) + ".txt");
			auto child = Platform::Child::Start({.Arguments = {"sh", "-c", "read line; echo $line"}, .OutputFile = out}, error);
			if (!child || !child->Finish(std::to_string(i) + "\n") || !child->Wait(std::chrono::seconds(20)).Succeeded()) return;
			ok[i] = JSL::String::trim(JSL::IO::getFile(out)) == std::to_string(i);
		});
	}
	for (auto &thread : threads) thread.join();
	for (int i = 0; i < 16; ++i) CHECK(ok[i] == 1);
}
#endif
