#pragma once
#include "../constants.h"
#include <set>
#include <map>
#include <memory>
#include "fileHeader.h"
#include <deque>
#include "linkCapture.h"
class Directory;

struct CaseInsensitiveLess {
    using is_transparent = void; // Enable string_view lookups

    bool operator()(std::string_view lhs, std::string_view rhs) const {
        return std::lexicographical_compare(
            lhs.begin(), lhs.end(),
            rhs.begin(), rhs.end(),
            [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) < 
                       std::tolower(static_cast<unsigned char>(b));
            }
        );
    }
};

// struct CompileResult{
//     bool Success;
//     int exitCode;
// }

class Note
{
    public:
        Note(fs::path path,std::weak_ptr<Directory> parent,  bool isError=false);

        static std::shared_ptr<Note> Create(fs::path path,std::weak_ptr<Directory> parent);
        void Delete(){LOG(DEBUG) << "Deleting " << SourcePath;};
        bool IsError = false;
        void Scan(bool saveToBuffer=false);
        void Compile(std::string_view preamble);
        friend class FileIndex;
        FileHeader Header;
        void SetDirty(){isDirty=true;};
    private:
        int BodyStartLine;
        fs::path SourcePath;
        fs::path BuildPath;
        fs::path CompilePath;
        int UniqueID;
        bool isDirty=false;
        std::weak_ptr<Directory> Parent;
        std::vector<std::weak_ptr<Note>> InboundLinks;
        
        std::map<std::string, std::weak_ptr<Note>,CaseInsensitiveLess> OutboundLinks;
        std::set<std::string> OrphanedLinks;

        std::vector<Link> ParsedLinks;

        void CheckLinks();
        void ToBuild(std::string_view preamble="",int Truncation=0);
        std::vector<std::string> PreambleBuffer;
        std::vector<std::string> BodyBuffer;
        std::deque<int> LinesWithLinks;
        bool NoTitleWarn = false;
};

typedef std::weak_ptr<Note> NotePtr;
//SSoT for where note types are included
