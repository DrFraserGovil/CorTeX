#include "initialiser.h"
#include "../settings/validate.h" //also include settings
#include "../constants.h"

namespace log = JSL::Log;

Initialiser::Initialiser(int argc,char**argv)
{
    Settings.Parse(argc,argv);
    ValidateSettings();

    WelcomeMessage();
}
void Initialiser::WelcomeMessage()
{
    if (log::Config.Level >= INFO)
    {
        log::Config.ShowHeaders = false;
        LOG(INFO) << txt::Yellow <<  "\n        -------- CorTeX Sentinel --------";
        LOG(INFO) << txt::Yellow <<  "Welcome to CorTeX: The LaTeX-graph knowledge system!\n";
        log::Config.ShowHeaders = true;
    }
}


Metadata Initialiser::GetMetadata()
{
    Metadata MetaCache;
    fs::path expected = (fs::path)Settings.Files.TargetDirectory / ".cortex";
    log::Config.ShowHeaders = false;
    if (fs::exists(expected))
    {
        auto metafile = (fs::path)Settings.Files.TargetDirectory /metadataLocation;
        if (fs::exists(metafile))
        {
            MetaCache.Load(metafile);
            LOG(INFO) << txt::Italics << MetaCache.Name << txt::Reset  << " cortex loaded";
        }
        else
        {
            LOG(WARN) << "A cortex directory was found, but the metadata is missing\nA new cortex metadata will be initialised.\n" << txt::Bold << "Existing notes will not be affected.";
            MetaCache.ExistsOnDisk = false;
        }
    }
    else
    {
        fs::create_directories(expected);
        MetaCache.ExistsOnDisk = false;
    }

    LoadSettings(); //performs its own checks - if not exist, creates it
    return MetaCache;
}

void Initialiser::LoadSettings()
{
    fs::path settings = (fs::path)Settings.Files.TargetDirectory / settingLocation;
    if (fs::exists(settings))
    {
        LOG(DEBUG) << "Loading settings from file " << settings;
        auto system = Settings.System;
        Settings.Configure(settings," ");
        Settings.System = system; //system settings are per-instance and shouldn't be cached!
        LOG(DEBUG) << Settings.Files.OutputDirectory;
    }
    else
    {
        Settings.SaveConfig((fs::path)Settings.Files.TargetDirectory / settingLocation); //creates a saved instance if necessary
    }
}

