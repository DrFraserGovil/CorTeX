#pragma once
#include <string>

namespace Notes
{
	//! How a reference counts on the graph, as marked where it was written (DESIGN.md §9.2)
	enum class Strength
	{
		Default, //!< unmarked: strong, unless the target note is `graph: loose`
		Loose,	 //!< [[~x]]: a real reference, but left off the graph
		Strong,	 //!< [[+x]]: on the graph, even if the target note is `graph: loose`
	};

	//! One [[...]] in a note's body (DESIGN.md §4.2)
	struct Reference
	{
		enum class Kind
		{
			Link,		  //!< [[target]]
			Transclusion, //!< [[!target]]
			Label,		  //!< [[#label]] or [[target#label]]
		};
		Kind Type = Kind::Link;
		Strength Marked = Strength::Default;
		//! The target note as written; empty for [[#label]], which finds its note by the label
		std::string Target;
		//! Target in the form used for matching (see Syntax::Slug); empty when Target is
		std::string Slug;
		//! For a label reference, the label (exact: labels are not slugs); else empty
		std::string Label;
		//! 1-based line within the body
		size_t Line = 0;
		//! 0-based column of its "[[" within that line
		size_t Column = 0;
	};

	//! A \label{...} written in a note's body: the note owns it (DESIGN.md §4.4)
	struct Label
	{
		std::string Name;
		size_t Line = 0;
	};
} // namespace Notes
