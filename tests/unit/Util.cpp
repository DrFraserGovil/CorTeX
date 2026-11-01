// Util: the small text and file helpers JSL doesn't have
#include "Util/Files.h"
#include "Util/Text.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Json escapes for a JS string literal", "[util]")
{
	CHECK(Util::Json("plain") == "\"plain\"");
	CHECK(Util::Json("a\"b\\c\nd\te") == "\"a\\\"b\\\\c\\nd\\te\"");
	CHECK(Util::Json(std::string("\x01", 1)) == "\"\\u0001\"");
}

TEST_CASE("Base64 matches the standard encoding", "[util]")
{
	CHECK(Util::Base64("") == "");
	CHECK(Util::Base64("f") == "Zg==");
	CHECK(Util::Base64("fo") == "Zm8=");
	CHECK(Util::Base64("foo") == "Zm9v");
	CHECK(Util::Base64("foobar") == "Zm9vYmFy");
}

TEST_CASE("Hash is stable across builds", "[util]")
{
	CHECK(Util::HashHex("") == "cbf29ce484222325");
	CHECK(Util::HashHex("a") == "af63dc4c8601ec8c");
}

TEST_CASE("Sanitise and PercentDecode", "[util]")
{
	CHECK(Util::Sanitise("Hello, World!", "-_", true) == "hello--world-");
	CHECK(Util::PercentDecode("a%20b%2Fc") == "a b/c");
	CHECK(Util::PercentDecode("100%") == "100%");
	CHECK(Util::PercentDecode("%zz") == "%zz");
}

TEST_CASE("IsInside", "[util]")
{
	CHECK(Util::IsInside("/a/b/c", "/a/b"));
	CHECK_FALSE(Util::IsInside("/a/b", "/a/b"));
	CHECK_FALSE(Util::IsInside("/a/b/../c", "/a/b"));
	CHECK_FALSE(Util::IsInside("/a/bc", "/a/b"));
}

TEST_CASE("Image types", "[util]")
{
	CHECK(Util::ImageMediaType("x.PNG") == "image/png");
	CHECK(Util::ImageMediaType("x.svg") == "image/svg+xml");
	CHECK_FALSE(Util::ImageMediaType("x.pdf"));
}
