#include "index.h"

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