#include "note_helpers.h"
#include "../global.h"


PathObj::PathObj(fs::path path) : Source(path)
{
    auto relative = fs::relative(path,Cortex.Values.SourceRoot);
    Compile = Cortex.Values.CompileRoot / relative;
    Build = Cortex.Values.BuildRoot / relative;
}