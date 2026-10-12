#pragma once
#include "Display/ANSI_Codes.h"


using colour = JSL::TerminalFormat;
struct ColourHolder
{
    colour DebugDefault =JSL::Text::Colour(80,80,60);
    colour DebugYellow=  JSL::Text::Colour(150,150,10);;
    colour DebugBlue = JSL::Text::Colour(50,50,80);
    colour DebugRed = JSL::Text::Colour(150,40,40);
    colour DebugGreen =JSL::Text::Colour(50,80,50);
    colour CompileSuccess = JSL::Text::Colour(50,180,50);
    colour CompileError = JSL::Text::Red;
    colour ScanStart = JSL::Text::Yellow;
    colour CompileStart = JSL::Background::White;
};