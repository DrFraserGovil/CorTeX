#pragma once

#include "metadata.h"
#include "../index/fileindex.h"
class Project
{
    public:
        Project();

        FileIndex Index;
        void Initialise(int argc,char**argv);
        Metadata Info;
    private:
};

