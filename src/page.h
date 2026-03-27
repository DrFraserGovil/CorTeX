#pragma once
#include "globalSettings.h"
#include <filesystem>
#include <vector>
#include <set>
#include <unordered_map>
#include "utils.h"
class Page
{
    public:
        std::filesystem::path Path;
        std::string Name;
        std::set<Page *> OutgoingConnections;
        std::set<Page *> IncomingConnections;
        Page(std::filesystem::path path);

        void ReadFile();

        void MarkDirty();
    private:
        bool IsDirty();
};

  