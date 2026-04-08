#include "index.h"
#include "../settings/settings.hpp"
#include <algorithm>
void FileIndex::Register(std::shared_ptr<Note> newNote)
{
    int id = GetID();
    newNote->UniqueID = id;
    Registry[id] = newNote;

    for (auto & alias: newNote->Header.Aliases)
    {
        Aliases[alias].Add(newNote);
    }

    if (newNote->isDirty)
    {
        NotifyDirty(id);
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
    if (std::find(DirtyFiles.begin(),DirtyFiles.end(),id) == DirtyFiles.end())
    {
        DirtyFiles.push_back(id);
    }
}


void FileIndex::Compile(bool forceAll)
{
    // Structure->ExistanceSweep(); //perform a full resweep of the structure to ensure that everything still works fine
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
                LOG(DEBUG) << JSL::Text::Colour(50,80,50) << "Compiling " << DirtyFiles[0];
                Registry[DirtyFiles[0]]->Compile(globalPreamble);
            }
            else
            {
                LOG(DEBUG) << JSL::Text::Colour(50,80,50) << "Ignoring " << DirtyFiles[0] << " due to file deletion";
            }
            DirtyFiles.pop_front();
        }
    }
}


void FileIndex::FindFile(fs::path path)
{
    auto truePath = Settings.Files.TargetDirectory / path;
    
    auto find = Structure->Find(path.parent_path());

    if (find.use_count() == 0)
    {
        return;
    }

    auto parent = find.lock();
    auto file = path.filename();
    
    if (parent->Notes.contains(file))
    {
        auto entry = parent->Notes[file].lock();
        if (fs::exists(truePath)) //check if deletion
        {
            LOG(DEBUG) << truePath << " identified with file " << entry->UniqueID;
            NotifyDirty(entry->UniqueID);
            
        }
        else
        {
            LOG(DEBUG) << truePath << " no longer on disk: deleting";
            DeleteFile(entry);
        }
    }
    else
    {
        LOG(DEBUG) << truePath << " not in index - creating a new entry";
        //create a new file
        auto note = parent->NewNote(truePath);
        NotifyDirty(note.lock()->UniqueID);
    }

}

bool FileIndex::IsDirty()
{
    return DirtyFiles.size() > 0;
}

void FileIndex::CleanOutput()
{
    std::error_code ec;
    using fsdir = fs::recursive_directory_iterator;

    fs::path dir = (fs::path)Settings.Files.TargetDirectory/ Settings.Files.OutputDirectory;
    // //now search for children
    std::set<fs::path> paths;
    for (auto   it = fsdir(dir,ec); it != fsdir(); ++it) 
    {
        if (ec ) continue;
        auto p = it->path();
        if (p.extension() == ".pdf")
        {
            paths.insert(fs::relative(p,dir));
        }
    }

    std::set<fs::path> expected;
    Structure->GatherOutputs(expected);


    std::set<fs::path> leftovers; // Create the destination

    std::set_difference(
        paths.begin(), paths.end(),
        expected.begin(), expected.end(),
        std::inserter(leftovers, leftovers.begin())
    );

    if (leftovers.size() > 0)
    {
        LOG(INFO) << "The following output files have no corresponding source.\nThey are being deleted.";
        for (auto & left : leftovers)
        {
            LOG(INFO) << "  - " << left.string();
            fs::remove(dir/left);
        }
    }
    else
    {
        LOG(INFO) << "No files to clean: output matches source";
    }
}