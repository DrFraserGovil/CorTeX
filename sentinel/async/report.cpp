#include "report.h"
#include "../settings/settings.hpp"
#include "JSL/modules/Strings/Strings.h"

FileReport::FileReport(fs::path path, inotify_event *event)
{
    Path = fs::relative(path / event->name,Settings.Files.TargetDirectory);
    Mask = event->mask;
    std::string stem = Path.stem();
    IsTerminationSequence = JSL::insensitiveEquals(stem, Settings.System.TerminationFileName);
}

bool FileReport::operator<(const FileReport &other) const
{
    return Path < other.Path;
}
