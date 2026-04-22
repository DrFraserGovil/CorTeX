#pragma once
#include <string_view>
#include <vector>
#include <set>
#include <map>
#include <memory>

class Note;
class Link
{
    public:
    int Line;
    int Start;
    int End;
    std::string LinkText; //needs to be a string so it can persist after the bufferis cleared
    std::string RenderText; 
    std::string Target;
    Link(){};
    Link(int lineNo, int startPos, int endPos, std::string_view line);
    static std::vector<Link> GetLinks(std::string_view line,int lineNo);
    std::string Render();
    bool SetTarget(std::weak_ptr<Note> target,std::string & requestingFile);

};
