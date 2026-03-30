#pragma once
#include "metadata.h"
#include <string_view>
#include "../async/worker.h"
class Interface
{
    public:
        Interface(Metadata cache);

        void BeginLoop();
        void ConnectWorker(Worker & w);
    private:
        Worker * Handler;
        bool Headless;
        void ConfigureLocation();
        bool ParseCommand(std::string_view cmd);
        bool CommandSearcher(std::vector<std::string_view> & data);
        Metadata MetaCache;
};