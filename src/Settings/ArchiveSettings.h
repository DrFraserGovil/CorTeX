
#pragma once
#include <JSL/Interface.h>

class FileSettings : public JSL::Interface::Aggregator<FileSettings>
{
  public:
	//! @alias allowed, allowed-files
	//! @brief Files which match these globs will be added into the archive
	std::vector<std::string> AllowedFilePatterns = {"*.tex"};

	//! @alias ignored, ignored-files
	//! @brief Files which match these globs can never be added (even if they meet the matching criteria)
	std::vector<std::string> IgnoredFilePatterns = {".*"};

	//! @alias home
	//! @brief The note shown when the viewer opens, as a path relative to the project root (e.g. home.tex). If empty or invalid, the viewer opens on an automatically constructed overview of the project. A display setting only: changing it never recompiles notes
	std::string HomeNote = "";
#include "ArchiveSettings.FileSettings.autogen"
};

class RenderSettings : public JSL::Interface::Aggregator<RenderSettings>
{
  public:
	//! @alias pdf
	//! @brief If true, each note is compiled to a standalone PDF instead of SVG pages for the browser viewer
	bool PdfOutput = false;

	//! @alias width
	//! @brief The paperwidth of the rendered document
	double WidthCM = 25;

	//! @alias zoom
	//! @brief How large notes are drawn in the viewer, relative to their true printed size (1 = actual size). A display setting only: changing it never recompiles notes, and readers can adjust it further with the viewer's zoom buttons
	double ViewerZoom = 1.4;

	//! @alias background, background-colour, background-color
	//! @brief The viewer's page colour, behind and around the notes, as any CSS colour (e.g. #f4ecd8, or wheat); empty keeps the default white. A display setting only: changing it never recompiles notes
	std::string BackgroundColour = "";

	//! @alias insert-title, title
	//! @brief If true, each note's title is typeset at the top of the note (using the CortexTitle macro, which the preamble may redefine)
	bool InsertTitle = true;
#include "ArchiveSettings.RenderSettings.autogen"
};

class SyntaxSettings : public JSL::Interface::Aggregator<SyntaxSettings>
{
  public:
	//! @alias extended, extended-syntax
	//! @brief If true, notes may also use CorTeX's extended syntax: *italic*, **bold**, ***both***, _monospace_, __small caps__, lines starting with # as headings (# section, ## subsection, ...), and lines starting with stars as lists ([[links]] always work)
	bool ExtendedSyntax = false;

	//! @alias list-type
	//! @brief The environment used for star lists (itemize or enumerate); a list can override it by starting with *ENUM or *ITEM
	std::string DefaultListType = "itemize";
#include "ArchiveSettings.SyntaxSettings.autogen"
};

class ArchiveSettings : public JSL::Interface::Aggregator<ArchiveSettings>
{
  public:
	SyntaxSettings Syntax;
	RenderSettings Render;
	FileSettings Files;
#include "ArchiveSettings.ArchiveSettings.autogen"
};

extern ArchiveSettings Archive;
