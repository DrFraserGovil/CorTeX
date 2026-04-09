#include "note_helpers.h"
#include "../global.h"


PathObj::PathObj(fs::path path) : Source(path)
{
    auto relative = fs::relative(path,Global.SourceRoot);
    Compile = Global.CompileRoot / relative;
    Build = Global.BuildRoot / relative;
}