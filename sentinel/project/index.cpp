#include "index.h"
#include "../settings/settings.hpp"
void FileIndex::Register(std::shared_ptr<Note> newNote)
{
    int id = GetID();
    newNote->UniqueID = id;
    Registry[id] = newNote;

    for (auto & alias: newNote->Header.Aliases)
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
    for (auto & alias : note->Header.Aliases)
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

void FileIndex::NotifyDirty(int id)
{
    DirtyFiles.push_back(id);
}


void FileIndex::Compile(bool forceAll)
{
    std::ostringstream preamble;
    preamble << "\\documentclass[width =" << Settings.Document.Width << "cm, " <<  Settings.Document.FontSize << "pt]{cortex}\n";

    for (auto & package: Settings.Document.Packages)
    {
        preamble << "\\usepackage{" << package << "}\n";
    }

    //compile the global settings
    preamble << "\\def\\titleFontSize{" << Settings.Document.TitleSize << "}\n";
    preamble << "\\def\\titleCentered{" << (int)Settings.Document.TitleCentered << "}\n";

    //now insert the macros file
    fs::path macros = (fs::path)Settings.Files.TargetDirectory / macroFile;
    JSL::forLineIn(macros,[&](std::string_view line)
    {
        if (line[0] != '%') // omit comment lines
        {
            preamble << line << "\n";
        }
    });

    std::string globalPreamble = preamble.str();
    if (forceAll)
    {
        for (auto & file : Registry)
        {
            file.second->Compile(globalPreamble);
        }
    }
    else
    {
        while (DirtyFiles.size() > 0)
        {
            if (Registry.contains(DirtyFiles[0]))
            {
                Registry[DirtyFiles[0]]->Compile(globalPreamble);
            }
            DirtyFiles.pop_front();
        }
    }
}


void FileIndex::FindFile(fs::path path)
{
    auto truePath = Settings.Files.TargetDirectory / path;
    
    auto parent = Structure->Find(path.parent_path()).lock();
    auto file = path.filename();
    
    LOG(DEBUG) << "Attempting to locate " << truePath;
    if (parent->Notes.contains(file))
    {
        auto entry = parent->Notes[file].lock();
        if (fs::exists(truePath)) //check if deletion
        {
            LOG(DEBUG) << "Found file, recompiling";
            DirtyFiles.push_back(entry->UniqueID);
            
        }
        else
        {
            LOG(DEBUG) << "File no longer on disk: deleting";
            entry->Delete();
        }
    }
    else
    {
        LOG(DEBUG) << "Not found - creating a new entry";
        //create a new file
        auto note = parent->NewNote(truePath);
        DirtyFiles.push_back(note.lock()->UniqueID);
    }

}