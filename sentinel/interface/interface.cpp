#include "interface.h"
#include "../settings/settings.hpp"
#include "JSL/modules/Display/Log.h"
#include <filesystem>
#include "../constants.h"


Interface::Interface(Metadata cache) : MetaCache(cache) 
{
    Headless = Settings.System.Headless.Active;
    if (!cache.ExistsOnDisk)
    {
        ConfigureLocation();
    }
    InterfaceLoop();
}

void Interface::InterfaceLoop()
{
     //do things
}




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

    MetaCache.Name = GetAnswer("Project Name",Settings.System.Headless.CortexName,Headless);
    MetaCache.Author = GetAnswer("Project Author",Settings.System.Headless.AuthorName,Headless);

    fs::path metalocation = (fs::path)Settings.Files.TargetDirectory / metadataLocation;
    MetaCache.Save(metalocation);
}
