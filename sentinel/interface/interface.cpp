#include "interface.h"
#include "../settings/settings.hpp"
#include "JSL/modules/Display/Log.h"
#include <filesystem>
#include "../constants.h"
#include <regex>
#include "directory_display.h"
#include "../project/metadata.h"
Interface::Interface() 
{
    Headless = Settings.System.Headless.Active;
    if (!MainProject.ExistsOnDisk)
    {
        ConfigureLocation();
    }
}

void Interface::BeginLoop()
{
    Handler->WaitForInitialisation();
    LOG(INFO) << "Launching interactive mode...";
    std::string cmd;
    bool continues = true;
    while (continues)
    {
        std::cout << txt::Blue << ">> " << txt::Cyan;
        std::getline(std::cin,cmd);
        continues = ParseCommand(JSL::trim(cmd));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Interface::ConnectWorker(Worker & w)
{
    Handler = &w;
}

void NicePrint(std::string_view key, std::string_view value)
{
    int buffer = 10 - key.size();
    buffer = std::max(1,buffer);
    LOG(INFO) << "  " << txt::Green << key << std::string(buffer, ' ') << txt::White << value;
}

bool voicedConcerns = false;

void ShowSettings(std::vector<std::string_view> & cmd)
{
    auto orig = JSL::Log::Config.Level;
    JSL::Log::Config.Level = INFO;
    if (orig < INFO && !voicedConcerns)
    {
        voicedConcerns = true;
        LOG(WARN) << "Calling a display command whilst in quiet mode. Quietness temporarily disabled.";
    }
    if (cmd.size() == 2)
    {
        auto out = Settings.GetDescription((std::string)cmd[1]);
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

const auto & equal = JSL::insensitiveEquals;

bool Interface::ParseCommand(std::string_view cmd)
{
    cmd = JSL::trim(cmd);
    if (equal(cmd,"exit") ||equal(cmd,"shutdown"))
    {
        Handler->AddTask(Task::Shutdown());
        return false;
    }

    auto array = JSL::split(cmd," ");

    bool foundCommand = CommandSearcher(array);
    if (!foundCommand && array.size() > 1)
    {
        std::swap(array[1],array[0]);
        foundCommand = CommandSearcher(array);
    }

    if (!foundCommand)
    {
        LOG(WARN) << "Unknown command";
    }
    return true;
}

bool Interface::CommandSearcher(std::vector<std::string_view> & array)
{
    if (equal(array[0],"show"))
    {
        ShowSettings(array);
        return true;
    }
    if (equal(array[0],"list") || equal(array[0],"ls"))
    {
        directoryDisplay(array);
        return true;
    }

    if (equal(array[0],"set"))
    {
        Handler->AddTask(Task::ParameterChange(array));
        return true;
    }
    if (equal(array[0],"add"))
    {
        auto T = Task::ParameterChange(array);
        T.Type = Instruction::VectorAdd;
        Handler->AddTask(T);
        return true;
    }
    if (equal(array[0],"remove") || equal(array[0],"rm"))
    {
        auto T = Task::ParameterChange(array);
        T.Type = Instruction::VectorRemove;
        Handler->AddTask(T);
        return true;
    }
  
    return false;
};


std::string GetAnswer(std::string prompt, std::string defaultAnswer,bool headless)
{
    if (headless)
    {
        return defaultAnswer;
    }
    std::string answer;
    std::cout << prompt << ":\t";
    std::getline(std::cin, answer);
    
    if (answer.empty())
    {
        std::cout << JSL::Cursor::CursorUp << JSL::Cursor::ClearLine;
        std::cout << prompt << ":\t" << txt::Green << defaultAnswer << "\n" << txt::Reset; 
        answer = defaultAnswer;
    }
    return answer;
}


void Interface::ConfigureLocation()
{
    LOG(INFO) << txt::Blue << "Initialising a new Cortex at " << fs::current_path();

    MainProject.Name = GetAnswer("Project Name",Settings.System.Headless.CortexName,Headless);
    MainProject.Author = GetAnswer("Project Author",Settings.System.Headless.AuthorName,Headless);

    fs::path metalocation = (fs::path)Settings.Files.TargetDirectory / metadataLocation;
    MainProject.Save(metalocation);
}
