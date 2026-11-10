#pragma once
#include <filesystem>

//! Where everything belongs in a project (DESIGN.md §2.1). Only the output folder is the author's choice; the rest lives in .cortex
namespace Project
{
	struct Paths
	{
		Paths(const std::filesystem::path &root, const std::filesystem::path &output);

		std::filesystem::path Root;
		std::filesystem::path Cortex;	 //!< .cortex
		std::filesystem::path Settings;	 //!< .cortex/settings.config
		std::filesystem::path Registry;	 //!< .cortex/registry.config
		std::filesystem::path Preamble;	 //!< .cortex/preamble.tex
		std::filesystem::path Output;	 //!< the viewer or the PDFs
		std::filesystem::path Build;	 //!< .cortex/build: LaTeX's working files, one folder per note
		std::filesystem::path Fragments; //!< .cortex/fragments: each note's processed body, which pages \input
		std::filesystem::path Class;	 //!< .cortex/class: cortex.cls, alone (see Tex::Compiler)
		std::filesystem::path Warm;		 //!< .cortex/warm: where warm workers run (see Tex::WarmPool)
		std::filesystem::path Staging;	 //!< .cortex/staging: a rebuild's output, until it's complete
		std::filesystem::path Previous;	 //!< .cortex/previous: the output a rebuild replaced, until the swap is done
		std::filesystem::path State;	 //!< .cortex/buildstate.tsv: what each note was compiled from
		std::filesystem::path Log;		 //!< .cortex/build.log: the last build's full account

		//! True if root holds a project (both its settings and its registry exist)
		static bool IsProject(const std::filesystem::path &root);
		//! The project containing start (a file or folder): the nearest folder at or above it which is a project; empty if none
		static std::filesystem::path Find(const std::filesystem::path &start);
	};
} // namespace Project
