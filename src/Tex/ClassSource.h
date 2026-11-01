#pragma once
#include <string_view>

namespace Tex
{
	//! The source of cortex.cls, the class every note is compiled against: the page layout, the output-mode drivers, and the commands
	//! CorTeX's syntax is rewritten into (\CortexLink, \CortexTransclude, \CortexRef, and the declarations the engine makes for them)
	std::string_view ClassSource();
} // namespace Tex
