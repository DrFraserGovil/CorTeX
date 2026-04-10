#include "metadata.h"
#include "../global.h"

Metadata::Metadata()
{
    SetDefaults();
}

void Metadata::SetDefaults()
{
    Name = "Unknown";
    Author = "None";
}

void Metadata::Initialise()
{
    LOG(DEBUG) << "Checking existence of " << Cortex.Values.MetaRoot; 


    bool dirExists = fs::exists(Cortex.Values.MetaRoot);
    bool fileExists = fs::exists(Cortex.Values.MetaHeadFile);

    if (!dirExists)
    {
        LOG(DEBUG) << "Initialising cortex metadata directory";
        fs::create_directories(Cortex.Values.MetaRoot);
    }
    if (dirExists && !fileExists)
    {
        LOG(INFO) << "Cortex metadata incomplete or corrupted. Reinitialising project.\n" << JSL::Text::Italics<< "Existing notes will not be affected.";
    }

    ExistsOnDisk = dirExists && fileExists;

    if (ExistsOnDisk) Load();
}

std::string joinTail(std::vector<std::string_view> & s, size_t start)
{
    std::ostringstream out;
    for (size_t i = start; i < s.size(); ++i)
    {
        if (i > start)
        {
            out << " ";
        }
        out << s[i];
    }
    return out.str();
}
void Metadata::Load()
{
    JSL::forSplitLineIn(Cortex.Values.MetaHeadFile," ",[&](auto line)
    {
        if (JSL::insensitiveEquals(line[0],"author:"))
    {
        Author = joinTail(line,1);
    }
    if (JSL::insensitiveEquals(line[0],"name:"))
    {
        Name = joinTail(line,1);
    }
    });
}

void Metadata::Save()
{
    std::stringstream s;
    s << "Name: " << Name << "\n";
    s << "Author: " << Author << "\n";
    JSL::writeStringToFile(Cortex.Values.MetaHeadFile,s.str(),std::ios::out);
}