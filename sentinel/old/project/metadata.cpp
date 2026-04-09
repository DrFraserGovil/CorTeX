#include "metadata.h"
#include <sstream>
#include "JSL/modules/FileIO/FileIO.h"
#include "../constants.h"
namespace fs = std::filesystem;

Metadata MainProject ;

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

void Metadata::SetDefaults()
{
    Name = "Unknown";
    Author = "None";
}

Metadata::Metadata()
{
    SetDefaults();
}
Metadata::Metadata(fs::path path)
{
    SetDefaults();
    Load(path);
}
void Metadata::Load(fs::path path)
{
    JSL::forSplitLineIn(path," ",[&](auto line)
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
    ExistsOnDisk = true;
}
void Metadata::Save(fs::path path)
{
    std::stringstream s;
    s << "Name: " << Name << "\n";
    s << "Author: " << Author << "\n";
    JSL::writeStringToFile(path,s.str(),std::ios::out);
}