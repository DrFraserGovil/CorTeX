#include "initialiser.h"
#include "../settings/validate.h" //also include settings
#include "../constants.h"
#include "../project/metadata.h"
namespace log = JSL::Log;

Initialiser::Initialiser(int argc,char**argv)
{
    Settings.Parse(argc,argv);
    ValidateSettings();
    WelcomeMessage();
    AssignMetadata();
}
void Initialiser::WelcomeMessage()
{
    if (log::Config.Level >= INFO)
    {
        log::Config.ShowHeaders = false;
        auto col = txt::Cyan;
        LOG(INFO) << col <<  "\n        -------- CorTeX Sentinel --------";
        LOG(INFO) << col <<  "Welcome to CorTeX: The LaTeX-graph knowledge system!\n";
        log::Config.ShowHeaders = true;
    }
}


void Initialiser::AssignMetadata()
{
    fs::path expected = (fs::path)Settings.Files.TargetDirectory / ".cortex";
    log::Config.ShowHeaders = false;
    if (fs::exists(expected))
    {
        auto metafile = (fs::path)Settings.Files.TargetDirectory /metadataLocation;
        if (fs::exists(metafile))
        {
            MainProject.Load(metafile);
            LOG(INFO) << txt::Italics << MainProject.Name << txt::Reset  << " cortex loaded";
        }
        else
        {
            LOG(WARN) << "A cortex directory was found, but the metadata is missing\nA new cortex metadata will be initialised.\n" << txt::Bold << "Existing notes will not be affected.";
            MainProject.ExistsOnDisk = false;
        }
    }
    else
    {
        fs::create_directories(expected);
        MainProject.ExistsOnDisk = false;
    }

    LoadSettings(); //performs its own checks - if not exist, creates it
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
    }
    else
    {
        Settings.SaveConfig((fs::path)Settings.Files.TargetDirectory / settingLocation); //creates a saved instance if necessary
    }
}

