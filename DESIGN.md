# CorTeX: behaviour

This document describes what CorTeX does: what it reads, what it produces, the syntax it accepts, and how it responds to the
author and the reader. It deliberately says nothing about how any of this is achieved. Where behaviour is undecided or known to
be wrong, it is listed under [Open questions](#14-open-questions) rather than described as intended. Behaviour described here which
CorTeX doesn't yet have is listed under [Not yet built](#15-not-yet-built). Kinds of note CorTeX doesn't have, but must be able to
gain without upheaval, are described under [Room to grow](#16-room-to-grow).

---

## 1. What CorTeX is

CorTeX is a personal knowledge base written in LaTeX: a folder of small notes, each a `.tex` file, which link to and include one
another. CorTeX compiles every note individually and presents the collection either as a browsable website that runs from local
files (the *viewer*), or as a folder of linked PDFs.

The source files are the single source of truth. Everything CorTeX produces can be deleted and regenerated from them, and nothing a
reader does in the viewer changes the project except through the deliberate, narrow actions in [§10](#10-acting-on-the-project-from-the-viewer).

---

## 2. A project

A project is a folder (the *root*) containing a `.cortex` folder. Everything below the root is the project's content.

### 2.1 Project files

| File | Purpose |
|---|---|
| `.cortex/settings.config` | The project's settings ([§11](#11-settings)) |
| `.cortex/registry.config` | The project's name, and the global sidebar ([§8.6](#86-the-sidebar)). Once called `registry.ctx`: a registry of that name is renamed when it is next read |
| `.cortex/preamble.tex` | LaTeX preamble shared by every note |
| the output folder | The viewer, or the PDFs: `view/` unless the `output` setting says otherwise. Owned entirely by CorTeX |

A folder is a project only if both `settings.config` and `registry.config` (or the older `registry.ctx`) exist. Anything else in `.cortex` belongs to CorTeX and
may be deleted or rewritten at any time.

### 2.2 Which files are notes

- A file is a *candidate* if it matches one of the `allowed` patterns (default `*.ctx` and `*.tex`) and none of the `ignored` patterns
  (default: hidden files, `.*`). `.cortex` and the output folder are never candidates.
- A candidate is a *note* only if it has a header containing a `title:` line ([§3](#3-the-note-file)). Candidates without one are
  skipped. These are usually `\input` fragments rather than mistakes, so they appear only in the build's summary count and are
  listed in the build log ([§13.4](#134-build-messages)).
- Other files in the project (images, packages, `\input` fragments, bibliographies) are not notes, but notes may use them as
  ordinary LaTeX would ([§6.4](#64-other-files-a-note-uses)).
- `.ctx` is the usual extension for notes: they are LaTeX notes exactly like `.tex` ones, named so that editors can tell a note from
  plain LaTeX ([§13.7](#137-the-language-server)). New notes made from the viewer are `.ctx` files. A project made before `.ctx`
  existed keeps the `allowed` patterns it saved, so add `*.ctx` to them to use it.

### 2.3 Note identity

Every note has:

- an **id**: its path relative to the root without the extension, lowercased, with every character other than letters, digits,
  `/`, `-` and `_` replaced by `-` (`Physics/Euler's Identity.tex` → `physics/euler-s-identity`). Two files (`euler.ctx` and `euler.tex` among them) whose paths give the
  same id (differing only in case or punctuation) stop the build with an error. The id is how the viewer addresses a note, and is
  stable as long as the file isn't moved or renamed.
- **names**: its title and each of its aliases, compared loosely ([§4.1](#41-matching-names)). Names are how notes link to one
  another; they need not be unique.

---

## 3. The note file

A note is split into sections by *separator lines*: a line of five or more `-` (or five or more `=`), ignoring surrounding spaces.
Only the first two separators are structural.

| Separators | Sections |
|---|---|
| none | body only (no header, so not a note) |
| one | header / body |
| two or more | header / local preamble / body (later separators are ordinary body text) |

```
title: Euler's Identity
alias: Euler identity, e^(i pi)
todo: needs a proof
-----
\usepackage{tikz}
-----
The most beautiful equation is [[Euler's formula]] evaluated at $\pi$.
```

- **Local preamble**: LaTeX placed after the project preamble when compiling this note (and any note that transcludes it,
  [§5.2](#52-transclusion)).
- **Body**: the note's content, in LaTeX plus CorTeX syntax ([§4](#4-links-and-transclusions), [§7](#7-extended-syntax)).

### 3.1 Header fields

One `key: value` per line; keys are case-insensitive; lines without a colon are ignored. If a key appears more than once, the
first occurrence wins (except where a field takes a list, below).

| Key | Value | Meaning |
|---|---|---|
| `title` | text | **Required.** The note's display title, and one of its names. It is LaTeX, typeset as the note's heading, so `&` is written `\&`; the viewer (graph, sidebar, search, tab) shows it as plain text: `\&` as `&`, `--` as `–`, `\emph{x}` as `x` |
| `alias`, `aliases` | comma-separated list | Further names the note can be linked by |
| `redirect` | a name | Makes this note a *stub* which forwards to another note ([§5.3](#53-redirects)) |
| `graph` | comma-separated: `hidden`, `loose` | How the note appears on graphs ([§9](#9-the-graph)) |
| `show-title` | `yes`/`no` (`true`/`false`, `on`/`off`) | Overrides the project's `insert-title` setting for this note |
| `todo` | optional text | Flags the note as needing work, with an optional reason. `yes`/`true`/`on` or empty flag it without a reason; `no`/`false`/`off` don't flag it |
| `sidebar` | comma-separated list of `target` or `target\|Label` | Extra sidebar links shown while this note is open |

Unknown values for `graph` and `show-title` are warned about and ignored. Unknown keys are ignored.

---

## 4. Links and transclusions

These are always available, whether or not extended syntax is on.

### 4.1 Matching names

A link target is matched against note names *loosely*: both are lowercased, and every run of characters other than letters,
digits, `-` and `_` becomes a single `-`, with none at either end. So `Euler's Identity`, `euler’s identity` and
` Euler s  identity` all match one another. This normalised form is the *slug*.

### 4.2 Syntax

Everything about a link is written inside the double brackets. Optional *markers* at the start of the brackets set its kind and
strength; they may come in any order, each at most once.

| Written | Meaning |
|---|---|
| `[[target]]` | link; shown as the target text |
| `[[target\|text]]` | link; shown as *text* (which may itself use formatting and maths) |
| `[[!target]]` | transclusion: the target's body is included here |
| `[[~target]]`, `[[~target\|text]]` | loose link ([§9.2](#92-loose-and-strong-connections)) |
| `[[+target]]` | strong link |
| `[[!~target]]`, `[[~!target]]` | loose transclusion |
| `[[!+target]]` | strong transclusion |
| `[[#label]]` | reference to the `\label{label}` in whichever note has it; shown as its number ([§4.4](#44-labels)) |
| `[[target#label]]` | reference to the `\label{label}` in the note *target* |
| `[[#label\|text]]`, `[[target#label\|text]]` | the same, shown as *text* |

(In the tables, `\|` is only Markdown escaping: the syntax is a plain `|`.)

Rules:

- A link must close on the same line. An unclosed `[[` is left as written, with a warning.
- `\[[` is never a link (so `\[[a,b]\]` stays display maths). `[ [` is two brackets.
- Links work everywhere in the body, including inside maths.
- Text inside a `%` comment is never a link, nor is anything between `% cortex off` and `% cortex on` ([§7.4](#74-switching-it-off)).
- Repeated or contradictory markers (`[[!!x]]`, `[[~+x]]`), or a link with no target, are left as written, with a warning.
- `|text` on a transclusion is ignored.
- Transclusion is of whole notes only: `[[!target#label]]` is left as written, with a warning. To include part of a note in
  several places, make that part a note of its own and transclude it into each.
- The strength markers apply to references too (`[[~#eq:euler]]` is a loose reference).
- The wiki-style `![[target]]` is **not** a transclusion: it is a literal `!` followed by an ordinary link, with a warning
  suggesting `[[!target]]`.

### 4.3 Resolution

A link resolves **from the note it was written in**, even when that text is shown inside another note by transclusion.

- If exactly one note has a matching name, the link points at it.
- If several do, the link points at the *nearest*: the one whose folder shares the most leading folders with the linking note's;
  then the shallower one; then the one with the earlier id. Every such shared name is reported as a warning on each build.
- If none do, the link is *unresolved*, and its target is a *wanted note* ([§5.4](#54-wanted-notes)).

Resolution depends only on the current set of notes and their names. Renaming or re-aliasing a note changes where links point
without recompiling the notes containing them.

### 4.4 Labels

A `#` reaches a place inside a note: one the author has marked with `\label`, exactly as LaTeX's `\ref` does. Headings and other
places without a label can't be reached; there is no matching by heading text.

- **Matching** is exact (case and punctuation matter), as LaTeX's labels are. Unlike names, labels are not slugs.
- **Ownership.** A label belongs to the note whose text it is written in, even when that text is shown elsewhere by transclusion.
- **`[[#label]]`** finds the note that owns the label, anywhere in the project. If several notes do (labels such as `fig:main` are
  commonly reused), it picks the nearest, by the same rule as names ([§4.3](#43-resolution)), and warns about that one reference.
  Labels shared between notes are not warned about otherwise.
- **`[[target#label]]`** resolves *target* as a note name, then looks for the label in that note only. A *target* that matches no
  note is a wanted note, as for any link.
- **Shown as** what `\ref{label}` prints in the note that owns the label (its number: "3", "2.1"), unless `|text` is given. The
  number is always current after a build.
- **Unresolved.** A label that can't be found is shown as `??` (as LaTeX does), with a warning. The reference still opens the
  target note, if there is one. A missing label never makes a wanted note.
- **Following** a reference opens the note at the label's position and briefly highlights it. In PDF mode, it opens the target
  file at that position.
- On the graph, a reference is a link to the note that owns the label, with the usual strength rules.
- **On the same page.** If the label appears on the page being shown (written in the note itself, or in text transcluded into it),
  the reference is local: it shows the number from this page and scrolls within it, exactly as `\ref{label}` would. Only a label
  that isn't on the page takes the reader to its owner. So a reference never shows one number beside an equation numbered
  differently.

#### Labels and transclusion

CorTeX extends LaTeX's syntax; it never changes what LaTeX's own commands do. Transcluded text is compiled as part of the page that
includes it, so:

- its equations, figures and sections are numbered by the including page's counters, continuing in order (an equation that is (2)
  in its own note may be (5) where it is transcluded);
- its labels join the including page's labels. Plain `\label` and `\ref` behave exactly as LaTeX makes them: if two notes shown on
  one page use the same label (or one note is transcluded twice), LaTeX reports multiply-defined labels and `\ref` picks one;
- elsewhere in the project, a label still belongs to the note it was written in: `[[#label]]` from a page that doesn't show it
  goes to that note, with that note's number, and a label is never counted twice because it is also shown inside another note.

LaTeX's warnings about labels (multiply defined, or referenced but never defined) are reported in the build output, naming the note
whose page they arose in ([§13.4](#134-build-messages)).

---

## 5. How notes relate

### 5.1 Links

A resolved link in the viewer opens its target. A link to a stub arrives at the stub's final destination. An unresolved link is
styled differently, and opens the wanted note's page.

Every note records its **backlinks**: the notes that link to or transclude it (excluding itself), regardless of strength.

### 5.2 Transclusion

`[[!target]]` includes the target's body at that point, compiled as part of the including note.

- The included body behaves as if written in place, except that it keeps its own frame of reference: its links resolve from its own
  note ([§4.3](#43-resolution)), and its relative file paths (`\includegraphics`, `\input`) resolve from its own folder. A note
  looks the same wherever it is included.
- The target's local preamble is added to the including note's preamble. Lines already present in the project preamble or the
  including note's local preamble are not repeated; any other conflict is an ordinary LaTeX error.
- Transclusion nests: a transcluded note's own transclusions are included too.
- A transclusion that would include a note inside itself, directly or through a chain, is not performed. Instead the text
  "circular transclusion" is shown in its place, and a warning names the line where it was written.
- A transclusion with no matching note shows "missing note: *target*" in its place.
- Transcluding a stub includes the note the stub finally forwards to.
- Changing a transcluded note recompiles every note that includes it.
- The note's title is not included, only its body.
- Its numbering and labels become part of the including page; see [Labels and transclusion](#labels-and-transclusion).

### 5.3 Redirects

A note with a `redirect:` header is a *stub*: it has no content of its own, only names.

- Its target is resolved like a link written in the stub.
- Any body text is ignored (with a warning, unless it is only comments), and its links and transclusions don't count.
- In the viewer, opening a stub goes straight to its final destination (following chains of stubs) and doesn't leave the stub in
  the browser history. A stub whose redirect matches nothing shows an explanation and what links to it; a loop of stubs shows an
  explanation. Both are warned about on every build.
- Stubs are not listed on the overview and are folded into their destination on the graph.

### 5.4 Wanted notes

An unresolved link target is a *wanted note*: a note the project refers to but doesn't yet have.

- The viewer shows a page for it, saying it doesn't exist yet and listing the notes that link to it.
- The overview lists every wanted note with its linkers.
- The graph shows wanted notes as hollow dashed nodes.
- From the wanted note's page, the author can create it ([§10](#10-acting-on-the-project-from-the-viewer)).
- The number of unresolved links is reported on each build.

---

## 6. Compilation

### 6.1 What a note compiles to

Each note is compiled on its own with XeLaTeX. The document consists of:

1. CorTeX's document class, set to the project's page width and margin;
2. the project preamble;
3. the note's local preamble, then the local preambles of everything it transcludes;
4. the note's title (if titles are inserted, [§11](#11-settings)), typeset with `\CortexTitle`;
5. the body.

All notes share one fixed text width (the `width` setting), whatever their content, so narrow content doesn't produce a narrow page.
A blank `margin` surrounds the page; anything drawn further outside the text width than this is cut off. A note produces a single
page of whatever height its content needs.

LaTeX is rerun (up to three passes) while it asks for another pass, so cross-references within a note settle. A pass still running
after 60 seconds is stopped, and the note fails with that explanation.

### 6.2 Customising the look

The author may redefine these in the project preamble:

| Command | Controls |
|---|---|
| `\CortexTitle{title}` | How the inserted title is typeset |
| `\CortexLink{origin}{slug}{text}` | How every link is typeset |
| `\CortexLinkText{text}`, `\CortexUnresolvedLink{text}` | PDF mode only: resolved and unresolved link styling |
| `\CortexTransclusionProblem{message}` | How a failed transclusion is shown |

### 6.3 Failure

A note that fails to compile never prevents other notes from building.

- If it has compiled before, its last good output is kept, and it is marked **stale**.
- If it never has, it is marked **failed** and has no output.
- Either way, the build reports a one-line diagnosis, pointing at the file and line in the author's source where the error was
  found: the note's body, its local preamble, the project preamble, or a transcluded note's body (naming the note it was
  transcluded into). Errors in other files (packages, `\input` files) are reported as LaTeX gives them.
- A missing image's diagnosis adds "does the image file exist?".
- The viewer shows the diagnosis on the note (stale) or in its place (failed), and the overview lists every problem.

### 6.4 Other files a note uses

Relative paths (`\includegraphics`, `\input`, local packages) are resolved from the folder of the note they were written in, as
they would be if it were compiled where it stands. This holds inside transcluded text too ([§5.2](#52-transclusion)).

- If any file inside the project that a note read changes, the note is recompiled. Files outside the project (the TeX installation)
  are not tracked.
- `\input` of a file that is itself a CorTeX note is warned about (its header would be typeset as text); transclusion is suggested.
- `\input` of a file containing `[[links]]` is warned about: CorTeX syntax is only processed in notes.

### 6.5 Images in the viewer

Images placed by a note (PNG, JPEG, GIF, WebP, SVG) are embedded in its output, so the viewer never depends on the project's image
files and an export is self-contained.

- An image in another format, or one that can't be found, is warned about and won't show in the viewer.
- If an image TeX placed is missing from the output (typically a package that has overridden the graphics driver), this is warned
  about.

### 6.6 When notes are recompiled

A build compiles only what is out of date. A note is recompiled if:

- its local preamble, body, or (when typeset) title changed;
- anything it transcludes changed, or where its transclusions point changed (in PDF mode, also where its links point);
- the number of a label it refers to changed, or which note that reference resolves to changed;
- a file inside the project that it read changed;
- it failed last time;
- its output is missing.

Every note is recompiled if the project preamble, the page width or margin, the output mode, the extended-syntax settings, the
title setting, or the CorTeX version changes.

Header changes (aliases, todo, sidebar, graph, redirects) and display settings recompile nothing, except notes whose transclusions
(or, in PDF mode, links) now point somewhere else. Otherwise they only update the index.

---

## 7. Extended syntax

Off by default; enabled with the `extended` setting. It applies only to the body, only outside maths, and never inside `%` comments.

### 7.1 Inline formatting

| Written | Result |
|---|---|
| `*text*` | italic |
| `**text**` | bold |
| `***text***` | bold italic |
| `_text_` | monospace |
| `__text__` | small caps |

- An opening marker must not follow a letter, digit, backslash or `}`, and must be followed by something other than a space or `}`.
  A closing marker must follow a non-space. So `\section*`, `2*3` and `\x*\x` are never formatting.
- Formatting never spans lines.
- `\*` and `\_` are always literal.

### 7.2 Headings

A line starting with `#` followed by a space is a heading: `#` section, `##` subsection, `###` subsubsection, `####` paragraph,
`#####` subparagraph. Deeper levels are warned about and use subparagraph. `#*`, `##*`, … give the unnumbered forms. The heading text
is processed like any other text.

### 7.3 Star lists

- A line starting `* ` is a list item; `** ` is a nested item; and so on.
- The list environment is the `list-type` setting (`itemize` or `enumerate`). A list starting `*ENUM ` or `*ITEM ` forces its type.
- A list ends at a blank line, or at a line that isn't a list item and is indented no further than the line that started the list.
  Lines indented further continue the current item. So a list under an indented heading ends at the next line at the heading's
  own indentation. Lines starting `\item` are left alone.
- The inside of a verbatim environment (`verbatim`, `lstlisting`, `minted`, `comment`), up to its `\end` line, is copied exactly as
  written. It has no CorTeX syntax and doesn't end a list, so a listing whose `\begin` is indented under an item stays in that item,
  however its contents are indented.

### 7.4 Switching it off

A line consisting only of a `% cortex off` comment (any number of `%`, any case and spacing) turns all CorTeX syntax off for the
rest of the note, links and transclusions included, so the text reaches LaTeX exactly as written; `% cortex on` turns it back on.
Turning it off ends any open star list.

### 7.5 Maths

Maths is recognised by its delimiters (`$…$`, `$$…$$`, `\(…\)`, `\[…\]`) and the standard maths environments (equation, align,
gather, multline, flalign, alignat, eqnarray, math, displaymath and their starred forms). Other environments are not treated as
maths. Unbalanced maths produces warnings, never errors.

### 7.6 Line numbers

Every rewrite keeps text on its own source line, so error line numbers always refer to the author's file.

---

## 8. The viewer

The default output (`index.html` in the output folder) is a single web page that runs from local files with no server and no network access.

### 8.1 Pages

| Address | Shows |
|---|---|
| `#/` | The home note (the `home` setting), or the overview if there is none |
| `#/overview` | The overview |
| `#/graph` | The project graph |
| `#/n/<id>` | A note |
| `#/l/<id>/<label>` | A note, scrolled to a label (the label's name in hexadecimal), whose line is briefly highlighted |
| `#/a/<slug>`, `#/a/<origin>/<slug>` | Follows a link: the note it resolves to, or the wanted note page |

The browser's Back and Forward work throughout; following a link or a stub doesn't leave an intermediate page in the history.

### 8.2 A note's page

- The compiled note, at the current zoom.
- A banner if the note is flagged as needing work (with its reason), and a banner with the diagnosis if it is stale.
- Under the note, if it has any connections: a *Connections* panel with the note's local graph ([§9.3](#93-a-notes-own-graph)), a
  depth choice, and a "Linked from" line listing its backlinks.
- A failed note shows only its diagnosis and its backlinks.

### 8.3 The overview

Built from the index, with no compilation:

- every note (except stubs), grouped by folder and sorted by title, with any non-ok status shown;
- the notes flagged as needing work, with their reasons;
- every wanted note, with the notes that link to it;
- every build problem, with its diagnosis.

### 8.4 Search

A search box in the sidebar filters every note by title, alias and folder as the reader types. `/` anywhere focuses it; the arrow
keys move through the results; Enter opens one; Escape clears it.

### 8.5 Zoom

Notes are drawn at the project's `zoom` times their printed size. On a note's page, a zoom control enlarges, shrinks or resets this.
The reader's choice is remembered in their browser, for every note, and never changes the project. Zoom never changes where lines
break.

### 8.6 The sidebar

From top to bottom:

1. Home, Overview (only when a home note is set) and Graph links;
2. the project name (from `registry.config`);
3. the logo (the `logo` setting), if any;
4. on a note or wanted page, the *Editor* tools ([§10](#10-acting-on-the-project-from-the-viewer));
5. search;
6. the global sidebar, then the open note's own `sidebar:` entries.

The global sidebar is written in `registry.config`, after a line `sidebar:`. Each following line is an entry, `target` or
`target|Label`; a line `[Heading]` starts a new group; blank lines and `%` lines are ignored. Entries resolve like links (the
global ones as if written at the root). An entry shows its label, else its target's title, else the text as written. An entry that
matches nothing links to the wanted page, with a warning on each build.

### 8.7 Live refresh

While a viewer tab is visible, it notices each new build and reloads the index and the current page, keeping the reader's scroll
position. An exported copy never refreshes.

### 8.8 Appearance settings

`background` sets the page colour around the notes. `zoom`, `logo`, `home` and all graph settings take effect without recompiling.

---

## 9. The graph

### 9.1 What it shows

- **Nodes**: every note (stubs folded into their destination) and every wanted note.
- **Edges**: one per pair of connected notes, drawn as an arrow from the linking note. A transclusion is drawn dashed. If a note
  both links to and transcludes another, the edge is a transclusion.
- **Size**: a node grows with its number of connections.
- **Colour**: nodes are coloured by folder, in one of three schemes (the `graph-colours` control, [§9.4](#94-the-project-graph)):
  `top`, a colour per top-level folder, shared by its subfolders; `shades`, a colour per top-level folder, with each subfolder a
  shade of it; `every`, a colour per folder. Colours are distinct until the folders outnumber the available hues (or, for
  `shades`, a folder's subfolders outnumber the shades), and are the same on every build and in every graph.
- **Marks**: a note needing work has a dashed red ring; a wanted note is a hollow dashed circle.
- A note with `graph: hidden` and all its edges are left out of every graph except its own.

### 9.2 Loose and strong connections

A connection is either strong (drawn) or loose (not drawn). Loose connections are for things a note mentions in passing rather than
relates to: every country mentions Earth, but that doesn't make it related to every other country.

- An unmarked link or transclusion is strong, unless its target (after redirects) has `graph: loose`, in which case it is loose.
- `[[~x]]` is always loose; `[[+x]]` is always strong.
- If a note refers to the same target several times, the connection is strong if any one reference is strong.
- A loose connection has no edge, no influence on the layout, and doesn't add to either node's size. It is otherwise a full link:
  it works, it appears in "Linked from", and an unresolved one is still a wanted note (which, if it has no strong connections,
  is not on the graph).

### 9.3 A note's own graph

Shown under each note: the note and everything within the chosen *depth* (1, 2 or 3 links, in either
direction, or All). Notes further away are drawn fainter. The depth choice is remembered in the reader's browser.

At depths 1 to 3, only those notes (and the links between them) are laid out, so nothing beyond the depth is drawn or pushes on
them. At All, the graph is the project graph itself, laid out exactly as on the graph page, so every note is where it is there. Either
way the note is laid out like any other, but the view stays centred on it. Whatever the depth, the graph opens zoomed to fit the note's
immediate neighbours (those one link away) around it; the reader zooms or pans out to see the rest.

### 9.4 The project graph

All notes. If the project has a home note, it is fixed at the centre.

Layout and colour are governed by four controls. The project's settings give their defaults; the graph page lets the reader change them,
and the reader's choices are remembered in their browser (and never change the project), with a way to return to the project's
defaults. Where the viewer can act on the project ([§10](#10-acting-on-the-project-from-the-viewer)), the reader can also save their
choices as the project's settings; their browser then forgets each choice the project has taken on.

| Control (setting) | Effect |
|---|---|
| `graph-layout` | `centre`: every note is drawn towards the middle. `rings`: each folder has a place of its own, like a sunburst chart: top-level notes in the middle, top-level folders on a ring around them, each subfolder further out at the same angle as its parent. A folder's share of the circle grows with its contents |
| `folder-pull` | 0 to 1: how strongly linked notes in different folders draw together, relative to notes in the same folder (1 ignores folders). With `rings`, also how far each folder is drawn from its place towards the middle (1 is the same as `centre`) |
| `folder-repulsion` | 0 or more: how much harder notes in different folders push apart, per step through the folder tree between them (0 treats all notes alike) |
| `graph-colours` | `top`, `shades` or `every`: how notes are coloured by folder ([§9.1](#91-what-it-shows)). It applies to a note's own graph too |

With `rings`, a wanted note is placed with the folder most of its linkers are in.

### 9.5 Interacting with a graph

- Hovering a node highlights it and its neighbours, and colours its edges by direction: outgoing green, incoming red.
- Clicking a node opens it (or the wanted page).
- Dragging a node moves it and fixes it in place.
- Scrolling zooms; dragging the background pans.
- The project graph fits itself to its box once settled; a note's own graph frames its immediate neighbours instead
  ([§9.3](#93-a-notes-own-graph)). Either way, this stops once the reader zooms or pans.
- Layout is the same each time the same project is shown.

---

## 10. Acting on the project from the viewer

The viewer can ask CorTeX to perform four actions, through `cortex://` links handled by the `cortex` program on the author's
machine. These appear only in the author's own copy, only on Linux ([§13.5](#135-platforms)),
and only once the handler is installed (`cortex install-handler`).

| Action | Where | Effect |
|---|---|---|
| **Edit** | a note (in the sidebar's *Editor* section) | Opens the note's source file in an editor |
| **Mark / Clear needs work** | a note | Adds a bare `todo:` header line, or removes every `todo:` line. Nothing else in the file changes |
| **Save as project defaults** | the graph page | Writes the graph controls' four values into the project's settings ([§9.4](#94-the-project-graph)). Nothing is recompiled |
| **Create & edit** | a wanted note | Writes a new note `<slug>.tex` beside the note whose link led there (or the first note wanting it), with `title: <slug>` and `alias: <slug>` (so links still find it once retitled), rebuilds, and opens it |

**Editors** come from the `editors` setting: named commands, e.g. `[nvim: neovide {file}, code: code -g {file}]`. If there are several,
the reader picks one (remembered per project in their browser); the first is the default. In a command, `{file}` is the note's full
path, `{dir}` its folder and `{root}` the project root. A command without `{file}` has the path added at the end, with a warning
when the settings are read. Commands are run directly, not through a shell: quotes group words, nothing else is interpreted, and
substituted paths are always single arguments.

**Safety.** Any web page could construct a `cortex://` link, so every link is treated as hostile:

- a link names only a project root, a note (or a folder and slug), an editor *name*, or graph settings; commands only ever come from the project's
  own settings;
- the root must be an absolute path to a CorTeX project; the note must be a `.tex` file with a note header, inside the root, not
  reached through anything that leads outside it;
- saving graph settings changes only `graph-layout`, `graph-colours`, `folder-pull` and `folder-repulsion`, each to a valid value
  (an invalid one refuses the whole link), and nothing else in the settings;
- create only writes a note the project currently wants, with a file name made only of the slug's safe characters, into an existing
  folder inside the project, and never overwrites a file;
- a link that breaks any rule does nothing.

Failures are reported as a desktop notification, since there is no terminal to report to.

After a todo change, a creation or a save of graph settings, the project is rebuilt (a running watch is asked to do it), so an open viewer refreshes.

---

## 11. Settings

Stored in `.cortex/settings.config`, one `key value` per line; lists are written `[a, b, c]`. Changed with `cortex settings` or by
editing the file. Invalid values are warned about and replaced by the default.

| Setting | Default | Meaning | Recompiles? |
|---|---|---|---|
| `output` | view | The output folder: relative to the root, or absolute. CorTeX owns it entirely, and deletes anything in it which it didn't write | — |
| `allowed` | `[*.ctx, *.tex]` | Patterns for candidate note files | — |
| `ignored` | `[.*]` | Patterns never considered, even if allowed | — |
| `home` | empty | The note the viewer opens on (path relative to the root); empty or invalid opens the overview, with a warning if invalid | no |
| `editors` | empty | Named editor commands ([§10](#10-acting-on-the-project-from-the-viewer)) | no |
| `pdf` | false | Produce PDFs instead of the viewer ([§12](#12-pdf-output)) | all |
| `width` | 25 | Text width of every note, in cm | all |
| `margin` | 8 | Blank margin around every note, in pt | all |
| `insert-title` | true | Typeset each note's title at its top | all |
| `extended` | false | Enable the extended syntax ([§7](#7-extended-syntax)) | all |
| `list-type` | itemize | Default environment for star lists (`itemize` or `enumerate`) | all |
| `zoom` | 1.4 | Viewer's default note size, relative to printed size | no |
| `background` | empty | Viewer page colour, any CSS colour | no |
| `logo` | empty | Image shown at the top of the sidebar (PNG, JPEG, GIF, WebP or SVG; path relative to the root) | no |
| `graph-layout` | centre | `centre` or `rings` ([§9.4](#94-the-project-graph)) | no |
| `folder-pull` | 0.5 | 0 to 1 ([§9.4](#94-the-project-graph)) | no |
| `folder-repulsion` | 0 | 0 or more ([§9.4](#94-the-project-graph)) | no |
| `graph-colours` | top | `top`, `shades` or `every` ([§9.1](#91-what-it-shows)) | no |
| `format-indent` | tab | One level of indentation for `cortex format`: `tab`, or a number of spaces ([§13.6](#136-formatting)) | no |
| `format-headings` | 3 | The deepest heading level whose contents are indented (1 = section … 5 = subparagraph; 0 = none) | no |
| `format-environments` | true | Indent the contents of environments | no |
| `format-braces` | true | Indent the lines inside a brace left open at the end of a line | no |
| `format-verbatim` | `[verbatim, lstlisting, minted, comment]` | Environments whose contents are left exactly as written | no |

When `cortex settings` changes a setting that recompiles every note, it says so and offers to rebuild. Declining discards the
change.

---

## 12. PDF output

With `pdf` on, each note compiles to a standalone PDF in the output folder, at the same relative path as its source (`a/b.tex` →
`view/a/b.pdf`). There is no viewer.

- A resolved link opens the target's PDF (in readers that support links between files). Links through stubs go to the final
  destination.
- An unresolved link is styled in red and goes nowhere.
- Transclusion, failure handling and incremental builds work as for the viewer.
- Switching modes removes the other mode's output.

---

## 13. Commands

`cortex <command> [options]`. One command at a time; no command means `build`.

| Command | Behaviour |
|---|---|
| `new` | Asks for a project name and creates a project in the current folder, optionally editing the settings first. Refuses if one already exists |
| `build` | Brings the output up to date ([§6.6](#66-when-notes-are-recompiled)), then exits |
| `rebuild` | Recompiles every note from scratch. The previous output stays in place until the new one is complete |
| `watch` | Builds, then rebuilds whenever anything in the project changes (one build per burst of saves). A failing build is reported and watching continues. Stops on `q` + Enter, or `cortex shutdown`. `settings` + Enter edits the settings exactly as `cortex settings` does, while the watch waits: changes and requests that arrive meanwhile are handled once the menu closes |
| `shutdown` | Stops the running watch or build on this project |
| `settings` | Edits the project settings interactively |
| `export -to <folder>` | Copies the viewer as a self-contained copy to share ([§13.3](#133-exporting)) |
| `install-handler` | Registers this `cortex` as the desktop's handler for `cortex://` links (Linux only) |
| `format [-file <note>]` | Re-indents a note in place ([§13.6](#136-formatting)); without `-file`, formats standard input to standard output |
| `lsp` | Runs the language server for editors, on standard input and output ([§13.7](#137-the-language-server)); started by the editor, not the author |
| `link -url <url>` | Performs a `cortex://` action ([§10](#10-acting-on-the-project-from-the-viewer)); run by the desktop, not the author (Linux only) |

| Option | Meaning |
|---|---|
| `-root <dir>`, `-dir <dir>` | The project root (default: the current folder) |
| `-v`, `-verbose` | Show debugging output |
| `-q`, `-quiet` | Show only warnings and errors |

### 13.1 Prerequisites

Before building, CorTeX checks that the programs it needs (XeLaTeX, and dvisvgm unless producing PDFs) can be found, and if not,
says which are missing and stops, rather than reporting every note as failed.

### 13.2 One process per project

Only one CorTeX process works on a project at a time. A `build`, `rebuild` or `export` started while another is running (typically
a watch) hands its request to that process and exits, saying so. Settings are re-read before each build, so a watch always uses the
current ones.

### 13.3 Exporting

`export` copies the built viewer into a folder of its own, for sharing:

- the copy contains no path on the author's machine and no Editor tools, and never refreshes;
- the destination must be new, empty, or an earlier export (which is replaced entirely); it may not be the root, the output folder or `.cortex`;
- the project is brought up to date first.

### 13.4 Build messages

Every build writes a full account to the build log, `.cortex/build.log`, replacing the previous one: each note compiled, each
failure with its diagnosis, and every warning in full (with file and line where it belongs to the author's source), including
shared names, broken redirects and sidebar entries, skipped candidates, LaTeX's label warnings, how many notes need work and how
many links are unresolved.

The terminal is kept quiet: it shows progress, one line per failed note with its diagnosis, and a short summary of everything else
by kind and count (e.g. "12 warnings, 3 shared names, 40 unresolved links"), pointing at the build log for the details.

### 13.5 Platforms

CorTeX builds, watches and exports on Linux, macOS and Windows, and the viewer works in any modern browser. Acting on the project
from the viewer ([§10](#10-acting-on-the-project-from-the-viewer)) is Linux only: elsewhere, the viewer has no Editor tools at all,
and `install-handler` and `link` say they aren't supported. The language server works on every platform.

### 13.6 Formatting

`cortex format` re-indents a note by its structure, using the project's `format-*` settings ([§11](#11-settings)). Outside a
project it uses the defaults. Editors call it through the language server.

- The header and the separators are left exactly as written. The local preamble and the body are re-indented: each line's
  indentation and trailing whitespace are replaced.
- Each line gets one level for every heading it is under (up to `format-headings`), every open environment, every brace left open
  at the end of an earlier line, and open `\[` display maths. Headings count both as sectioning commands (`\section`,
  `\subsection*`, …) and, with extended syntax, as `#` lines; a heading closes those at its own level or deeper. A line starting
  with closers (`}`, `\end{…}`, `\]`) sits at the level they close to.
- Star list items get one extra level per star beyond the first. Lines continuing an item go one level deeper than it. A line
  counts as a continuation only if it is indented further than the list, as [§7.3](#73-star-lists) requires, so formatting never
  changes what a note compiles to.
- `format-verbatim` environments, and everything between `% cortex off` and `% cortex on`, are copied as written.
- Blank lines stay blank, and line endings (`\n` or `\r\n`) are kept. Formatting a formatted note changes nothing.

### 13.7 The language server

`cortex lsp` speaks the Language Server Protocol, so an editor can use what CorTeX knows about the project. `editor/nvim` sets it up
for Neovim. It offers:

| Feature | What it does |
|---|---|
| Completion | Note names (titles and aliases) inside `[[`; labels after `#` (the target's, or every note's for `[[#`); names on `redirect:` and `sidebar:` header lines |
| Definition | The note a link or transclusion reaches (through redirects), or the `\label` a label reference names |
| Hover | A link's target: title, path, aliases, why it needs work, any redirect; or that it is a wanted note |
| References | Every `[[…]]` in the project which reaches the note under the cursor or, away from a link, the current note |
| Formatting | [§13.6](#136-formatting) |
| Link colours | Semantic tokens: the target (and label) of every `[[…]]` in the body, marked `resolved` if it reaches a note (or label) and `unresolved` if not (a wanted note, a missing label, or malformed), with modifiers for `transclusion`, `label`, `loose` and `strong`. Re-sent after every build, since a new note can make a link resolve |
| Diagnostics | Every problem from the build log which belongs to a file, at its line. A LaTeX failure goes in the file LaTeX stopped in, which may be a transcluded note |

Answers use the text the editor has open, saved or not, and the rest of the project as last read.

**One process per project ([§13.2](#132-one-process-per-project)).** If no other CorTeX process holds the project, the server
holds it and acts as its watch. It builds, rebuilds on every change, serves other commands' requests, and the viewer refreshes as
usual. If another process holds it (a watch in a terminal, or a second editor), the server only reads the project when it
changes, never compiling. Its diagnostics then cover everything except LaTeX's own errors, and it says so in the editor's log.

---

## 14. Open questions

None at present.

## 15. Not yet built

Nothing at present.

---

## 16. Room to grow

Today every note is a `.tex` file. Three further kinds of note are expected, and nothing CorTeX does should make them hard to add.
None is designed in detail; what follows is their intended shape, and the principles the rest of this document must keep to so they
fit.

### 16.1 Principles

1. **A note is anything CorTeX can turn into a LaTeX body, with a title and the other header fields.** How a file provides those
   depends on its kind; everything after that is the same for every kind. Links, transclusion, labels, redirects, the graph, the
   viewer, the Editor tools, incremental builds and failure handling ([§4](#4-links-and-transclusions)–[§10](#10-acting-on-the-project-from-the-viewer))
   never ask what kind of file a note came from.
2. **The kind of a file is given by its name, by convention rather than configuration.** The final extension is the *format*,
   which CorTeX understands (`.tex`, `.json`, image formats). An optional middle extension, letters only, is a *type*: the name of
   an environment or macro the author defines in the preamble (`pythagoras.theorem.tex`). Anything else between the stem and the
   extension is part of the name (`notes-v1.2.tex` has no type).
3. **New types need no change to CorTeX**, only a definition in the preamble. An undefined type is an ordinary LaTeX error in the
   notes that use it.
4. **New formats change only how a file is read.** Adding one never changes what a note is, or how notes relate.
5. **Every format can carry every header field** ([§3.1](#31-header-fields)), in whatever way suits it, so no kind of note is a
   lesser citizen: each can have aliases, be flagged as needing work, be loose on the graph, and so on.
6. **Transcluding a note includes its body as its format produces it**, so any kind of note can be transcluded into any other.

### 16.2 Expected kinds

| File | Format, type | Becomes |
|---|---|---|
| `euler.tex` | LaTeX | the body as written (today's notes) |
| `pythagoras.theorem.tex` | LaTeX, `theorem` | the body wrapped in `\begin{theorem}` … `\end{theorem}` |
| `gorgenthar.statblock.json` | data, `statblock` | the data's values, then `\statblock`, which lays them out |
| `heaven.png` | image | a page showing the image; transcluded, the image itself |

**Wrapped notes.** The same as LaTeX notes, with the body placed inside the type's environment. Useful when many notes share one
form (theorems, definitions, recipes) and should all change together.

**Data notes.** For structured material written once and typeset many ways (monster statistics, character sheets, tables of
constants). The file holds data, not LaTeX:

- each value becomes available to the type's macro by name (`\CortexGet{hp}`); values are LaTeX source, so may contain markup and
  maths; lists and nested values are reachable by numbered and dotted names (`actions.1.name`), with a count for each list;
- the values of one note never leak into another, even when one is transcluded into the other;
- header fields are ordinary values (`title`, `alias`, `todo`, …);
- the type's macro does all the layout, so changing it in the preamble restyles every note of that type.

**Image notes.** An image in the project becomes a note of its own: it can be linked to, appears on the graph, and has a page in the
viewer showing the image. Transcluding it places the image. Its title comes from its file name unless given another way.

