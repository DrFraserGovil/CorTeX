#pragma once
#include <string>
#include <filesystem>
#include "JSL/modules/Display/Log.h"

const std::string metadataLocation = ".cortex/about.dat";
const std::string clsLocation = "cortex.cls";
const std::string macroFile = "macros.sty";
const std::string settingLocation = ".cortex/settings.dat";
namespace fs = std::filesystem;
namespace txt = JSL::Text;

