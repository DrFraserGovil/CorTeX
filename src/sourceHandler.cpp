#include "sourceHandler.h"

void SourceHandler::Initialise(std::set<fs::path> directories)
{
    SourceFiles.clear();
    for (auto dir: directories)
    {
        auto files = JSL::listFiles(dir);
        for (auto file : files)
        {
            if (!is_directory(file.Path) && glob(file.Path,Settings.Files.WatchedPatterns) &&!glob(file.Path,Settings.Files.IgnoredPatterns))
            {
                auto path = fs::relative(file.Path,Settings.Files.TargetDirectory);
                LOG(DEBUG) << "File " << path << " detected";
                SourceFiles.insert(path);
            }
        }
    }
}

std::set<fs::path> SourceHandler::ListFiles()
{
    return SourceFiles;
}

bool SourceHandler::Contains(const fs::path &path)
{
    return SourceFiles.contains(path);
}
