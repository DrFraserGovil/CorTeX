#include "TexGenerator.h"
#include "../../global.h"
TexGenerator::TexGenerator(LinkSet & links) : Links(links)
{

}

void TexGenerator::WritePreamble(std::string_view preamble,FileHeader header)
{
    LOG(DEBUG) << "\tWriting global preamble";
    auto preambleLines = JSL::split(preamble,"\n");
    for (int i = 0; i < preambleLines.size(); ++i)
    {
        InsertLine(preambleLines[i]);
    }

    LOG(DEBUG) << "\tWriting local preamble";
    for (int i = 0; i < Buffer.Preamble.size(); ++i)
    {
        InsertLine(Buffer.Preamble[i]);
    }

    // //writing title
    std::string title = "\\title{" + header.Title + "}";
    InsertLine(title);
    InsertLine("\\begin{document}");
    InsertLine("\\maketitle");
}

void TexGenerator::BeginBuild(std::string_view preamble, BufferObj & inputBuffer,FileHeader header)
{
    Lines.resize(0);
    std::swap(Buffer,inputBuffer); //inputBUffer is invalidated, but now we own it 
    WritePreamble(preamble,header);
    LinkRender();
    BodyPass();
    InsertLine("\\end{document}");
}

void TexGenerator::Flush(std::fstream & output, int truncation)
{
    for (auto lineIdx: Lines)
    {
        if (lineIdx >= 0)
        {
            output << Buffer.Body[lineIdx] << "\n";
        }
        else
        {
            output << InsertedLines[abs(lineIdx) - 1] << "\n";
        }
    }
    Buffer.Reset();
}

void TexGenerator::LinkRender()
{
     //sweep through and render the links automatically
     //links are always rendered, ignorign the format disabler
    if (Links.Lines.size() > 0)
    {        
        int n = Links.Parsed.size();
        LOG(DEBUG) << "\tRendering " << n << " link" << ((n > 1)?"s":"");
        int linkID = 0;
        for (auto linkLine : Links.Lines)
        {
            auto & line = Buffer.Body[linkLine];
            int pos = 0;
            std::ostringstream newline;
            while (Links.Parsed[linkID].Line == linkLine)
            {
                auto & link = Links.Parsed[linkID];
                newline<< line.substr(pos,link.Start-pos);
                newline << link.Render();
                pos = link.End;
                ++linkID;
            }
            newline << line.substr(pos);
            Buffer.Body[linkLine] = newline.str();
        }
    }
}


struct StarResult
{
    std::string_view whitespace;
    std::string_view remainder;
    bool ManualItem;
    std::string ForceType;
    int count;
};

StarResult StarCount(std::string_view line)
{
    StarResult out;
    out.ManualItem = false;
    out.ForceType = "";
    size_t pos = line.find_first_not_of(" \t");
    if (pos == std::string_view::npos)
    {
        out.count = 0;
        return out;
    }


    //check if user has manually called item -- continues current 
   
    const std::string item = "\\item";
    auto itemTest = line.substr(pos,item.size());
    if (itemTest == item)
    {
        out.ManualItem = true;
        return out;
    }


    
    int stars = 0;
    while (pos < line.size()-1 && line[pos] == '*')
    {
        ++stars;
        ++pos;
    }
    if (line[pos] != ' ')
    {
        //test for type forcing
        auto test = line.substr(pos,5);
        if (test == "ENUM ")
        {
            out.ForceType = "enumerate";
        }
        else
        {
            if (test == "ITEM ")
            {
                out.ForceType = "itemize";
            }
            else
            {
                stars = 0; //we require a space after the stars to count as valid
            }
        }        
    }
    if (stars > 0)
    {
        if (out.ForceType.size() == 0)
        {
            out.whitespace = line.substr(0,pos-stars);
            out.remainder = line.substr(pos+1);
        }
        else
        {
            out.whitespace = line.substr(0,pos-stars);
            out.remainder = line.substr(pos+5);
        }
    }
    out.count = stars;
    return out;
}   


void TexGenerator::ListCheck(int idx,std::string_view line)
{
    auto stars = StarCount(line);
    if (stars.ManualItem){return;} //do nothing and trust the user

    if (stars.ForceType.size() > 0)
    {
        ListType = stars.ForceType;
        if (stars.count == ListDepth) //i.e. changed the type but no depth change
        {
            std::string cmd = "\\end{" + OpenLists.top() + "}";
            OpenLists.pop();
            InsertLine(cmd);
            --ListDepth; //s
        }
    }

    if (stars.count != ListDepth)
    {
        if (stars.count > ListDepth)
        {
            ++ListDepth; //bit cheeky, we effectively truncate the stars
            std::string cmd = "\\begin{" + ListType +"}";
            InsertLine(cmd);
            OpenLists.push(ListType);
        }
        else
        {
            int PendingDecrement = ListDepth - stars.count;
             while (PendingDecrement > 0)
            {
                std::string cmd = "\\end{" + OpenLists.top() + "}";
                OpenLists.pop();
                InsertLine(cmd);
                --PendingDecrement;
            }
        
            ListDepth = stars.count;
        }

        if (ListDepth == 0)
        {
            ListType =Cortex.Settings.Document.DefaultEnumerate  ? "enumerate" : "itemize";
        }
        // Buffer.Body[idx] = std::to_string(stars) + "-" + (std::string)line;
        
    }

    if (ListDepth > 0 )
    {
        Buffer.Body[idx] = (std::string)stars.whitespace +  "\t\\item{}" + (std::string)stars.remainder;
    }

}



void TexGenerator::BodyPass()
{
    State.Scan(Buffer.Body);
    int NValid = State.LineStatus.size(); 

    std::string defaultList = Cortex.Settings.Document.DefaultEnumerate  ? "enumerate" : "itemize";
    
    ListType = defaultList;
    ListDepth = 0;
    for (int i = 0; i < NValid; ++i)
    {
        std::string_view line = Buffer.Body[i];

        auto & status = State.LineStatus[i];
    
        if (status.Enabled)
        {
            ListCheck(i,line);
            //formatting allowed
        }

        Lines.push_back(i);

       
    }
    if (!OpenLists.empty())
    {
        ListCheck(NValid,""); //forces all open lists to close
    }
}