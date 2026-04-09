#include "glob.h"
#include <fnmatch.h>
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