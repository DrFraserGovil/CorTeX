#include "validate.h"
#include "../global.h"



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
    log.DebugBoxing = false;
    // log.DebugLint = 20;
    log.DebugLineSize = Settings.System.VeryVerboseWidth;
    log.SetLevel(INFO);
    if (Settings.System.Quiet)
    {
        log.SetLevel(ERROR);
    }
    if (Settings.System.Verbose)
    {
        log.SetLevel(DEBUG);
    }
    if (Settings.System.VeryVerbose)
    {
        log.SetLevel(DEBUG);
        log.DebugBoxing = true;
    }
    log.DebugColour= Cortex.Colours.DebugDefault;
    log.ForceClear = true;    
}


inline bool equal(std::string_view a, std::vector<std::string_view> b)
{
    for (auto & elem : b)
    {
        bool test = JSL::insensitiveEquals(a,elem);
        if (test) return true;
    }
    return false;
}

using vsv = std::vector<std::string_view>;
#define MATCH(cmd,arg...) if(equal(compilerSettings.CompilerCommand,(vsv){arg})){compilerSettings.CompilerCommand = cmd; return;}

void SetCompiler(SettingsObject_Compiler & compilerSettings)
{
    MATCH("pdflatex","pdflatex","pdf");
    MATCH("xelatex","xelatex","xe","x");
    MATCH("lualatex","lualatex","lua","l");

    LOG(WARN) << "Unrecognised compiler command '" << compilerSettings.CompilerCommand << "'.\nDefaulting to pdflatex";
    compilerSettings.CompilerCommand = "pdflatex";
}

bool SetProtected(std::vector<std::string> custom)
{
    auto & env =  Cortex.Values.ProtectedEnvironments;
    auto cached = env; //make a copy

    env = {"equation","align","alignat","verbatim","inline-math","display-math","comment"}; //these are hardcoded defaults
    env.insert(env.end(),custom.begin(),custom.end());
    
    int N = env.size();
    for (int i = 0; i < N; ++i)
    {
        auto & test = env[i];
        if (!test.empty() && *(test.end()-1) != '*')
        {
            bool starredNeeded = (std::find(env.begin(),env.end(),test+"*")==env.end());

            if (starredNeeded)
            {
                env.push_back(test+"*");
            }
        }
    }
    LOG(DEBUG) << "The following environments are protected: " << JSL::MakeString(env);
    return env!=cached;
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
    SetCompiler(Settings.Compiler);
    bool changedEnvironments = SetProtected(Settings.Compiler.CustomProtectedEnvironments);
    if (!firstLoop)
    
    {
        if (CachedSettings.Document != Settings.Document || CachedSettings.Compiler != Settings.Compiler || changedEnvironments)
        {   
            LOG(DEBUG) << "Detected a change in compiler settings\nTriggering recompile";
            return true;
        }
    }
    firstLoop = false;
    CachedSettings = Settings;

    return false;
}
