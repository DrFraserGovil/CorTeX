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
        void Initialise();
        void Save();
    private:
        void Load();
        void SetDefaults();
        
};