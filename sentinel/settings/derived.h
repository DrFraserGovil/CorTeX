#pragma once
#include "settings.hpp"

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
        bool Synchronise();

};

extern DerivedSettings Global;