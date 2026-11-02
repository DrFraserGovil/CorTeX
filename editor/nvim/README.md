# CorTeX for Neovim

Editing support for CorTeX notes (Neovim 0.11 or later):

- **Filetype.** `.ctx` files, and `.tex` files inside a CorTeX project, get the `cortex` filetype. Your project's own files in
  `.cortex/` (the preamble and so on) stay plain LaTeX. Treesitter parses notes with its LaTeX parser, so TeX highlighting works as
  usual. Because the filetype isn't `tex`, texlab no longer attaches to notes, so it doesn't compile fragments, run chktex or run
  latexindent on them.
- **The language server** (`cortex lsp`), started for any note:
  - completion of note names after `[[`, labels after `#`, and names on `redirect:` and `sidebar:` header lines;
  - go to definition (`gd`, or `<C-]>`) on links, transclusions and label references;
  - hover (`K`) for a link's target;
  - references (`grr`): every link to the note under the cursor, or to the current note;
  - diagnostics from each build;
  - build progress while notes compile (`maths/euler.ctx (3/12)`), shown wherever your setup shows LSP progress (noice, fidget,
    or `vim.lsp.status()` in a statusline);
  - formatting;
  - **link colours**: each link's target is cyan if it reaches a note (or label) and red if it doesn't (a wanted note, a missing
    label, a malformed link). Links are re-coloured as you type, and after every build.
- **`:CortexNote a/b/chocolate`** opens `a/b/chocolate.ctx` (relative to the working directory, like `:edit`; `.ctx` is added unless
  the path ends in `.ctx` or `.tex`). A new note gets its header: `title: Chocolate` (from the file name, with `-` and `_` as spaces
  and each word capitalised), an empty `alias:` line and the separator. Nothing is written until you save, and missing folders are
  made then. An existing note is just opened. `:CortexNote!` takes the path from the current file's folder instead, so
  `:CortexNote! ../chocolate` makes a note in the folder above the current note's.
- **Format on save**, using `cortex format`'s rules and your project's `format-*` settings.
- **Highlighting** of CorTeX's own syntax on top of treesitter: the header, separators, `[[markers target#label|text]]`,
  `#` headings and star lists (when the project uses extended syntax), and `% cortex off` regions.

## Setup

`cortex` must be on your `PATH`, or give its full path as `cmd`. With lazy.nvim:

```lua
{
	dir = "~/Documents/Personal/cortex/editor/nvim",
	ft = { "cortex", "tex" },
	config = function()
		require("cortex").setup({
			-- every option, with its default:
			cmd = { "cortex", "lsp" },
			format_on_save = true,
			tex_in_projects = true, -- .tex notes in a project are cortex notes too
			highlight = true,
			link_colours = { resolved = "#56b6c2", unresolved = "#e06c75" }, -- false: leave them to your colour scheme
		})
	end,
},
```

`ft` loads the plugin lazily. The filetype rule for `.ctx` only exists once the plugin has loaded, so if a `.ctx` file is the first
file you open, use `lazy = false` instead.

## Building and watching

The server *is* a watch. When Neovim opens a note and no other CorTeX process is working on the project, the server builds the
project, rebuilds it on every change, and refreshes the viewer, exactly as `cortex watch` does. `cortex build`, `cortex shutdown`
and the viewer's buttons talk to it as they would to a watch.

If a `cortex watch` is already running in a terminal, or another Neovim has the project open, the server only reads the project.
It never compiles, so it reports the build's problems except LaTeX's own errors. It says so in `:LspLog`.

## Colours

Every highlight group links to a standard treesitter group, so your colour scheme applies. To change one, set it after your colour
scheme loads:

| Group | Default link | |
|---|---|---|
| `CortexHeaderKey`, `CortexHeaderValue` | `@property`, `@string` | `title: …` lines |
| `CortexSeparator` | `@punctuation.delimiter` | `-----` |
| `CortexBracket`, `CortexMarker` | `@punctuation.bracket`, `@operator` | `[[ ]]`, `! ~ +` |
| `CortexTarget`, `CortexLabel`, `CortexText` | `@markup.link`, `@constant`, `@markup.link.label` | `target#label\|text` |
| `CortexHeading`, `CortexBullet` | `@markup.heading`, `@markup.list` | `# …`, `* …` |
| `CortexOff` | `@comment` | inside `% cortex off` |
| `CortexLinkResolved`, `CortexLinkUnresolved` | cyan, red (`link_colours`) | a link's target, once the server is attached |

The server's own token groups are also there to override: `@lsp.typemod.link.resolved.cortex`, `@lsp.typemod.link.unresolved.cortex`,
and `@lsp.mod.<modifier>.cortex` for `transclusion`, `label`, `loose` and `strong`. For example, to underline loose links:
`vim.api.nvim_set_hl(0, "@lsp.mod.loose.cortex", { underdotted = true })`.

## Checking it works

Open a note and run `:set ft?`, which should print `cortex`. Then run `:checkhealth vim.lsp`, which should show the `cortex` client
attached. Type `[[` to see note names.
