#pragma once
#include <string>
#include <filesystem>
class Metadata
{
    public:
        bool ExistsOnDisk;
        std::string Name;
        std::string Author;
        Metadata();
        Metadata(std::filesystem::path path);
        void Load(std::filesystem::path path);
        void Save(std::filesystem::path path);
    private:
        void SetDefaults();
        
};