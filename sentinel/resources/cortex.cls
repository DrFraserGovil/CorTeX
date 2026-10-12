%%%% This defines the style sheet for the cortex notes
%%%% It contains many definitions which are referenced internally by the CorTeX sentinel: DO NOT MODIFY unless you are confident that it won't break things. 
%%%% Errors in this document will prevent a MCE from being generated
%%%% For custom macros available globally in a project, see *macros.sty* instead

\NeedsTeXFormat{LaTeX2e}
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
    \renewcommand{\maketitle}{
        \makeatletter
        {\begin{minipage}{\linewidth} % Force the width
            \if\titleCentered1
            \centering
            \fi
            \bfseries \fontsize{\titleFontSize}{\titleFontSize}\selectfont \@title
        \end{minipage}     
        \makeatother
        }
    }
\fi