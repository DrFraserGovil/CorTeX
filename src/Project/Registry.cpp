#include "Registry.h"
#include <JSL.h>

Project::SidebarEntry Project::SidebarEntry::Parse(std::string_view entry)
{
	auto bar = entry.find('|');
	return {JSL::String::trim(entry.substr(0, bar)), bar == std::string_view::npos ? "" : JSL::String::trim(entry.substr(bar + 1))};
}

Project::Registry Project::Registry::Read(const std::filesystem::path &root)
{
	Registry registry{root.filename().string(), {}};
	auto file = root / ".cortex/registry.ctx";
	if (!std::filesystem::exists(file)) return registry;

	bool inSidebar = false;
	for (auto &raw : JSL::IO::getFileLines(file))
	{
		auto line = JSL::String::trim(raw);
		if (!inSidebar)
		{
			if (JSL::String::iEquals(line, "sidebar:"))
			{
				inSidebar = true;
				continue;
			}
			auto eq = line.find('=');
			if (eq != std::string::npos && JSL::String::trim(std::string_view(line).substr(0, eq)) == "name")
			{
				registry.Name = JSL::String::trim(std::string_view(line).substr(eq + 1));
			}
			continue;
		}
		if (line.empty() || line.starts_with("%")) continue;
		if (line.starts_with("[") && line.ends_with("]"))
		{
			registry.Sidebar.push_back({JSL::String::trim(std::string_view(line).substr(1, line.size() - 2)), {}});
			continue;
		}
		if (registry.Sidebar.empty()) registry.Sidebar.push_back({"", {}}); // entries before any heading form an untitled first group
		registry.Sidebar.back().Entries.push_back(SidebarEntry::Parse(line));
	}
	return registry;
}
