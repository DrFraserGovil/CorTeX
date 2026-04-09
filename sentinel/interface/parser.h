#pragma once

#include <vector>
#include <string_view>
#include "JSL/modules/Strings/Strings.h"
#include "tasks.h"


Task ParseCommand(std::string_view line);
