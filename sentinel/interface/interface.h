#pragma once
#include "metadata.h"
class Interface
{
    public:
        Interface(Metadata cache);

    private:
        bool Headless;
        void InterfaceLoop();
        void ConfigureLocation();
        Metadata MetaCache;
};