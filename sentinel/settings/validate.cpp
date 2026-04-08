#include "validate.h"
#include "../constants.h"
#include "JSL/modules/Display/Log.h"
#include "../project/index.h"

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



bool ValidateSettings()
{
    ConfigureLogging();


    //ensure the output directory is included in the ignored pattern, to prevent hellish event recursions
    auto & files = Settings.Files;    
    auto & tmp = files.IgnoredPatterns;
    std::string ignore =  (fs::path)(files.OutputDirectory);
    ignore = "*" + ignore + "*";
    if (std::find(tmp.begin(),tmp.end(),ignore)==tmp.end())
    {
        tmp.push_back(ignore);
        LOG(DEBUG) << "Updated ignored patterns to " << JSL::MakeString(Settings.Files.IgnoredPatterns);
    }

     FixFontSize("body text",Settings.Document.FontSize);
   

    if (CachedCompileSettings != Settings.Document)
    {   
        CachedCompileSettings = Settings.Document;
        LOG(DEBUG) << "Detected a change in compiler settings - triggering recompile";
        return true;
    }

    return false;
}

void ConfigureLogging()
{
    auto & log = JSL::Log::Config;
    log.SetLevel(INFO);
    if (Settings.System.Quiet)
    {
        log.SetLevel(ERROR);
    }
    if (Settings.System.Verbose)
    {
        log.SetLevel(DEBUG);
    }
    log.WarnColour = JSL::Text::Red;
    log.DebugColour= JSL::Text::Colour(80,80,60);
    
}

SettingsObject_Document CachedCompileSettings;