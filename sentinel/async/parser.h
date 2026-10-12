#pragma once

#include <vector>
#include <string_view>
#include "tasks/tasks.h"


Task ParseCommand(std::string_view line);
