
#pragma once
#include <JSL/Interface.h>
class ArchiveSettings : public JSL::Interface::Aggregator<ArchiveSettings>
{
  public:
	//! @alias width
	//! @brief The paperwidth of the rendered document
	double RenderWidthCM = 25;
	//! @alias height
	//! @brief The paperheight of the rendered document
	double RenderHeightCM = 25;

#include "ArchiveSettings.ArchiveSettings.autogen"
};

extern ArchiveSettings Archive;
