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
    AddHeadlessWatch();
    if(!Cortex.Settings.System.Headless.Active)
    {
        AddMenu();
    }

  
}




void WatcherObject::Start()
{
    if (!Cortex.Settings.System.Headless.Active)
    {
        LOG(INFO) << "Launching interactive mode";
    }
    Thread = std::thread(&WatcherObject::Loop,this);
}
void WatcherObject::Loop()
{
    if (!Cortex.Settings.System.Headless.Active) Cortex.Prompt();
    std::string line;
    // Prompt();
    const int N = Polls.size();
    int i = 0;
    while(Running)
    {
        int pollresult = poll(Polls.data(),N,Cortex.Settings.System.PollingDelay);


        if (pollresult <= 0) 
        {
            
            ++i;
            if (i == 10)
            {
                Cortex.Worker->Prod();
                i = 0;
            }
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
                        Cortex.Worker->AddTask(task);
                        addedTask = true;
                    }
                }
            }
            if (!addedTask)
            {
                Cortex.Prompt();
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
    LOG(DEBUG) << "Creating filewatch system";

    pollfd w;
    w.fd = WatcherID;
    w.events = POLLIN;
    Polls.push_back(w);
    Callbacks.push_back(
        [&](){
            int length = read(WatcherID, buffer, sizeof(buffer));
            std::set<FileReport> batch;
            int i = 0;
            while (i < length)
            {
                struct inotify_event* event = (struct inotify_event*)&buffer[i];
                if (event->len)
                {
                    int id =event->wd;
                    if (id != Cortex.Antenna.ID)
                    {
                        auto report = FileReport(WatchMap[id],event);
    
                        if (report.IsImportant)
                        {
                            batch.insert(report);
                        }
                    }
                    else
                    {
                        auto task = Cortex.Antenna.Ping();
                        if (task.Type != Instruction::None)
                        {
                            Cortex.Worker->AddTask(task);
                        }
                    }

                }           
                i += sizeof(struct inotify_event) + event->len;
            }
            if (!batch.empty())
            {
                std::lock_guard<std::mutex> lock(WatcherSync);
                bool newReports = (Reports.size() == 0 );
                for (auto& report : batch)
                {
                    // Check if we already have a report for this path
                                        
                    auto it = Reports.find(report);
                    if (it == Reports.end())
                    {
                        //if not, add it in
                        Reports.insert(report);
                    }
                    else
                    {
                        //if a match, compound the mask
                        auto modrep = Reports.extract(it);
                        modrep.value().Mask |= report.Mask;
                        Reports.insert(std::move(modrep));
                    }
                }
                if (newReports)
                {
                    Cortex.Worker->AddTask(Instruction::FileChange);
                }
                // else
                // {
                //     LOG(DEBUG) << "A";
                // }
            }


        }
    );
}

void WatcherObject::AddHeadlessWatch()
{
    LOG(DEBUG) << "Creating signal interceptor";
    Cortex.Antenna.ID = inotify_add_watch(WatcherID,Cortex.Values.SharedSessionDirectory.c_str(),IN_CREATE|IN_DELETE);
}

int WatcherObject::WatchDir(std::weak_ptr<Directory> dirPtr)
{
    auto dir = dirPtr.lock();
    LOG(DEBUG) << "\tWatching directory " << dir->Path.Source.string();
    int id = inotify_add_watch(WatcherID,dir->Path.Source.c_str(),IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE);
    WatchMap[id] = dirPtr;
    return id;
}

std::set<FileReport> WatcherObject::GetReports()
{
    std::set<FileReport> out;
    {
        std::lock_guard<std::mutex> lock(WatcherSync);
        std::swap(out,Reports);
    }
    return out;
}