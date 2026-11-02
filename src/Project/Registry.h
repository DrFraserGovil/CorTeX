#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

//! .cortex/registry.config (once registry.ctx, which is renamed when found): the project's name ("name = ..."), then after a "sidebar:" line the global sidebar (DESIGN.md §8.6), as
//! [Heading] lines and entries written like link targets ("target" or "target|Label")
namespace Project
{
	struct SidebarEntry
	{
		//! The target as written
		std::string Text;
		//! The label to show instead of the target's title; empty for the title
		std::string Label;

		//! Parses "target" or "target|Label"
		static SidebarEntry Parse(std::string_view entry);
	};

	struct SidebarGroup
	{
		std::string Heading;
		std::vector<SidebarEntry> Entries;
	};

	struct Registry
	{
		//! The project's name: as registered, else the root folder's name
		std::string Name;
		std::vector<SidebarGroup> Sidebar;

		static Registry Read(const std::filesystem::path &root);
	};
} // namespace Project
