#pragma once


class Initialiser
{
    public:
        Initialiser(int argc, char**argv);  
    private:
        void WelcomeMessage();
        void LoadSettings();
        void AssignMetadata();
};