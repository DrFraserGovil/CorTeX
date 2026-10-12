// The things in here can be accessed (and modified) anywhere in the code 
// Where possible, should only be included in .cpp files to prevent excess reinclusion
#pragma once
#include "Display/Log.h"
#include "project/project.h"

namespace fs = std::filesystem;
namespace txt = JSL::Text;
extern Project Cortex;
