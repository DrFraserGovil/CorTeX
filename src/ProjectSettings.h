
#pragma once
#include <JSL/Interface.h>
class ProjectSettings : public JSL::Interface::Aggregator<ProjectSettings>
{
  public:
	//! @alias width
	//! @brief The paperwidth of the rendered document
	double RenderWidthCM = 25;
	//! @alias height
	//! @brief The paperheight of the rendered document
	double RenderHeightCM = 25;

	//! @alias vector
	//! @brief test
	std::vector<double> testVec = {1, 2, 3.3};
#include "Settings.ProjectSettings.autogen"
};

extern ProjectSettings Project;
