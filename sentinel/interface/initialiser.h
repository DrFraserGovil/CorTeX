#pragma once
#include "metadata.h"

class Initialiser
{
    public:
        Initialiser(int argc, char**argv);  
        Metadata GetMetadata();
    private:
        void WelcomeMessage();
        void LoadSettings();

};