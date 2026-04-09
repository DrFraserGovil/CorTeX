#include <unistd.h>
#include "project.h"
#include "../global.h"
namespace log = JSL::Log;


void WelcomeMessage()
{
    auto col = JSL::Text::Cyan;
    LOG(INFO) << col <<  "\n        -------- CorTeX Sentinel --------";
    LOG(INFO) << col <<  "Welcome to CorTeX: The LaTeX-graph knowledge system!\n";
}


void LoadSettings()
{
    if (fs::exists(Global.SettingsFile))
    {
        LOG(DEBUG) << "Loading settings from file";
        auto system = Settings.System;
        Settings.Configure(Global.SettingsFile," ");
        Settings.System = system; //system settings are per-instance and shouldn't be cached!
    }
    else
    {
        LOG(DEBUG) << "No save file detected, using those given";
        Settings.SaveConfig(Global.SettingsFile);
    }
}

void CheckHeadless()
{
    //check if the user is in a headless state but did not inform us
    if (!Settings.System.Headless.Active)
    {
        bool isatty = JSL::PipedInputFound(); //platform specific wrapper for isatty(cin)

        //maybe a bit linux specific here
        pid_t fg_pgrp = tcgetpgrp(STDIN_FILENO);
        bool isbackground = (fg_pgrp != -1 && fg_pgrp != getpgrp());

        bool forceHeadless =  isatty || isbackground;
        if (forceHeadless)
        {
            LOG(WARN) << "Automatically switching system to --headless mode";
            Settings.System.Headless.Active = true;
        }
    }
}


////////////////// Linkage function

void Project::Initialise(int argc, char ** argv)
{
    Settings.Parse(argc,argv);    
    Global.Synchronise();
    WelcomeMessage();
    CheckHeadless();
    Info.Initialise();
    LoadSettings();

    Index.Initialise();
}