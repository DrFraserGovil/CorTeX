#include "fileindex.h"
#include "../global.h"
#include <array>
#include <algorithm>
void FileIndex::Initialise()
{
    LOG(DEBUG) << "Index initialising";
    RootDir = Directory::MakeFrom(Cortex.Values.SourceRoot);
    SequentialID = 0;
}


template<class T,size_t count>
bool contains(T test,const std::array<T,count> & array)
{
    return std::find(array.begin(),array.end(),test) != array.end();
}

template<std::size_t N>
using extensions = std::array<std::string,N>;
#define list std::to_array<std::string>

std::weak_ptr<Note> FileIndex::NewNote(fs::path path, std::weak_ptr<Directory> parent)
{
    int id = SequentialID;

    
    ////do the tex handling here
    std::string extension = path.extension();
    if (contains(extension,list({ ".tex" })))
    {
        auto note = std::make_shared<Note>(id,path,parent); //default object is a tex file
        return Register(note);
    }
    // if (contains(extension,list({ ".tikz" })))
    // {
    //     LOG(INFO) << "found tikz";
    //     return std::make_shared<Note>(true); //send a badConstruct signal
    // }
    
    LOG(WARN) << "Encountered file of unknown extension (" << path << ")\nAttempting to interpret as a tex file";
    auto note = std::make_shared<Note>(id,path,parent); //send a badConstruct signal
    return Register(note);
}

std::weak_ptr<Note> FileIndex::Register(std::shared_ptr<Note> note)
{
    SequentialID++;
    int id = note->ID;
    Registry[id] = note;
    PathRegistry[note->Path.Source] = id;
    // for (auto & alias: Registry[id]->Header.Aliases)
    // {
    //     Aliases[alias].Add(newNote);
    // }

    if (note->IsDirty)
    {
        NotifyDirty(id);
    }
    return note;
}

void FileIndex::NotifyDirty(int id)
{
    if (std::find(DirtyFiles.begin(),DirtyFiles.end(),id) == DirtyFiles.end())
    {
        DirtyFiles.push_back(id);
    }
}