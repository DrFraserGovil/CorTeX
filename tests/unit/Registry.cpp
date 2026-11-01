// Project::Registry: the name and the global sidebar
#include "Project/Registry.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("The fixture's registry", "[registry]")
{
	auto registry = Project::Registry::Read(CORTEX_FIXTURE);
	CHECK(registry.Name == "Fixture");
	REQUIRE(registry.Sidebar.size() == 2);
	CHECK(registry.Sidebar[0].Heading == "Start");
	REQUIRE(registry.Sidebar[0].Entries.size() == 2);
	CHECK(registry.Sidebar[0].Entries[1].Text == "Euler");
	CHECK(registry.Sidebar[0].Entries[1].Label == "Euler (label)");
	CHECK(registry.Sidebar[1].Entries[0].Label.empty());
}

TEST_CASE("Without a registry, the folder names the project", "[registry]")
{
	auto registry = Project::Registry::Read("/cortex/no/such/project");
	CHECK(registry.Name == "project");
	CHECK(registry.Sidebar.empty());
}
