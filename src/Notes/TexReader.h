#pragma once
#include "Reader.h"

namespace Notes
{
	//! A LaTeX note (DESIGN.md §3): header, optional local preamble, and a body in LaTeX plus CorTeX's syntax
	class TexReader : public Reader
	{
	  public:
		std::optional<Note> Read(const std::filesystem::path &root, const std::filesystem::path &source, const ReadOptions &options,
								 Report::Log &report) const override;
	};
} // namespace Notes
