#include "fileindex.h"
#include "../global.h"
#include <array>
#include <algorithm>
void FileIndex::Initialise()
{
    LOG(DEBUG) << "Index initialising";
    SequentialID = 0;
    RootDir = Directory::MakeFrom(Cortex.Values.SourceRoot);
    UpdateLinkNetwork();
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

    auto relpath = fs::relative(note->Path.Source,Cortex.Values.SourceRoot);
    PathRegistry[relpath] = id;
    
    Aliases.Sync(note);
    

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

std::weak_ptr<Note> FileIndex::GetNote(fs::path path)
{
    if (PathRegistry.contains(path))
    {
        return Registry[PathRegistry[path]];
    }
    else
    {
        return std::weak_ptr<Note>{};
    }
    
} 


void FileIndex::Delete(std::weak_ptr<Note> note)
{
    auto n = note.lock();
    auto relpath = fs::relative(n->Path.Source,Cortex.Values.SourceRoot);
    Registry.erase(n->ID);
    PathRegistry.erase(relpath);

    n->Delete();
}

bool FileIndex::IsDirty()
{
    return !DirtyFiles.empty();
}

std::weak_ptr<Note> FileIndex::GetLink(std::string_view keyView, fs::path requestingFile)
{
    const std::string key = (std::string)keyView;
    if (Aliases.Aliases.contains(key))
    {
        return Aliases.Aliases[key].GetClosestLink(requestingFile);
    }
    else
    {
        return std::weak_ptr<Note>{};
    }
}


void FileIndex::UpdateLinkNetwork(bool forceAll)
{
    //marks all files as dirty
    LOG(DEBUG) << DebugTitleColour << "Updating link network";
    if (forceAll)
    {
        LOG(DEBUG) << "\tForcing full disk sweep and compile";
        DirtyFiles.clear();
        for (auto &[id,note]: Registry)
        {
            note->IsDirty = true;
            DirtyFiles.push_back(id);
        }
    }



    std::deque<int> newDirty;
    for (auto &[id,note]: Registry)
    {
        if (note->IsDirty)
        {
            LOG(DEBUG) << "\tScanning file " << note->Path.Source.string();
            note->Scan(true);
        }

        if (note->PendingMetaDataChange)
        {
            note->PendingMetaDataChange = false;
            Aliases.Sync(note);
        }
        if (note->SetLinkConnections() || note->IsDirty)
        {
            newDirty.push_back(id);
            if (!note->IsDirty)
            {
                note->Scan(true,false); //if the file isn't already dirty, we need to rescan to load it into memory -- but disable link parsing as we already know where they point
            }
        }
    }
    std::swap(DirtyFiles,newDirty);
}