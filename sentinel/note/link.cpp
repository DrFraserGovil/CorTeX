#include "link.h"
#include "JSL/modules/Strings/trim.h"
#include "../global.h"
Link::Link(int lineNo, int startPos, int endPos, std::string_view line) : Line(lineNo), Start(startPos), End(endPos)
{
    auto capture = line.substr(startPos+2,endPos-startPos-4);
    auto idx = capture.find_first_of("|");
    if (idx != std::string_view::npos)
    {
        LinkText = JSL::trim(capture.substr(0,idx));
        RenderText = JSL::trim(capture.substr(idx+1));
        if (RenderText.size() == 0)
        {
            RenderText = LinkText;
        }
    }
    else
    {
        LinkText = JSL::trim(capture);
        RenderText = LinkText;
    }
}

std::vector<Link> Link::GetLinks(std::string_view line,int lineNo)
{
    std::vector<Link> out;
    if (line.size() < 5) return out;
    bool prevBracket = (line[0] == '[');
    bool insideLink=false;
   
    int beginIdx;
    for (int j = 1; j < line.size()-2; ++j)
    {
        bool isBracket = (line[j] == '[');
        if (isBracket && prevBracket)
        {
            insideLink = true;
            beginIdx = j-1;
        }

        if (insideLink)
        {
            if (line[j] == ']' && line[j+1] == ']')
            {
                int endIdx = j+2;
                out.emplace_back(lineNo,beginIdx,endIdx,line);
                insideLink = false;
            }
        }


        prevBracket = isBracket;
    }
    return out;

}


std::string Link::Render()
{
    if (!Target.empty())
    {
        return "\\href{" + Target + "}{" + std::string(RenderText) + "}";
    }
    LOG(WARN) << "Could not resolve link to alias '" << LinkText << "'";
    return "\\textcolor{red}{" + std::string(RenderText) + "}";
}

bool Link::SetTarget(std::weak_ptr<Note> target, std::string & requestingFile)
{
    auto oldTarget = Target;
    Target = "";
    if (auto targetPtr = target.lock())
    {
        auto path = targetPtr->Path.Compile;
        auto relpath = fs::relative(path,fs::path(requestingFile).parent_path());
        Target = relpath.string();
    }
    return oldTarget != Target;
}