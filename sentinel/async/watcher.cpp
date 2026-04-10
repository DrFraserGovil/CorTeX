#include "watcher.h"
#include <poll.h>
#include <unistd.h>
#include "../global.h"
#include <sys/inotify.h>
#include "parser.h"
#include "tasks/tasks.h"
#include "worker.h"
WatcherObject::WatcherObject() : Callbacks({})
{
    Running= true;

    AddFileWatch();
    
    if(!Cortex.Settings.System.Headless.Active)
    {
        AddMenu();
    }

  
}

void Prompt()
{
    std::cout << JSL::Cursor::ClearLine << JSL::Text::Blue << ">> " << JSL::Text::Cyan <<std::flush;
}


void WatcherObject::Start()
{
    Thread = std::thread(&WatcherObject::Loop,this);
}
void WatcherObject::Loop()
{
    if (!Cortex.Settings.System.Headless.Active) Prompt();
    std::string line;
    // Prompt();
    const int N = Polls.size();
    while(Running)
    {
        int pollresult = poll(Polls.data(),N,Cortex.Settings.System.PollingDelay);


        if (pollresult <= 0) 
        {
            if (errno == EINTR  || pollresult == 0) continue; //no wakeup, just a timeout
            break;
        }

        for (int i = 0; i < N; ++i)
        {
            if (Polls[i].revents & POLLIN)
            {
                Callbacks[i]();
            }
        }
    }
}

void WatcherObject::Exit()
{
    Running = false;
    if (Thread.joinable())
    {
        Thread.join();
    }
}

void WatcherObject::AddMenu()
{
    LOG(INFO) << "Initialising interactive mode";
    pollfd w;
    w.fd = STDIN_FILENO;
    w.events = POLLIN;
    Polls.push_back(w);
    Callbacks.push_back(
        [&](){
            std::string line;
            bool addedTask = false;

            if (std::getline(std::cin, line))
            {
                auto tline = JSL::trim(line);
                if (tline.size() > 0)
                {
                    auto task = ParseCommand(tline);
                    if (task.Type != Instruction::None)
                    {
                        LOG(INFO) << "Adding task";
                        Cortex.Worker->AddTask(task);
                        addedTask = true;
                    }
                }
            }
            if (!addedTask)
            {
                Prompt();
            }
        }
    );
}
void WatcherObject::AddFileWatch()
{
    WatcherID = inotify_init();
    if (WatcherID < 0)
    {
        LOG(ERROR) << "Could not establish inotify process\nReason: " << std::strerror(errno);
        exit(1);
    }


    pollfd w;
    w.fd = WatcherID;
    w.events = POLLIN;
    Polls.push_back(w);
    Callbacks.push_back(
        [&](){
            int length = read(WatcherID, buffer, sizeof(buffer));
            LOG(INFO) << "Got " << std::string(buffer,length);
            
        }
    );
}