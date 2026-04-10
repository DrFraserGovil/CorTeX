#pragma once
#include <string_view>
#include <string>
#include <filesystem>
#include <vector>
class Directory; //forward declaration for ptr

namespace fs = std::filesystem;


struct PathObj
{
    fs::path Source;
    fs::path Compile;
    fs::path Build;

    PathObj(){};
    PathObj(fs::path path);
};

struct BufferObj
{
    std::vector<std::string> Preamble;
    std::vector<std::string> Body;

    void Reset(){Preamble.resize(0); Body.resize(0);}
};
