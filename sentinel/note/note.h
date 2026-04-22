#pragma once
#include <memory>
#include <filesystem>
#include "note_helpers.h"
#include "fileHeader.h"
#include "linkSet.h"
#include "builder/TexGenerator.h"
class Directory;
class Note
{
    public:
        PathObj Path;
        const int ID;
        bool IsDirty;
        bool HasBeenDeleted;
        void DiskCheck();
        Note(int id, std::filesystem::path path, std::weak_ptr<Directory> parent);
        FileHeader Header;
        void Scan(bool saveToBuffer,bool scanLinks=true);
        friend class FileIndex;
        friend class CompilerObject;
        bool PendingMetaDataChange = false;
        
        
        void Build(std::string_view preamble);
        bool FlushBuild(int truncation);
    private:
        bool FailedToCompile = false;
        BufferObj Buffer;
        std::weak_ptr<Directory> Parent;
        bool SetLinkConnections();
        LinkSet Links;
        int BodyStartLine;
        TexGenerator Generator;

        void Delete();

    // static std::shared_ptr<Note> Create(std::filesystem::path path, std::weak_ptr<Directory> parent);
};