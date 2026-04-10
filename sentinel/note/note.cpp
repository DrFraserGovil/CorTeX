#include "note.h"
#include "../global.h"

Note::Note(int id, std::filesystem::path path, std::weak_ptr<Directory> parent) : Path(path),ID(id), Parent(parent)
{
    Path.Compile.replace_extension(".pdf");
    IsDirty = false;
    Scan(false);
    DiskCheck();
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

bool isDelimiter(std::string_view line)
{
    if (line.empty()) return false;

    if (line.size() < Cortex.Settings.Files.StructureDelimiterRepeatCount) return false;

    auto firstChar = line[0];
    for (int i = 1; i < line.size(); ++i)
    {
        if (line[i] != firstChar) return false;
    }
    return true;
}


void Note::Scan(bool saveToBuffer)
{
    std::vector<std::vector<std::string>> fileChunks;
    std::vector<std::string> bucket;
    int i = 1;
    JSL::forLineIn(Path.Source,[&](auto line){

        if (isDelimiter(line) && fileChunks.size() < 2)
        {
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
    Header.Parse(fileChunks[0]);
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

    //don't care about preamble at this stage, just sweep the body for link indicators

    Links.Parse(Buffer.Body);
    if (!saveToBuffer)
    {
        Buffer.Reset();
    }

}

void Note::Build(std::string_view preamble,int Truncation)
{
    std::fstream output(Path.Build,std::ios::out);
    //global preamble & documentclass
    output << preamble;

    //local preamble
    for (auto & line : Buffer.Preamble)
    {
        output << line << "\n";
    }

    //main matter
    output << "\\begin{document}\n";
    int linkId = 0;
    output << "\\title{" << Header.Title << "}\n";
    for (int i = 0; i < Buffer.Body.size()-Truncation; ++i)
    {
        std::string_view line = Buffer.Body[i];

        if (i == Links.Lines[linkId])
        {
            int pos = 0;
            while (Links.Parsed[linkId].Line == i)
            {
                auto & link = Links.Parsed[linkId];
                output << line.substr(pos,link.Start-pos);
                output << "\\textcolor{red}{" << link.RenderText <<"}";
                pos = link.End;
                ++linkId;
            }
            output << line.substr(pos);
            
        }
        else
        {
            output << line;
        }
        output << "\n";
    }

    output << "\\end{document}";

    output.close();
}

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