#pragma once
#include <string_view>
#include "../async/worker.h"
class Interface
{
    public:
        Interface();

        void BeginLoop();
        void ConnectWorker(Worker & w);
        bool Headless;
    private:
        Worker * Handler;
        void ConfigureLocation();
        bool ParseCommand(std::string_view cmd);
        bool CommandSearcher(std::vector<std::string_view> & data);
};