#include "index.h"
#include "../settings/settings.hpp"
void FileIndex::Register(std::shared_ptr<Note> newNote)
{
    int id = GetID();
    newNote->UniqueID = id;
    Registry[id] = newNote;

    for (auto & alias: newNote->Aliases)
    {
        Aliases[alias].Add(newNote);
    }
}

int FileIndex::GetID()
{
    ++SequentialID;
    return SequentialID;
}

void FileIndex::Initialise()
{
    Structure = std::make_shared<Directory>(Settings.Files.TargetDirectory);
    Structure->IsRoot = true;
    Structure->Walk();
}

std::weak_ptr<Directory> FileIndex::GetStructure()
{
    return Structure;
}

void FileIndex::DeleteFile(std::weak_ptr<Note> file)
{
    auto note = file.lock();
    for (auto & alias : note->Aliases)
    {
        Aliases[alias].Remove(file);
    }

    Registry.erase(note->UniqueID); //erase from the registry 
    note->Delete(); //handfles file cleanup and signals to the inbound links that something is amiss
}

void FileIndex::UnwatchAll()
{
    Structure->Unwatch();
}