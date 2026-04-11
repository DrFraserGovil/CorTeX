#include "worker_functions.h"
#include "../global.h"

namespace txt = JSL::Text;

void NicePrint(std::string_view key, std::string_view value)
{
    int buffer = 10 - key.size();
    buffer = std::max(1,buffer);
    LOG(INFO) << "  " << txt::Green << key << std::string(buffer, ' ') << txt::White << value;
}

bool voicedConcerns = false;

void ShowSetting(std::vector<std::string> & cmd)
{
    auto orig = JSL::Log::Config.Level;
    JSL::Log::Config.Level = INFO;
    if (orig < INFO && !voicedConcerns)
    {
        voicedConcerns = true;
        LOG(WARN) << "Calling a display command whilst in quiet mode. Quietness temporarily disabled.";
    }

    if (cmd.size() == 1)
    {
        auto out = Cortex.Settings.GetDescription(cmd[0]);
        if (out.size() == 0)
        {
            LOG(WARN) << txt::Italics << "No matches found.";
            return;
        }
        if (out.size() > 1)
        {
            LOG(INFO) << txt::Italics << out.size() << " matches found";
        }
        for (auto & setting : out)
        {
            LOG(INFO) << txt::Bold << txt::Green << setting.Name;
            NicePrint("Key",setting.Key);
            auto cleantype = std::regex_replace(setting.TypeString, std::regex("std::"), "");
            NicePrint("Datatype",cleantype);
            if (setting.CurrentValue == setting.DefaultValue)
            {
                NicePrint("Value",setting.CurrentValue + " (default)");
            }
            else
            {
                NicePrint("Value",setting.CurrentValue);
                NicePrint("Default",setting.DefaultValue);
            }
            NicePrint("Notes",setting.Description);
        }
    }
    else
    {
        LOG(WARN) << "Show command accepts a single argument (" << cmd.size() << " provided)";
    }
    JSL::Log::Config.Level = orig;
}


std::pair<bool,JSL::ParameterDescription> CheckParameterData(std::vector<std::string> & data)
{
    std::pair<bool,JSL::ParameterDescription> out(false,JSL::ParameterDescription());
    if (data.size() == 0)
    {
        LOG(WARN) << "Please specify a parameter to modifiy";
        return out;
    }

   
    auto counts = Cortex.Settings.GetDescription(data[0]);
    if (counts.size() == 0)
    {
        LOG(WARN) << "No parameter found matching key '" << data[0] << "'";
        return out;
    }
    if (counts.size() > 1)
    {
        LOG(WARN) << "Multiple parameters found matching that search:";
        for (auto count : counts)
        {
            LOG(WARN) << "  " << count.Name << " (key: " << count.Key << ")";
        }
        LOG(WARN) << "Please use a unique identifier";
        return out;
    }

    const std::vector<std::string> forbiddenValues = {"i","headless","h","config","config-delimiter"};
    if (std::find(forbiddenValues.begin(), forbiddenValues.end(),counts[0].Key)!=forbiddenValues.end())
    {
        LOG(ERROR) << "Cannot mutate parameter '" << counts[0].Name << "' (-" <<counts[0].Key << ") at runtime. \nThis value can only be modified by the CLI";
        return out;
    }

    out.first = true;
    out.second = counts[0];
    return out;
}

bool ChangeSetting(std::vector<std::string> & data)
{
    auto [valid,description] = CheckParameterData(data);
    if (!valid)
    {
        return false;
    }
    data[0] = description.Key; //swap in the key in case the user gave the parameter name
    LOG(DEBUG) << "Processing change to " << description.Key;
    try
    {
        Cortex.Settings.ParseLine(data);
       
       return ConsolidateChanges();
    }
    catch (...)
    {
        LOG(WARN) << "An error was encountered whilst parsing your argument.";
        return false;
    }

}

bool ConsolidateChanges()
{
    bool requiresRecompile = Cortex.Synchronise();
    
    return requiresRecompile;
}


bool Reset(std::vector<std::string> & array)
{
    if (array.size() == 1)
    {
        if (JSL::insensitiveEquals(array[0],"class"))
        {
            auto target = Cortex.Values.ClassFile_User;
            if (fs::exists(target)){fs::remove(target);};

            Cortex.Compiler.CheckResources(true);
            return true;
        }
        if (JSL::insensitiveEquals(array[0],"macro"))
        {
            auto target = Cortex.Values.MacroFile;
            if (fs::exists(target)){fs::remove(target);};

            Cortex.Compiler.CheckResources(true);
            return true;
        }
    }

    LOG(WARN) << "Please specify either 'class' or 'macro' to reset";
    return false;
}

bool Pause(std::vector<std::string> & array)
{
    auto & pause = Cortex.Settings.System.Pause;

    if (array[0] == "0")
    {
        if (pause)
        {
            LOG(INFO) << txt::Italics << txt::Green <<  "Resuming compilation";
            pause = false;
            return true;
        }    
        else
        {
            LOG(WARN) << "Compilation was not paused; resume command ignored";
        }
    }
    else
    {
        if (pause)
        {
            LOG(WARN) << "Compilation is already paused";
        }
        else
        {
            pause=true;
            LOG(INFO) << txt::Italics << txt::Yellow << "Pausing compilation";
        }
    }
    return false;
}