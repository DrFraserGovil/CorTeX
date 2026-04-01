#include "validate.h"
#include "../constants.h"
#include "JSL/modules/Display/Log.h"

void ValidateSettings()
{
    ConfigureLogging();


    //ensure the output directory is included in the ignored pattern, to prevent hellish event recursions
    auto & files = Settings.Files;    
    auto & tmp = files.IgnoredPatterns;
    std::string ignore =  (fs::path)(files.OutputDirectory);
    ignore += "*";
    if (std::find(tmp.begin(),tmp.end(),ignore)==tmp.end())
    {
        tmp.push_back(ignore);
        LOG(DEBUG) << "Updated ignored patterns to " << JSL::MakeString(Settings.Files.IgnoredPatterns);
    }



}

void ConfigureLogging()
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