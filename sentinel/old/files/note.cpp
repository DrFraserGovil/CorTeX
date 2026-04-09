#include "note.h"
#include <array>
#include <algorithm>
#include "../project/index.h"
#include "JSL/modules/FileIO/FileIO.h"
#include "../settings/settings.hpp"
#include "directory.h"

Note::Note(fs::path path,std::weak_ptr<Directory> parent, bool isError):IsError(isError), Parent(parent){
    UniqueID = -1; //negative = unregistered
    SourcePath = path;

    BuildPath = (Parent.lock()->BuildEquivalent / SourcePath.stem());
    BuildPath.replace_extension(".tex");

    CompilePath = (Parent.lock()->OutputEquivalent / SourcePath.stem());
    CompilePath.replace_extension(".pdf");

    Scan();
    DiskCheck();
};


template<class T,size_t count>
bool contains(T test,const std::array<T,count> & array)
{
    return std::find(array.begin(),array.end(),test) != array.end();
}

template<std::size_t N>
using extensions = std::array<std::string,N>;
#define list std::to_array<std::string>


std::shared_ptr<Note> Note::Create(fs::path path,std::weak_ptr<Directory> parent)
{
    std::string extension = path.extension();
    
    if (contains(extension,list({ ".tex" })))
    {
        auto out = std::make_shared<Note>(path,parent); //default object is a tex file
        MasterIndex.Register(out);
        return out;
    }
    // if (contains(extension,list({ ".tikz" })))
    // {
    //     LOG(INFO) << "found tikz";
    //     return std::make_shared<Note>(true); //send a badConstruct signal
    // }
    
    LOG(WARN) << "Encountered file of unknown extension (" << path << ")\nAttempting to interpret as a tex file";
    auto out = std::make_shared<Note>(path,parent,true); //send a badConstruct signal
    MasterIndex.Register(out);
    return out;
}


bool isDelimiter(std::string_view line)
{
    if (line.empty()) return false;

    if (line.size() < Settings.Files.StructureDelimiterRepeatCount) return false;

    auto firstChar = line[0];
    for (int i = 1; i < line.size(); ++i)
    {
        if (line[i] != firstChar) return false;
    }
    return true;
}

