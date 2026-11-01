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
