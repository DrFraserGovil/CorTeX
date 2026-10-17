#pragma once
#include "JSL.h"

//! Opens the settings console for the project's ArchiveSettings, saving the result
//! @param offerRebuild If true (i.e. on an existing project), changes which affect every note require confirmation, and trigger a clean rebuild
void ConfigInterface(bool offerRebuild = true);
void CreateArchive();
