#pragma once
#include "network.h"
#include <set>
#include <filesystem>
namespace fs = std::filesystem;
class SourceHandler
{
    public:
        void Initialise(std::set<fs::path> directories);
        std::set<fs::path> ListFiles();
        bool Contains(const fs::path & path);
    private:
        std::set<fs::path> SourceFiles; // a list of all source files **relative to the root path**
        WikiNetwork Network;
};