void Note::Scan(bool saveToBuffer)
{
    IsError = false;
    std::vector<std::vector<std::string>> fileChunks;
    std::vector<std::string> bucket;
    int i = 1;
    JSL::forLineIn(SourcePath,[&](auto line){

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
    if (i == 1){IsError = true; return;}
    fileChunks.push_back(bucket);

    //always assume firts block is title + link metadata
    Header.Parse(fileChunks[0]);
    if (Header.Title.size() == 0)
    {
        if (!NoTitleWarn)
        {
            LOG(WARN) << "No title detected for '" << SourcePath.filename().string() << "'. Default name will be assigned";
            NoTitleWarn = true;
        }
        Header.Title = SourcePath.stem().string();
    }

    if (fileChunks.size() == 2)
    {
        PreambleBuffer.resize(0);
        std::swap(BodyBuffer,fileChunks[1]);
    }
    if (fileChunks.size() == 3)
    {
        std::swap(PreambleBuffer,fileChunks[1]);
        std::swap(BodyBuffer,fileChunks[2]);
    }

    //don't care about preamble at this stage, just sweep the body for link indicators

    CheckLinks();
    if (!saveToBuffer)
    {
        //free the memory
        BodyBuffer.clear();
        PreambleBuffer.clear();
    }

}


void Note::CheckLinks()
{
    ParsedLinks.clear();
    LinesWithLinks.clear();
    auto originalOrphans = OrphanedLinks;
    for (int i = 0; i < BodyBuffer.size(); ++i)
    {
        std::string_view line = BodyBuffer[i];
        auto links = Link::GetLinks(line,i);
        
        if (links.size() > 0) LinesWithLinks.push_back(i);
        ParsedLinks.insert(ParsedLinks.end(),links.begin(),links.end());
    }

    for (auto & link : ParsedLinks)
    {
        if (!OutboundLinks.contains(link.LinkText))
        {
            OrphanedLinks.insert((std::string)link.LinkText);
        }
    }
}

void Note::ToBuild(std::string_view preamble,int Truncation)
{
    if (!IsError)
    {
        std::fstream output(BuildPath,std::ios::out);
        //global preamble & documentclass
        output << preamble;

        //local preamble
        for (int i = 0; i < PreambleBuffer.size(); ++i)
        {
            output << PreambleBuffer[i] << "\n";
        }

        //main matter
        output << "\\begin{document}\n";
        int linkId = 0;
        output << "\\title{" << Header.Title << "}\n";
        for (int i = 0; i < BodyBuffer.size()-Truncation; ++i)
        {
            if (i == LinesWithLinks[0])
            {
                int pos = 0;
                std::string_view line = BodyBuffer[i];
                while (ParsedLinks[linkId].Line == i)
                {
                    auto & link = ParsedLinks[linkId];
                    output << line.substr(pos,link.Start-pos);
                    output << "\\textcolor{red}{" << link.RenderText <<"}";
                    pos = link.End;
                    ++linkId;
                }
                output << line.substr(pos);
                
            }
            else
            {
                output << BodyBuffer[i];
            }
            output << "\n";
        }

        output << "\\end{document}";

        output.close();
    }

}


void Note::Compile(std::string_view preamble)
{
    if (BodyBuffer.size() == 0){Scan(true);};
    if (IsError){return;}
    int truncation = 0;
    auto dir = Parent.lock()->BuildEquivalent;
    fs::path build;
    while (truncation < BodyBuffer.size())
    {
        ToBuild(preamble,truncation);
        
        auto canonical = (fs::canonical)((fs::path)Settings.Files.TargetDirectory);
        std::string texinputs = "TEXINPUTS=.:" + canonical.string() + "/: ";
        
        std::string cmd = texinputs + "pdflatex -interaction=nonstopmode -halt-on-error -output-directory=" + dir.string();
        cmd += " " + BuildPath.string() + "> /dev/null 2>&1";

        int status = std::system(cmd.c_str());
        int exitCode = WEXITSTATUS(status);
        
        if (exitCode == 0){
            break;
        } 
        ++truncation;
    }
    auto buildpdf = BuildPath;
    if (fs::exists(buildpdf))
    {
        buildpdf.replace_extension(".pdf");
        fs::rename(buildpdf,CompilePath);
        if (truncation == 0)
        {
            LOG(DEBUG) << JSL::Text::Green << "Successfully compiled " << Header.Title << " (Note " << UniqueID <<")";
        }
        else
        {
            LOG(WARN) << JSL::Text::Red << "Compilation error in " << Header.Title << " (Note " << UniqueID <<")\n\tError found on line " << BodyBuffer.size() - truncation + BodyStartLine << "\n\tGenerated a Minimally-Compiling Document";
        }
    }
    else
    {
        LOG(WARN) << "Failed to compile a MCD for " << Header.Title << " (Note " << UniqueID <<")";
    }
    BodyBuffer.clear();
    PreambleBuffer.clear();
    isDirty = false;
}

void Note::DiskCheck()
{
    if (!fs::exists(SourcePath))
    {
        return;
    }

    bool fileExists = fs::exists(CompilePath);
    if (!fileExists)
    {
        isDirty = true;
        return;
    }


    auto sourceTime = fs::last_write_time(SourcePath);
    auto compileTime = fs::last_write_time(CompilePath);

    if (sourceTime > compileTime)
    {
        isDirty = true;
    }

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
    LOG(DEBUG) << "Deleting " << SourcePath.string() << " and associated files";
    
    // tryDelete(SourcePath); // shouldn't risk deleting source code automatically!
    tryDelete(BuildPath);
    tryDelete(CompilePath);
}