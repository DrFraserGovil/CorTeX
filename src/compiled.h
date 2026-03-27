#pragma once

#include <filesystem>
#include <set>
#include "network.h"



class CompiledOutput
{
    public:
        CompiledOutput(){};


        void ForceReconstruct(std::set<std::filesystem::path> sourcePaths,std::set<std::filesystem::path> directories);

        std::set<fs::path> OutdatedFiles;
    private:
        std::set<std::filesystem::path> OutputDirectories;

        void ConstructFromSource(std::set<std::filesystem::path> directories);

        void DetectOutdated(std::set<std::filesystem::path> sourcePaths);

        fs::path BuildPath;
};          