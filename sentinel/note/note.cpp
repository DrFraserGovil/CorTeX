#include "note.h"
#include "../global.h"
#include "FileIO/FileIO.h"
Note::Note(int id, std::filesystem::path path, std::weak_ptr<Directory> parent) : Path(path),ID(id), Parent(parent), Generator(Links)
{
    Path.Compile.replace_extension(".pdf");
    IsDirty = false;
    Scan(false);
    DiskCheck();
    LOG(DEBUG) << txt::Italics<< "\tNote " << id << " (" << Path.Source.filename().string() << ": " << Header.Title <<")";
    if (IsDirty)
    {
        LOG(DEBUG) << "\t\t" << Cortex.Colours.DebugRed << "Out of sync with compiled output";
    }
}

void Note::DiskCheck()
{
    HasBeenDeleted = false;
    if (!fs::exists(Path.Source))
    {
        HasBeenDeleted = true;
        return;
    }

    bool fileExists = fs::exists(Path.Compile);
    if (!fileExists)
    {
        IsDirty = true;
        return;
    }


    auto sourceTime = fs::last_write_time(Path.Source);
    auto compileTime = fs::last_write_time(Path.Compile);

    if (sourceTime > compileTime)
    {
        IsDirty = true;
    }

}

bool isDelimiter(std::string_view line, char expectedDelimiter)//we can use linebreak as a safe 'null': it can never enter into this routine as it is the definition of the end of a line!
{
    if (line.empty()) return false;

    if (line.size() < Cortex.Settings.Files.StructureDelimiterRepeatCount) return false;

    auto firstChar = line[0];
    if (expectedDelimiter != '\n' && firstChar != expectedDelimiter) return false;
    if (std::isspace(firstChar)) return false;
    for (int i = 1; i < line.size(); ++i)
    {
        if (line[i] != firstChar) return false;
    }
    return true;
}


void Note::Scan(bool saveToBuffer,bool scanLinks)
{
    std::vector<std::vector<std::string>> fileChunks;
    std::vector<std::string> bucket;
    int i = 1;
    char expectedDelimiter='\n';
    JSL::forLineIn(Path.Source,[&](auto line){

        if (isDelimiter(line,expectedDelimiter) && fileChunks.size() < 2)
        {
            expectedDelimiter =line[0];
            BodyStartLine = i+1;
            fileChunks.push_back(bucket);
            bucket.clear();
        }
        else
        {
            bucket.push_back(line);
        }
        i++;
    });

    fileChunks.push_back(bucket);

    //always assume firts block is title + link metadata
    PendingMetaDataChange = Header.Parse(fileChunks[0]);
    if (Header.Title.size() == 0)
    {
        Header.Title = Path.Source.stem().string();
    }

    if (fileChunks.size() == 2)
    {
        Buffer.Preamble.resize(0);
        std::swap(Buffer.Body,fileChunks[1]);
    }
    if (fileChunks.size() == 3)
    {
        std::swap(Buffer.Preamble,fileChunks[1]);
        std::swap(Buffer.Body,fileChunks[2]);
    }

    
    if (scanLinks)      Links.Parse(Buffer.Body);
    //don't care about preamble at this stage, just sweep the body for link indicators
    if (!saveToBuffer)
    {
        Buffer.Reset();
    }
}


void Note::Build(std::string_view preamble)
{
    IsDirty = false;
    Generator.BeginBuild(preamble,Buffer,Header);
}
bool Note::FlushBuild(int truncation)
{
    Generator.Report();
    JSL::initialiseFile(Path.Build);
    std::fstream output(Path.Build,std::ios::out);
    Generator.Flush(output,truncation);
    output.close();
    return false;
}
// void Note::Build(std::string_view preamble,int Truncation)
// {
//     IsDirty = false;
//     std::fstream output(Path.Build,std::ios::out);
//     //global preamble & documentclass
//     output << preamble;

//     //local preamble
//     for (auto & line : Buffer.Preamble)
//     {
//         output << line << "\n";
//     }

//     //main matter
//     output << "\\begin{document}\n";
//     int linkId = 0;
//     output << "\\title{" << Header.Title << "}\n";
//     std::string fileSource = 
//     for (int i = 0; i < Buffer.Body.size()-Truncation; ++i)
//     {
//         std::string_view line = Buffer.Body[i];
//         if (linkId < Links.Lines.size() && Links.Lines[linkId] == i)
//         {
//             int pos = 0;
//             while (Links.Parsed[linkId].Line == i)
//             {
//                 auto & link = Links.Parsed[linkId];
//                 output << line.substr(pos,link.Start-pos);
//                 output << link.Render(fileSource);
//                 // output << "\\textcolor{red}{" << link.RenderText <<"}";
//                 pos = link.End;
//                 ++linkId;
//             }
//             output << line.substr(pos);
            
//         }
//         else
//         {
//             output << line;
//         }
//         output << "\n";
//     }

//     output << "\\end{document}";

//     output.close();
// }




void tryDelete(fs::path path)
{
    if (fs::exists(path))
    {
        fs::remove(path);
    }
}

void Note::Delete()
{
    LOG(DEBUG) << "Deleting " << Path.Source.string() << " and associated files";
    
    // tryDelete(SourcePath); // shouldn't risk deleting source code automatically!
    tryDelete(Path.Build);
    tryDelete(Path.Compile);
}

bool Note::SetLinkConnections()
{
    bool changed = false;
    auto fileSource = Path.Compile.string();
    for (auto & link : Links.Parsed)
    {
        auto target = Cortex.Index.GetLink(link.LinkText,Path.Compile);
       
        changed |= link.SetTarget(target,fileSource);
    }
    return changed;
}