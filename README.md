# CorTeX: a LaTeX second brain

CorTeX is a knowledge base of small LaTeX notes, linked like a wiki. Each note compiles on its own; the results are gathered into a
viewer (a web page that runs from local files, with no server) with links, backlinks, search, transclusion and a graph of how notes
connect.

What CorTeX does, in full, is described in [DESIGN.md](DESIGN.md), which is the specification.

## A note

```latex
title: Euler's Identity
alias: euler
-----
\usepackage{tikz}          % a local preamble, for this note only (optional)
-----
\begin{equation} e^{i\pi} + 1 = 0 \label{eq:euler} \end{equation}
It follows from [[Calculus]]; see also [[#eq:euler]], and [[!Units]] is transcluded here.
```

## Requirements

- To build CorTeX: CMake 3.15 or later and a C++20 compiler. [JSL](https://github.com/DrFraserGovil/JSL) is fetched automatically.
- To build a project: XeLaTeX and dvisvgm (both part of TeX Live).

CorTeX builds on Linux, macOS and Windows; opening notes in an editor from the viewer is Linux only.

## Building CorTeX

```sh
make          # builds ./cortex
make test     # builds and runs the tests (the end-to-end ones need XeLaTeX and dvisvgm)
make test TEST_FLAGS="[syntax]"   # just some of them
```

## Using it

```sh
cortex new               # make the current folder a project
cortex watch             # build, then rebuild whenever a note changes (q + Enter stops)
cortex build             # build once; cortex rebuild starts from scratch
cortex export -to DIR    # a copy of the viewer to share
cortex install-handler   # lets the viewer open notes in your editor (Linux)
cortex -h                # everything else
```

Then open `view/index.html` in a browser. A build reports problems briefly; `.cortex/build.log` has the details.

## Source layout

| Folder | Contents |
|---|---|
| `src/App` | The commands |
| `src/Settings` | Command-line and project settings, read and checked into a `Config` |
| `src/Project` | The project's paths, its registry (name, sidebar), and the lock that keeps one process per project |
| `src/Notes` | Reading notes: the header, the extended syntax and links; one reader per file format |
| `src/Network` | Resolving links into a network: names, redirects, backlinks, wanted notes, labels |
| `src/Build` | The build: what to compile and with what, the record of past builds, and sweeping old output |
| `src/Tex` | Running XeLaTeX and dvisvgm |
| `src/Output` | Writing the viewer's files, or PDFs |
| `src/Report` | Warnings and failures, the build log, and the terminal summary |
| `src/Platform` | Running programs, and the desktop integration |
| `src/Link` | The `cortex://` link handler |
| `src/Viewer` | The viewer, whose `js/` files are joined in name order into one script |
| `tests` | Unit tests, and end-to-end tests that build `tests/fixture` |

## Ideas

Beyond what DESIGN.md describes (including its [Room to grow](DESIGN.md#16-room-to-grow)):

- structured notes, edited as forms in the viewer and compiled into richer LaTeX;
- daily notes, named and organised by date;
- queries over notes' metadata;
- annotating external documents, with a shared bibliography;
- compiling a chosen set of notes into one structured document.
