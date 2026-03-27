#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>


#include "globalSettings.h"
#include "foamtex.h"

SettingsObject Settings; //defines the global object that's extern in the header

void ConfigureOutput()
{
    JSL::Log::Config.SetLevel(INFO);
    if (Settings.System.Quiet)
    {
        JSL::Log::Config.SetLevel(ERROR);
    }
    if (Settings.System.Verbose)
    {
        JSL::Log::Config.SetLevel(DEBUG);
    }
}

void ValidateSettings()
{
    ConfigureOutput();

    //ensure the output directory is included in the ignored pattern, to prevent hellish event recursions
    auto & files = Settings.Files;
    auto & tmp = files.IgnoredPatterns;
    std::string ignore =  (fs::path)(files.OutputDirectory);
    if (std::find(tmp.begin(),tmp.end(),ignore)==tmp.end())
    {
        tmp.push_back(ignore+ "*");
        LOG(DEBUG) << "Updated ignored patterns to " << JSL::MakeString(Settings.Files.IgnoredPatterns);
    }
}

int main(int argc, char**argv)
{
    Settings.Parse(argc,argv);
    ValidateSettings();

    LOG(INFO) << "Launching foamtex on " << Settings.Files.TargetDirectory;
    auto Foam = FoamTexArchive();
   
    // auto q = SystemWatcher(".");

    // LOG(INFO) << "Foamtex Sentinel Activating...";
    // q.Listen();
    // LOG(INFO) << "Foamtex exiting";
}
