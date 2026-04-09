#pragma once
#include <string_view>
#include <string>
#include <filesystem>
class Directory; //forward declaration for ptr

namespace fs = std::filesystem;
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

struct PathObj
{
    fs::path Source;
    fs::path Compile;
    fs::path Build;

    PathObj(){};
    PathObj(fs::path path);
};