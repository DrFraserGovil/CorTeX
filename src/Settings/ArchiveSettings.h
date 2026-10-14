
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

	//! @alias allowed, allowed-files
	//! @brief Files which match these globs will be added into the archive
	std::vector<std::string> AllowedFilePatterns = {"*.tex"};

	//! @alias ignored, ignored-files
	//! @brief Files which match these globs can never be added (even if they meet the matching criteria)
	std::vector<std::string> IgnoredFilePatterns = {".*"};

	//! @alias pdf
	//! @brief If true, each note is compiled to a standalone PDF instead of SVG pages for the browser viewer
	bool PdfOutput = false;
};

extern ArchiveSettings Archive;
