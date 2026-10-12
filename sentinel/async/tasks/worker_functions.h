#pragma once
#include <vector>
#include <string_view>
#include "directory_display.h"
#include "Parameters/Describer.h"
#include "Display/Log.h"

void ShowSetting(std::vector<std::string> & cmd);

bool ChangeSetting(std::vector<std::string> & cmd);

std::pair<bool,JSL::ParameterDescription> CheckParameterData(std::vector<std::string> & data);

bool ConsolidateChanges();

void directoryDisplay(std::vector<std::string> & array);

bool fileChange();

bool Reset(std::vector<std::string> & array);

bool Pause(std::vector<std::string> & array);

//has to be down here as it depends on other funcs
#include "vector_templates.h"