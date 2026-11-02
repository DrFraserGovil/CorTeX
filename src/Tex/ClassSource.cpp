#include "ClassSource.h"

// In SVG mode the generated \documentclass line also carries the global option dvisvgm, which every driver-aware package (graphics,
// graphicx, color, xcolor, the LaTeX3 backend, ...) picks up whenever and by whomever it is loaded; pgf reads \pgfsysdriver instead, set
// here. [pdf] leaves the native drivers alone.
static constexpr std::string_view CortexClass = R"tex(\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{cortex}[CorTeX note class]
\newif\ifcortex@svg
\cortex@svgtrue
\def\cortex@width{25cm}
\def\cortex@margin{8pt}
\DeclareKeys[cortex]{
	svg .code = \cortex@svgtrue,
	pdf .code = \cortex@svgfalse,
	width .store = \cortex@width,
	margin .store = \cortex@margin,
	unknown .code = \PassOptionsToClass{\CurrentOption}{standalone}
}
\ProcessKeyOptions[cortex]
\ifcortex@svg
	\def\pgfsysdriver{pgfsys-dvisvgm.def}
\fi
% a margin on every page (the margin setting): without one, glyphs touch the edge of the SVG and the browser clips their tops,
% and anything drawn outside the text width (such as a tcolorbox with oversize, whose frame hangs out by its rule and padding) is cut off
\edef\cortex@border{\noexpand\PassOptionsToClass{border=\cortex@margin}{standalone}}
\cortex@border
\LoadClass{standalone}

% The engine wraps every note body in this, so all notes share one fixed width (and narrower content can be centred within it).
% minipage resets \parindent and \parskip to zero, so the values set by the preambles are captured before it opens, and restored inside.
\newlength{\cortex@parindent}
\newskip\cortex@parskip
\newenvironment{cortexbody}
{%
	\setlength{\cortex@parindent}{\parindent}%
	\setlength{\cortex@parskip}{\parskip}%
	\begin{minipage}{\cortex@width}%
	\setlength{\parindent}{\cortex@parindent}%
	\setlength{\parskip}{\cortex@parskip}%
}
{\end{minipage}}

