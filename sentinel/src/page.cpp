#include "page.h"


Page::Page(std::filesystem::path path) : Path(path)
{
    Name = lowerStem(path);
    LOG(DEBUG) << Name << " page created";
}

