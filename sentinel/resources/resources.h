#pragma once
#include <string_view>

namespace Resources {
    // From cortex.cls
    inline constexpr std::string_view cortex_cls = R"cortex_cls(\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{cortex}

\RequirePackage{kvoptions}
\SetupKeyvalOptions{
  family=CORTEX,
  prefix=cortex@
}

% Define the 'width' option
\DeclareStringOption[10cm]{width} % Default is 10cm

% Standard toggle options
\newif\if@fullpage
\@fullpagefalse
\DeclareOption{article}{\@fullpagetrue}
\DeclareOption{standalone}{\@fullpagefalse}

%% catch everything else and pass to base classes
\DeclareOption*{%
  \PassOptionsToClass{\CurrentOption}{article}%
  \PassOptionsToClass{\CurrentOption}{standalone}%
}

% Process everything
\ProcessKeyvalOptions*
\ProcessOptions\relax

\if@fullpage
    \LoadClass{extarticle}
    % Use the captured width for geometry
    \RequirePackage[paperwidth=\cortex@width, margin=1in]{geometry}
\else
    % Pass the captured width to standalone's varwidth
    \LoadClass[varwidth=\cortex@width,margin=5pt,
        class=extarticle
    ]{standalone}
    
    
    \newcommand{\titleFontSize}{21pt}
    \newcommand{\titleCentered}{0}
    \renewcommand{\title}[1]{
        \makeatletter
        {\begin{minipage}{\linewidth} % Force the width
            \if\titleCentered1
            \centering
            \fi
            \bfseries \fontsize{\titleFontSize}{\titleFontSize}\selectfont #1
        \end{minipage}     
        \makeatother
        }
    }
\fi)cortex_cls";

}