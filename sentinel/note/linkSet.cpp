#include "linkSet.h"
#include "note.h"


LinkSet::LinkSet()
{
    Reset();
}

void LinkSet::Reset()
{
    Orphaned.clear();
    Outbound.clear();
    Parsed.clear(); Parsed.resize(0);
    Lines.clear();
}



void LinkSet::Parse(std::vector<std::string> & content)
{
    Reset();

    for (int i = 0; i < content.size(); ++i)
    {
        std::string_view line = content[i];
        auto links = Link::GetLinks(line,i);

        if (links.size() > 0)
        {
            Lines.push_back(i);
            Parsed.insert(Parsed.end(),links.begin(),links.end());
        }
    }

    for (auto & link : Parsed)
    {
        if (!Outbound.contains(link.LinkText))
        {
            Orphaned.insert((std::string)link.LinkText);
        }
    }
}