// The things in here can be accessed (and modified) anywhere in the code 
// Where possible, should only be included in .cpp files to prevent excess reinclusion
#pragma once
#include "settings/settings.hpp" //automatically grants access to the global Settings object defined as extern here
#include "JSL/modules/Display/Log.h"
#include "settings/derived.h"
#include "project/project.h"

namespace fs = std::filesystem;
namespace txt = JSL::Text;
extern Project Cortex;
