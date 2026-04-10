#include "validate.h"
#include "../constants.h"




void FixFontSize(std::string value, size_t & target)
{
    std::vector<size_t> validSizes = {8,9,10,11,12,14,17,20};
    
    int mindiff = 100000;
    int closest =-1;
    for (int i =0; i < validSizes.size(); ++i)
    {
        if (target==validSizes[i]) return; //the font size is a good one

        size_t diff = std::abs((int)validSizes[i] - (int)target);
        if (diff < mindiff)
        {
            mindiff = diff;
            closest = validSizes[i];
        }
    }

    LOG(WARN) << value << " font size " << target << " is not supported by extsizes, set to " << closest;
    target = closest;

}


void ConfigureLogging(SettingsObject & Settings)
{
    auto & log = JSL::Log::Config;
    log.ShowHeaders = false;
    log.SetLevel(INFO);
    if (Settings.System.Quiet)
    {
        log.SetLevel(ERROR);
    }
    if (Settings.System.Verbose)
    {
        log.SetLevel(DEBUG);
    }
    log.DebugColour= JSL::Text::Colour(80,80,60);
    log.ForceClear = true;
    
}

bool firstLoop = true;
bool ValidateSettings(SettingsObject & Settings,SettingsObject & CachedSettings)
{
    ConfigureLogging(Settings);


    //ensure the output directory is included in the ignored pattern, to prevent hellish event recursions
    auto & files = Settings.Files;    
    auto & tmp = files.IgnoredPatterns;
    std::string ignore =  (fs::path)(files.OutputDirectory);
    ignore = "*" + ignore + "*";
    if (std::find(tmp.begin(),tmp.end(),ignore)==tmp.end())
    {
        tmp.push_back(ignore);
    }

     FixFontSize("body text",Settings.Document.FontSize);
   
    if (!firstLoop)
    {
        if (CachedSettings.Document != Settings.Document)
        {   
            LOG(DEBUG) << "Detected a change in compiler settings\nTriggering recompile";
            return true;
        }
    }
    firstLoop = false;
    CachedSettings = Settings;

    return false;
}
