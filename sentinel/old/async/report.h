#pragma once
#include "../constants.h"
#include <sys/inotify.h>

struct FileReport
{
    fs::path Path;
    uint32_t Mask;
    bool IsTerminationSequence;

    FileReport( fs::path path, inotify_event * event);
    bool operator<(const FileReport& other) const;
};