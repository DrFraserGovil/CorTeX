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
    LOG(DEBUG) << "Beginning build";
    std::swap(Buffer,inputBuffer); //inputBUffer is invalidated, but now we own it 

    
    WritePreamble(preamble,header);
    BasicLinePass();
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
}

void TexGenerator::BasicLinePass()
{
    if (Links.Lines.size() > 0)
    {        
        LOG(DEBUG) << "\tRendering links";
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
    //finall insertion
    for (int i = 0; i < Buffer.Body.size(); ++i)
    {
        Lines.push_back(i);
    }
}