#pragma once
#include <iosfwd>

//! The language server (DESIGN.md §13.7): `cortex lsp`, started by an editor, speaking the Language Server Protocol over a pair of streams
namespace Lsp
{
	//! Serves the editor until it says to exit, or its input ends
	//! @details If no other CorTeX process holds the project, the server holds it and is its watch: it builds, rebuilds on every change,
	//! and answers other cortex commands, exactly as `cortex watch` does. If another process holds it, the server is passive: it only
	//! re-reads the project on changes (never compiling), so its answers stay current while the other process builds
	//! @returns 0 after a clean shutdown, 1 if the editor exited without asking the server to shut down first
	int Serve(std::istream &in, std::ostream &out);
} // namespace Lsp