% With the InsertTitle setting, every note begins \CortexTitle{<title>}; users may \renewcommand it
\newcommand{\CortexTitle}[1]{{\Large\bfseries #1\par}\medskip}

% [[links]] are rewritten by the engine into \CortexLink{<origin>}{<slug>}{<text>}, where <origin> is the key of the note the link was written
% in: a link always resolves from where it was written, even inside a transcluded note.
% SVG output: the text is wrapped in an <a> pointing at the viewer's route for that origin and slug (a raw dvisvgm special: no hyperref, no
% border box, and a link which wraps across lines stays one anchor); the viewer resolves it and styles the link.
% PDF output: there is no viewer, so the engine resolves every link before compiling, and declares each target with
% \CortexLinkTarget{<origin>}{<slug>}{<path to the target PDF, relative to this one, hex-encoded so no filename can upset TeX>}.
% A resolved link becomes a GoToR annotation (a raw xdvipdfmx special: "open that file"), followed by PDF readers such as Okular;
% an unresolved one is just styled. Users may \renewcommand \CortexLinkText / \CortexUnresolvedLink (or \CortexLink itself) in their preamble.
\newcommand{\CortexLinkText}[1]{\special{color push rgb 0.1 0.37 0.7}#1\special{color pop}}
\newcommand{\CortexUnresolvedLink}[1]{\special{color push rgb 0.75 0.1 0.15}#1\special{color pop}}
\newcommand{\CortexLinkTarget}[3]{\expandafter\def\csname cortex@target@#1@#2\endcsname{#3}}
\ifcortex@svg
	\newcommand{\CortexLink}[3]{\special{dvisvgm:raw <a class="cortex-link" xlink:href="\string##/a/#1/#2">}#3\special{dvisvgm:raw </a>}}
\else
	\newcommand{\CortexLink}[3]{%
		\ifcsname cortex@target@#1@#2\endcsname
			\special{pdf:bann << /Type/Annot /Subtype/Link /Border[0 0 0] /A << /S/GoToR /F <\csname cortex@target@#1@#2\endcsname> /D[0 /Fit] >> >>}%
			\CortexLinkText{#3}%
			\special{pdf:eann}%
		\else
			\CortexUnresolvedLink{#3}%
		\fi}
\fi

% [[!transclusions]] are rewritten into \CortexTransclude{<origin>}{<slug>}. The engine resolves each one before compiling (from its origin,
% like links) and declares it with \CortexTranscludeTarget{<origin>}{<slug>}{<fragment>}{<folder>}: the fragment to \input, and the
% transcluded note's folder (relative to the page's, empty if the same), which is searched first while it is input, so its relative
% \includegraphics and \input paths resolve from its own folder. A transclusion which can't be made (no such note; it would loop back on
% itself) is declared with \CortexTranscludeProblem, and shown in place with \CortexTransclusionProblem, which users may \renewcommand.
\newcommand{\CortexTranscludeTarget}[4]{\expandafter\def\csname cortex@transclude@#1@#2\endcsname{\cortex@transclude{#3}{#4}}}
\newcommand{\CortexTranscludeProblem}[3]{\expandafter\def\csname cortex@transcludeproblem@#1@#2\endcsname{#3}}
\newcommand{\CortexTransclusionProblem}[1]{{\special{color push rgb 0.75 0.1 0.15}[#1]\special{color pop}}}
\newcommand{\CortexTransclude}[2]{%
	\@ifundefined{cortex@transclude@#1@#2}%
		{\@ifundefined{cortex@transcludeproblem@#1@#2}%
			{\CortexTransclusionProblem{cannot transclude #2}}%
			{\CortexTransclusionProblem{\csname cortex@transcludeproblem@#1@#2\endcsname}}}%
		{\csname cortex@transclude@#1@#2\endcsname}}
% The search paths are saved and restored around the \input (not put in a group, which would change what the transcluded text means:
% its definitions would no longer outlive it). Transclusions nest, so the saved paths are kept per depth
\newcount\cortex@depth
\def\cortex@none{\cortex@none}
\def\cortex@transclude#1#2{%
	\if\relax\detokenize{#2}\relax
		\input{#1}%
	\else
		\global\advance\cortex@depth\@ne
		\ifx\input@path\@undefined
			\expandafter\let\csname cortex@inputpath@\the\cortex@depth\endcsname\cortex@none
			\def\input@path{{#2/}}%
		\else
			\expandafter\let\csname cortex@inputpath@\the\cortex@depth\endcsname\input@path
			\edef\input@path{{#2/}\unexpanded\expandafter{\input@path}}%
		\fi
		% graphics searches \Ginput@path (set by \graphicspath) in place of \input@path, even when it is undefined
		\ifx\Ginput@path\@undefined
			\expandafter\let\csname cortex@graphicspath@\the\cortex@depth\endcsname\cortex@none
			\def\Ginput@path{{#2/}}%
		\else
			\expandafter\let\csname cortex@graphicspath@\the\cortex@depth\endcsname\Ginput@path
			\edef\Ginput@path{{#2/}\unexpanded\expandafter{\Ginput@path}}%
		\fi
		\input{#1}%
		\expandafter\ifx\csname cortex@inputpath@\the\cortex@depth\endcsname\cortex@none
			\let\input@path\@undefined
		\else
			\expandafter\let\expandafter\input@path\csname cortex@inputpath@\the\cortex@depth\endcsname
		\fi
		\expandafter\ifx\csname cortex@graphicspath@\the\cortex@depth\endcsname\cortex@none
			\let\Ginput@path\@undefined
		\else
			\expandafter\let\expandafter\Ginput@path\csname cortex@graphicspath@\the\cortex@depth\endcsname
		\fi
		\global\advance\cortex@depth\m@ne
	\fi}

% Label references ([[#label]], [[target#label]]) are rewritten into \CortexRef{<origin>}{<slug>}{<label>}{<text>}. The engine resolves
% each before compiling and declares it with \CortexRefTarget{<origin>}{<slug>}{<label>}{<shown>}{<action>}: what to show when no text is
% given (\ref{<label>} for a label on this page, else the number it has in the note which owns it, or ??), and where it goes. In SVG the
% action is a viewer route; in PDF it is the body of a PDF action dictionary (/S/GoTo ... or /S/GoToR ...). Either may be empty: no link.
\newcommand{\CortexRefTarget}[5]{\expandafter\def\csname cortex@ref@#1@#2@#3\endcsname{\cortex@ref{#4}{#5}}}
\newcommand{\CortexRef}[4]{%
	\@ifundefined{cortex@ref@#1@#2@#3}%
		{\CortexUnresolvedLink{??}}%
		{\csname cortex@ref@#1@#2@#3\endcsname{#4}}}
\def\cortex@ref#1#2#3{% shown, action, text
	\if\relax\detokenize{#3}\relax\cortex@reflink{#2}{#1}\else\cortex@reflink{#2}{#3}\fi}
\ifcortex@svg
	\def\cortex@reflink#1#2{%
		\if\relax\detokenize{#1}\relax#2\else
			\special{dvisvgm:raw <a class="cortex-ref" xlink:href="\string##/#1">}#2\special{dvisvgm:raw </a>}%
		\fi}
\else
	\def\cortex@reflink#1#2{%
		\if\relax\detokenize{#1}\relax#2\else
			\special{pdf:bann << /Type/Annot /Subtype/Link /Border[0 0 0] /A << #1 >> >>}\CortexLinkText{#2}\special{pdf:eann}%
		\fi}
\fi

% Every \label leaves an invisible mark where it is, so a reference can take the reader to it: in SVG an empty element carrying its
% position, in PDF a named destination. The engine declares each label on the page with \CortexLabelName{<label>}{<hex name>}, so
% nothing from the label itself reaches the output unescaped; labels it doesn't know about (from an \input file, say) get no mark.
% Since 2023-06-01 LaTeX's \label has a hook, which every \label (amsmath's display ones too) runs, and which packages keep: nameref,
% for one, replaces a \label that isn't LaTeX's own with LaTeX's. Before that, \label is wrapped before any package is loaded
\newcommand{\CortexLabelName}[2]{\expandafter\def\csname cortex@labelname@#1\endcsname{#2}}
\ifcortex@svg
	\def\cortex@mark#1{\special{dvisvgm:raw <g class="cortex-label" data-label="#1" data-x="{?x}" data-y="{?y}"/>}}
\else
	\def\cortex@mark#1{\special{pdf:dest <#1> [@thispage /XYZ @xpos @ypos null]}}
\fi
\def\cortex@labelmark#1{\@ifundefined{cortex@labelname@#1}{}{\expandafter\cortex@mark\expandafter{\csname cortex@labelname@#1\endcsname}}}
\@ifl@t@r\fmtversion{2023-06-01}
	{\AddToHookWithArguments{label}{\cortex@labelmark{#1}}}
	{\let\cortex@label\label
	 \def\label#1{\@bsphack\cortex@labelmark{#1}\cortex@label{#1}\@esphack}}
)tex";

std::string_view Tex::ClassSource()
{
	return CortexClass;
}
