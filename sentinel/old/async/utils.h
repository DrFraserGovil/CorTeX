#pragma once
#include <vector>
#include <fnmatch.h>
#include <filesystem>

// bool glob(const std::string & path, const std::vector<std::string> & globPatterns);

void MakeLowerCase(std::string & string);

std::string lowerStem(std::filesystem::path & path);