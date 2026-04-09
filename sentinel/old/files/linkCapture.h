#pragma once
#include <string_view>
#include <vector>
class Link
{
    public:
    int Line;
    int Start;
    int End;
    std::string_view LinkText;
    std::string_view RenderText;
    Link(){};
    Link(int lineNo, int startPos, int endPos, std::string_view line);
    static std::vector<Link> GetLinks(std::string_view line,int lineNo);
};

