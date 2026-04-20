#pragma once
#include "settings.hpp"
#include <filesystem>
/*
    The derived settings are values which are inferred automatically from the Settings
    Whenever the settings are updated, Synchronise should be called
*/

class DerivedSettings
{
    public:
        std::filesystem::path SourceRoot;
        std::filesystem::path CompileRoot;
        std::filesystem::path BuildRoot;
        std::filesystem::path MetaRoot;
        std::filesystem::path MetaHeadFile;
        std::filesystem::path SettingsFile;
        std::filesystem::path MacroFile;
        std::filesystem::path ClassFile_User;
        std::filesystem::path ClassFile_Compiler;
        std::filesystem::path SharedSessionDirectory;
        bool Synchronise(SettingsObject & Settings,SettingsObject & CachedSettings);

};
