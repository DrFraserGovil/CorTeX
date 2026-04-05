#pragma once
#include<string>
#include<string_view>
#include<vector>
class FileHeader
{
    public:
        FileHeader();
        bool Parse(std::vector<std::string> & data);
        std::string Title;
        std::vector<std::string> Aliases;
        std::vector<std::string> Tags;
};