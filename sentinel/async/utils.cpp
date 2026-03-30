#include "utils.h"
#include <ctype.h>
#include <algorithm>

#include "../constants.h"
bool glob(const std::string & path, const std::vector<std::string> & globPatterns)
{
    for (const auto & pattern: globPatterns)
    {
        if (fnmatch(pattern.c_str(), path.c_str(), FNM_CASEFOLD) == 0)
        {
            return true;
        }
    }
    return false;
}

std::string lowerStem(std::filesystem::path & path)
{
    std::string stem = path.stem();
    MakeLowerCase(stem);
    return stem;
}

void MakeLowerCase(std::string & string)
{
     std::transform(string.begin(), string.end(), string.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
}