#include "note.h"
#include <array>
#include <algorithm>
#include "../project/index.h"


Note::Note(fs::path path, bool isError):IsError(isError){
    UniqueID = -1; //negative = unregistered
    LOG(DEBUG) << "Found " << path;
    SourcePath = path;
};


template<class T,size_t count>
bool contains(T test,const std::array<T,count> & array)
{
    return std::find(array.begin(),array.end(),test) != array.end();
}

template<std::size_t N>
using extensions = std::array<std::string,N>;
#define list std::to_array<std::string>


std::shared_ptr<Note> Note::Create(fs::path path)
{
    std::string extension = path.extension();
    
    if (contains(extension,list({ ".tex" })))
    {
        auto out = std::make_shared<Note>(path); //default object is a tex file
        MasterIndex.Register(out);
        return out;
    }
    // if (contains(extension,list({ ".tikz" })))
    // {
    //     LOG(INFO) << "found tikz";
    //     return std::make_shared<Note>(true); //send a badConstruct signal
    // }
    
    LOG(WARN) << "Encountered file of unknown extension (" << path << ")\nAttempting to interpret as a tex file";
    auto out = std::make_shared<Note>(path,true); //send a badConstruct signal
    MasterIndex.Register(out);
    return out;
}