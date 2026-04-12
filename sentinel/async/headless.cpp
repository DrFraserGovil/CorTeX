#include "headless.h"
#include "../global.h"
#include <unordered_map>
#include <sstream>
#include <thread>
#include "parser.h"
#include "watcher.h"
// HeadlessBroadcast::HeadlessBroadcast()
// {

// };


void HeadlessInterface::DetectStatus(int argc, char**argv)
{
    Status = Mode::Recieve;
    if (argc > 0)
    {
        auto first = argv[1][0];
        if (first != '-')
        {
            Status = Mode::Send;
        }
        if ((std::string)argv[1]=="purge")
        {
            Status = Mode::Purge;
        }
    }
}

void HeadlessInterface::CreateSession()
{
    // auto index = Cortex.Values.SharedSessionDirectory / "index."
    SetTarget();
    if (fs::exists(LockFile))
    {
        LOG(WARN) <<  "A cortex session of ID " << ID << " already exists\nChecking if the process is dead";
        fs::remove(LockFile);
        bool returned = FileLoop(LockFile,true);
        if (returned)
        {
            LOG(ERROR) << "Cortex session " << ID << " is still active\nCannot start a second session";
            exit(1); 
        }
        LOG(WARN) << "No response from process: acquiring the lock";
    }
    LOG(DEBUG) << "Assigning session id " << ID;
    JSL::initialiseFile(LockFile);
}

void HeadlessInterface::SetTarget()
{
    if (fs::exists(Cortex.Values.SourceRoot))
    {
        std::string canonical = fs::canonical(Cortex.Values.SourceRoot).string();
        ID = std::hash<std::string>{}(canonical);
        
        
        LockFile = Cortex.Values.SharedSessionDirectory / ("session_" + std::to_string(ID) + ".lock");
        MessageFile = LockFile;
        MessageFile.replace_extension(".msg");
        BadTarget = false;
    }
    else
    {
        BadTarget = true;
    }
}

void HeadlessInterface::EndSession()
{
    if (Status==Mode::Recieve && fs::exists(LockFile)) fs::remove(LockFile);
}

std::set<fs::path> HeadlessInterface::GetActive()
{
    std::set<fs::path> out;
    if (fs::exists(Cortex.Values.SharedSessionDirectory))
    {
        using fsdir = fs::directory_iterator;
        std::error_code ec;
        for (auto it = fsdir(Cortex.Values.SharedSessionDirectory,ec); it !=fsdir(); ++it)
        {
            if (ec) continue;

            auto path = it->path();

            if (path.extension() == ".lock")
            {
                out.insert(path);
            }
        }
    }
    return out;
}

void HeadlessInterface::FindSession()
{
    SetTarget();
    if (!BadTarget && fs::exists(LockFile))
    {
        LOG(DEBUG) << "Connected automatically to " << LockFile.stem();
        return;
    }

    auto sessions = GetActive();

    if (sessions.size() == 0)
    {
        LOG(ERROR) << "No active cortex session exist: cannot send signal";
        exit(1);
    }
    if (sessions.size() > 1)
    {
        LOG(ERROR) << "Multiple sessions are active outside of your targeted file: cannot connect automatically\nPlease specify the cortex session to issue a command to";
        LOG(WARN) << "Run 'cortex purge' to remove corrupted session locks";
        exit(1);
    }
    
    LockFile = *sessions.begin();
    MessageFile = LockFile;
    MessageFile.replace_extension(".msg");
    LOG(DEBUG) << "Connected to running service " << LockFile.stem();

}

void HeadlessInterface::Broadcast(int argc, char**argv)
{
    if (Status == Mode::Purge)
    {
        Purge();
        return;
    }

    FindSession();
    std::ostringstream cmd;
    cmd << argv[1]; //we know this is good, as a prerequisite of entering broadcast mode

    for (int i = 2; i < argc; ++i)
    {
        if (argv[i][0] == '-') break;
        cmd <<" " << argv[i];
    }
   
    auto cmdstr = cmd.str();
    LOG(INFO) << "Sending signal to parent process";
    bool success = SendMessage(MessageFile,cmdstr);
    if (!success)
    {
        LOG(ERROR) << "Service timed out waiting for a previous message: parent has died";
        exit(1);
    }
    bool consumed = FileLoop(MessageFile,false);
    if (!consumed)
    {
        LOG(ERROR) << "Signal not acknowledged by " << LockFile.stem().string();
        fs::remove(MessageFile);
        exit(1);
    }

    LOG(INFO) << JSL::Text::Green << "Signal recieved";

}

bool HeadlessInterface::SendMessage(fs::path target, std::string & msg)
{
    bool vanished = FileLoop(target,false);

    if (vanished)
    {
        JSL::writeStringToFile(target,msg + "\n",std::ios::out);
        LOG(DEBUG) << "Message sent to " << target;
        return true;
    }
    else
    {
        fs::remove(target); //remove .cmd
        fs::remove(LockFile); //remove .lock
        return false;
    }
}
bool HeadlessInterface::FileLoop(fs::path target, bool testExist)
{
    auto & headless = Cortex.Settings.System.Headless;

    bool fileExists = fs::exists(target);
    for (int i = 0; i < headless.RecursionLimit;++i)
    {
        if (fileExists == testExist) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(headless.RecursionDelay));
        fileExists = fs::exists(target);
    }


    return fileExists == testExist;


}

void HeadlessInterface::Purge()
{
    //nuke all active sessions
    //if they're still alive, then they recreate their locks and its all fine
    auto sessions = GetActive();
    for (auto session : sessions)
    {
        fs::remove(session);
    }
}


Task HeadlessInterface::Ping()
{
    Task out(Instruction::None);
    if (!fs::exists(LockFile))
    {
        //this is the case if it has been deleted by accident, or in order to test if the process is alive
        //since we're here, we're alive - recreate the file to signal we're here
        LOG(DEBUG) << "Detected a purge attempt: reaquiring lock";
        if (!fs::exists(Cortex.Values.SharedSessionDirectory))
        {
            fs::create_directories(Cortex.Values.SharedSessionDirectory);
            Cortex.Watcher->AddHeadlessWatch();
        }
        JSL::initialiseFile(LockFile);
    }
    
    if (fs::exists(MessageFile))
    {
        LOG(DEBUG) << "External ping recieved";
        std::ostringstream contents;
        JSL::forLineIn(MessageFile,[&](auto line)
        {
              contents<<  line << " ";
        });
        std::string all = contents.str();
        auto cmd = JSL::trim(all);
        LOG(DEBUG) << "Recieved command '" << cmd << "'";
        out = ParseCommand(cmd);
        fs::remove(MessageFile);
    }
    return out;
}