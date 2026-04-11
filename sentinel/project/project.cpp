#include <unistd.h>
#include "project.h"
#include "../global.h"
#include "../async/worker.h"
#include "../async/watcher.h"
namespace log = JSL::Log;


void WelcomeMessage()
{
    auto col = JSL::Text::Cyan;
    LOG(INFO) << col <<  "\n        -------- CorTeX Sentinel --------";
    LOG(INFO) << col <<  "Welcome to CorTeX: The LaTeX-graph knowledge system!\n";
}


void LoadSettings()
{
    if (fs::exists(Cortex.Values.SettingsFile))
    {
        LOG(DEBUG) << "Loading settings from file";
        auto system = Cortex.Settings.System;
        Cortex.Settings.Configure(Cortex.Values.SettingsFile," ");
        Cortex.Settings.System = system; //system settings are per-instance and shouldn't be cached!
    }
    else
    {
        LOG(DEBUG) << "No save file detected, using those given";
        Cortex.Settings.SaveConfig(Cortex.Values.SettingsFile);
    }
}

void CheckHeadless()
{
    //check if the user is in a headless state but did not inform us
    if (!Cortex.Settings.System.Headless.Active)
    {
        bool isatty = JSL::PipedInputFound(); //platform specific wrapper for isatty(cin)

        //maybe a bit linux specific here
        pid_t fg_pgrp = tcgetpgrp(STDIN_FILENO);
        bool isbackground = (fg_pgrp != -1 && fg_pgrp != getpgrp());

        bool forceHeadless =  isatty || isbackground;
        if (forceHeadless)
        {
            LOG(WARN) << "Automatically switching system to --headless mode";
            Cortex.Settings.System.Headless.Active = true;
        }
    }
}



std::string GetAnswer(std::string prompt, std::string defaultAnswer,bool headless)
{
    if (headless)
    {
        return defaultAnswer;
    }
    std::string answer;
    std::cout << txt::Yellow << prompt << ":\t" << txt::Cyan;
    std::getline(std::cin, answer);
    
    if (answer.empty())
    {
        std::cout << JSL::Cursor::CursorUp << JSL::Cursor::ClearLine;
        std::cout << prompt << ":\t" << txt::Green << defaultAnswer << "\n" << txt::Reset; 
        answer = defaultAnswer;
    }
    return answer;
}


void Project::SetMetadata()
{
    LOG(INFO) << txt::Blue << "Initialising a new Cortex at " << fs::canonical(Cortex.Values.SourceRoot.string());
    bool Headless = Cortex.Settings.System.Headless.Active;
    Info.Name = GetAnswer("Project Name",Cortex.Settings.System.Headless.CortexName,Headless);
    Info.Author = GetAnswer("Project Author",Cortex.Settings.System.Headless.AuthorName,Headless);

    Info.Save();
}


////////////////// Linkage function

void Project::Initialise(int argc, char ** argv)
{
    Settings.Parse(argc,argv);    
    Synchronise(false);

    WelcomeMessage();
    CheckHeadless();
    Info.Initialise();
    LoadSettings();

    if (!Info.ExistsOnDisk)    SetMetadata();

    Index.Initialise();
    CachedSettings = Settings;
}

void Project::Connect(WorkerObject * worker, WatcherObject * watcher) 
{
    Worker = worker;
    Watcher = watcher;
    Index.RootDir->Connect();
    Watcher->Start();
}

void Project::Prompt()
{
    std::cout << JSL::Cursor::ClearLine << JSL::Text::Blue << ">> " << JSL::Text::Cyan <<std::flush;
}

bool Project::Synchronise(bool save)
{
    bool compile = Values.Synchronise(Settings,CachedSettings);
    if (save)
    {
        Settings.SaveConfig(Values.SettingsFile);
    }
    return compile;
}

void Project::Clean()
{
    LOG(INFO) << "Beginning a cleaning pass";

    std::error_code ec;
    using fsdir = fs::recursive_directory_iterator;

    if (fs::exists(Values.BuildRoot))
    {
        LOG(DEBUG) << "Deleting entire build directory";
        fs::remove_all(Values.BuildRoot);
    }

    
    // //now search for children
    std::set<fs::path> paths;
    LOG(DEBUG) << "Detected files:";
    for (auto   it = fsdir(Values.CompileRoot,ec); it != fsdir(); ++it) 
    {
        if (ec ) continue;
        auto p = it->path();
        if (p.extension() == ".pdf" || fs::is_directory(p))
        {
            auto r = fs::relative(p,Values.CompileRoot);
            LOG(DEBUG) << "\t = " << r.string();
            paths.insert(r);
        }
    }

    std::set<fs::path> expected;
    Index.RootDir->ListAll(expected);

    LOG(DEBUG) << "Expected files:";
    for (auto & el : expected)
    {
        LOG(DEBUG) << "\t = " << el.string();
    }

    std::set<fs::path> leftovers; // Create the destination

    std::set_difference(
        paths.begin(), paths.end(),
        expected.begin(), expected.end(),
        std::inserter(leftovers, leftovers.begin())
    );

    if (leftovers.size() > 0)
    {
        LOG(INFO) << "The following output disk objects have no corresponding source.\nThey are being deleted.";
        for (auto & left : leftovers)
        {
            LOG(INFO) << "  - " << left.string();
            fs::remove(Values.CompileRoot/left);
        }
    }
    else
    {
        LOG(INFO) << "No files to clean: output matches source";
    }
    Index.RootDir->ExistenceSweep();
    
}