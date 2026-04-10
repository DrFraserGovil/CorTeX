#pragma once
#include "../constants.h"
#include <sys/inotify.h>
#include "../index/directory.h"
struct FileReport
{
    fs::path Path;
    uint32_t Mask;

    bool IsImportant;

    FileReport(std::weak_ptr<Directory> dir, inotify_event * event);
    std::strong_ordering operator<=>(const FileReport& other) const;
};