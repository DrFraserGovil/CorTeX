#include "compiled.h"
#include <unordered_map>


void CompiledOutput::ForceReconstruct(std::set<std::filesystem::path> sourcePaths, std::set<std::filesystem::path> directories)
{
    ConstructFromSource(directories);
    DetectOutdated(sourcePaths);
}

void CompiledOutput::ConstructFromSource(std::set<std::filesystem::path> sourcePaths)
{
    // Create a list of all directories that *should* exist based on the source code
    // creates them if needs be
    OutputDirectories.clear();
    fs::path root = Settings.Files.TargetDirectory;
    BuildPath = root / Settings.Files.OutputDirectory;
    for (auto & path : sourcePaths)
    {
        auto relpath = std::filesystem::relative(path,root);
        auto tmp = BuildPath / relpath;
        if (!std::filesystem::exists(tmp))
        {
            std::filesystem::create_directories(tmp);
        }
        OutputDirectories.insert(relpath);
    }
}

struct FileModIndicator
{
    fs::path Path;
    fs::file_time_type LastModified;
    bool NeedsRewrite = true;
    FileModIndicator(){};
    FileModIndicator(std::string path,fs::file_time_type time): Path(path), LastModified(time){};
};

void CompiledOutput::DetectOutdated(std::set<std::filesystem::path> filePaths)
{
    //Steps through the output directory and finds all pdf files that do exist-- so we can compare them to those that *should*
    
    //first need to build a registry of the times the source files were updated
    std::unordered_map<std::string,FileModIndicator> registry;
    for (auto path : filePaths)
    {
        auto fullPath = Settings.Files.TargetDirectory / path;
        std::string stem = path.replace_extension(""); // strip extension, but leave the file structure
        registry[stem] = FileModIndicator(stem,fs::last_write_time(fullPath));
    }
    
    std::set<fs::path> locatedFiles;

    for (auto dir: OutputDirectories)
    {
        auto files = JSL::listFiles(BuildPath/dir);
        for (auto file : files)
        {
            if (!is_directory(file.Path))
            {
                LOG(INFO) << file.Path;
                std::string path = fs::relative(file.Path,Settings.Files.OutputDirectory).replace_extension("");

                if (registry.contains(path))
                {
                    //found a matching file
                    auto time = fs::last_write_time(file.Path);
                    if (time > registry[path].LastModified)
                    {
                        registry[path].NeedsRewrite = false;
                    }
                }
                else
                {
                    //no matching source file found, so file gets deleted
                    LOG(WARN) << "The file " << file.Path << " has no corresponding source file. It is being deleted";
                    fs::remove(file.Path);
                }
            }
        }
    }

    OutdatedFiles.clear();
    std::stringstream s;
    for (auto path : registry)
    {
        if (path.second.NeedsRewrite)
        {
            OutdatedFiles.insert(path.first);
            s << "\n" <<path.first;
        }
    }
    if (OutdatedFiles.size() > 0)
    {
        LOG(DEBUG) << "Files are out-of-sync with source on foamtex startup:" << s.str();
    }
}