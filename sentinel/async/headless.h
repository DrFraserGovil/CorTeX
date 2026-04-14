#pragma once
#include <filesystem>
#include <set>
#include "tasks/tasks.h"

struct Signal
{
    bool Recieved;
    Task Command;
};
class HeadlessInterface
{
    public:
        enum class Mode
        {
            Send,
            Recieve,
            Purge
        };
        Mode Status; 
        void DetectStatus(int argc, char **argv);
        void CreateSession();
        void EndSession();
        
        void Broadcast(int argc, char**argv);
        Task Ping();
        int ID;
    private:
        void FindSession(bool allowFailure=false);

        std::filesystem::path LockFile;
        std::filesystem::path MessageFile;
        void SetTarget();
        std::set<std::filesystem::path> GetActive();
        void Purge();
        bool SendMessage(std::filesystem::path target,std::string & msg);
        bool VerifyMessage(std::filesystem::path target);
        bool FileLoop(std::filesystem::path target,bool testExist);
        bool BadTarget;
};


// class HeadlessBroadcast
// {
//     public:
//         HeadlessBroadcast();
//         void SendMessage();
// };
    
// class HeadlessAntenna
// {
//     public:
//         HeadlessAntenna();
//         void CreateSession();
//         void ProcessMessage();
        
// };