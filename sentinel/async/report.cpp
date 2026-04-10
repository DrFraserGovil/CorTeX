#include "report.h"
#include "../settings/settings.hpp"
#include "JSL/modules/Strings/Strings.h"
#include "../global.h"

FileReport::FileReport(std::weak_ptr<Directory> dir, inotify_event *event)
{
    Path = fs::relative(dir.lock()->Path.Source / event->name,Cortex.Settings.Files.TargetDirectory);
    // Mask = event->mask;
    // std::string stem = Path.stem();
    // IsTerminationSequence = JSL::insensitiveEquals(stem, Cortex.Settings.System.TerminationFileName);
}

std::strong_ordering FileReport::operator<=>(const FileReport &other) const
{
    return Path <=> other.Path;
}
