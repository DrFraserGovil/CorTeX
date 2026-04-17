#include "fileHeader.h"
#include "JSL/modules/Strings/Strings.h"
#include "../constants.h"
FileHeader::FileHeader()
{
    
}


bool FileHeader::Parse(std::vector<std::string> & bucket)
{
     //traverse *backwards* so that first title declaration takes priority
    auto equals = JSL::insensitiveEquals;

    std::string newTitle;
    std::vector<std::string> newAlias;
    std::vector<std::string> newTags;


    for (int i = bucket.size() -1; i >= 0; i--)
    {
        auto delim = bucket[i].find_first_of(':');
        if (delim != std::string_view::npos)
        {
            std::string_view line = bucket[i];

            auto cmd = JSL::trim(line.substr(0,delim));
            auto value = JSL::trim(line.substr(delim+1));
            
            if (equals(cmd,"title"))
            {
                newTitle = (std::string)value;
            }
            if (equals(cmd,"alias"))
            {
                auto vals = JSL::split(value,",");
                for (auto val : vals)
                {
                    newAlias.push_back((std::string)JSL::trim(val));
                }
            }
            if (equals(cmd,"tags") | equals(cmd,"tag"))
            {
                auto vals = JSL::split(value,",");
                for (auto val : vals)
                {
                    newTags.push_back((std::string)JSL::trim(val));
                }
            }
        }
    }
    //sort so that permutations in values do not trigger a recompile
    std::sort(newAlias.begin(),newAlias.end());
    std::sort(newTags.begin(),newTags.end());

    bool dataChange = false;
    if (newTitle != Title)
    {
        Title = newTitle;
        dataChange = true;
    }
    if (newAlias != Aliases)
    {
        std::swap(newAlias,Aliases);
        dataChange  = true;
    }
    if (newTags != Tags)
    {
        std::swap(newTags,Tags);
        dataChange  = true;
    }
    dataChange |= Initialised; //if not initialised, then we don't care about changes - we're just populating for the first time 
    Initialised = true;
    
    return dataChange;
}

