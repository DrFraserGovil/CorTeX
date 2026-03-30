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

    //strip whitespace form glob-vectors
    for (size_t i = 0; i < Settings.Files.IgnoredPatterns.size(); ++i)
    {
        Settings.Files.IgnoredPatterns[i] = JSL::trim(Settings.Files.IgnoredPatterns[i]);
    }
    for (size_t i = 0; i < Settings.Files.WatchedPatterns.size(); ++i)
    {
        Settings.Files.WatchedPatterns[i] = JSL::trim(Settings.Files.WatchedPatterns[i]);
        
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