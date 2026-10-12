#include "TexGenerator.h"
#include "../../global.h"
TexGenerator::TexGenerator(LinkSet & links) : Links(links)
{
    FormatReplacers["*"] = {"\\textit{","}","italic"};
    FormatReplacers["**"] = {"\\textbf{","}","bold"};
    FormatReplacers["***"] = {"\\textbf{\\textit{","}}","bold-italic"};
    FormatReplacers["_"] = {"\\texttt{","}","monospace"};
    FormatReplacers["__"] = {"\\textsc{","}","smallcap"};
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


void TexGenerator::ListCheck(int  idx)
{
    std::string_view line = "";
    if (idx > 0){line = Buffer.Body[idx];};
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
            TrackReplacement(ListType);
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
        
    }

    if (ListDepth > 0 )
    {
        Buffer.Body[idx] = (std::string)stars.whitespace +  "\t\\item{}" + (std::string)stars.remainder;
    }

}

bool WhereNext(int & current, std::vector<std::pair<int,bool>> activation, int & activationIdx, int maxsize)
{
    ++current;
    if (activation.size() == 0) return false;

    if (current == 0)
    {   
        bool active;
        bool jump = 0;
        if (activation[0].first == 0) //line starts inactive
        {
            active = activation[0].second;
            jump = 1;
        }
        else
        {
            active = !activation[0].second;
        }

        if (!active)
        {
            if (activation.size() <= jump)
            {
                current = maxsize;
                return false;
            }
            current = activation[jump].first;
            ++activationIdx;
        }
        return false;
    }
    int endIdx = activation[activationIdx].first;
    if (current == endIdx)
    {
        ++activationIdx;
        if (activationIdx >= activation.size())
        {
            current = maxsize; //forces exit
        }
        else
        {
            current = activation[activationIdx].first;
        }
        return true;
    }
    return false;
    

}

void TexGenerator::FormatCheck(int idx)
{
    std::string_view line = Buffer.Body[idx];
    //initial quick scan to see if anything even needs doing:
    bool foundChar = false;
    std::vector<char> controlCharacters = {'*','_'};
    for(auto letter : line)
    {
        if (std::find(controlCharacters.begin(),controlCharacters.end(),letter) != controlCharacters.end())
        {
            foundChar = true;
            break;
        }
    }
    if (!foundChar) return;
    //we are in a line with at least one control character
    auto & status = State.LineStatus[idx];
    
    int i = -1;
    int block = 0;
    bool grabbed = false;
    char grabbedChar;
    std::string currentFormat;
    int grabIdx;

    WhereNext(i,status.PartialArray,block,line.size());
    bool escaped = false;
    char prev = '\0';
    std::deque<std::tuple<int,int,std::string>> registered;
    while (i < line.size())
    {
        //handle escaping
        if (line[i] == '\\')
        {
            if (prev == '\\')
            {
                escaped = !escaped;
            }
            else
            {
                escaped = true;
            }
        }
        prev = line[i];
        if (std::isspace(line[i]) || line[i] == '{' || line[i] == '[')
        {
            escaped = false;
        }



        if (!escaped)
        {
       
            auto g = std::find(controlCharacters.begin(),controlCharacters.end(),line[i]);
            if (g != controlCharacters.end())
            {
                grabbedChar = *g;
                int lookahead = 1;
                while (line[i+lookahead] == grabbedChar)
                {
                    ++lookahead;
                    if (i + lookahead == line.size() && !grabbed)
                    {
                        break;
                    }
                }

                std::string sequence = std::string(lookahead,grabbedChar);
               
                if (grabbed && sequence == currentFormat)
                {
             
                    //got a closer!
                    registered.emplace_back(grabIdx,i+lookahead,sequence);
                    grabbed = false;
                }
                else
                {
                    if (grabbed)
                    {
                        LOG(DEBUG) << "Mismatched format indicator at l:c = " << idx << ":" << grabIdx;
                        LOG(DEBUG) << "Encountered " << sequence << " but expected " << currentFormat;
                        grabbed=false;
                    }
                    if (i + lookahead < line.size() && !std::isspace(line[i+lookahead]))
                    {
                        grabbed=true;
                        std::swap(currentFormat,sequence);
                        grabIdx = i;
                    }
                }

                i += lookahead -1; //don't double count the things we just added in
            }
        }
       

        

        bool jumped = WhereNext(i,status.PartialArray,block,line.size());
        if (jumped)
        {
            //abort current attempts
            escaped = false;
            grabbed = false;
        }
    }
    
    for (auto it = registered.rbegin(); it!=registered.rend();++it)
    {
        auto reformat = *it;
        auto format = std::get<2>(reformat);
        int b =  std::get<0>(reformat);
        int e =  std::get<1>(reformat);
        
        if (FormatReplacers.contains(format))
        {
            std::string contents = (std::string)line.substr(b+format.size(),e-b-2*format.size());
            auto wrap = FormatReplacers[format];
            auto begin = std::get<0>(wrap);
            auto end = std::get<1>(wrap);
            auto name = std::get<2>(wrap);
            Buffer.Body[idx].replace(b,e-b,begin + contents + end);
            line = Buffer.Body[idx]; //need to reassign the stringview as the underlying string has been reallocated
            TrackReplacement(name);
        }
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
        auto & status = State.LineStatus[i];
        
        if (status.Enabled)
        {
            FormatCheck(i);
            if (status.Type == status.Full)
            {
                ListCheck(i);
            }
            else
            {
                //list checks are only valid if the initial line is format enabled, and doesn't affect the rest of the line.
                bool activeAtStart = !status.PartialArray[0].second;
                if (activeAtStart)
                {
                    ListCheck(i);
                }
            }
            
        }

        Lines.push_back(i);

       
    }
    if (!OpenLists.empty())
    {
        ListCheck(-1); //forces all open lists to close
    }
}

void TexGenerator::TrackReplacement(std::string & cmd)
{   
    if (ReplaceMap.contains(cmd))
    {
        ReplaceMap[cmd] += 1;
    }
    else
    {
        ReplaceMap.insert({cmd,1});
    }
    
}

void TexGenerator::Report()
{
    if (!ReplaceMap.empty())
    {
        std::ostringstream out;
        out << "\tAutoformatting reports:";
        for (auto type : ReplaceMap)
        {
            out << "\n\t\t" << type.second << " " << type.first << (type.second > 1 ? "s" :""); 
        }
        LOG(DEBUG) << out.str();
    }
}