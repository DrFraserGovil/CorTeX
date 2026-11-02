-- CorTeX for Neovim: the cortex filetype, the cortex language server, format on save, and highlighting of CorTeX's own syntax on top
-- of treesitter's LaTeX highlighting. See ../../README.md
local M = {}

M.options = {
	-- the cortex executable (on the PATH, or a full path)
	cmd = { "cortex", "lsp" },
	-- re-indent notes with `cortex format`'s rules on every save
	format_on_save = true,
	-- .tex files inside a CorTeX project are notes too (.ctx files always are)
	tex_in_projects = true,
	-- highlight [[links]], the header, # headings and star lists
	highlight = true,
	-- once the language server is attached, link targets are coloured by whether they reach a note (or label); false leaves them
	-- to your colour scheme (the groups are CortexLinkResolved and CortexLinkUnresolved)
	link_colours = { resolved = "#56b6c2", unresolved = "#e06c75" },
}

local ns = vim.api.nvim_create_namespace("cortex")

--- THE FILETYPE

local function in_project(path)
	-- the project's own files (.cortex/preamble.tex and the like) are plain LaTeX, not notes
	if path:find("/%.cortex/") then return nil end
	return vim.fs.root(path, ".cortex")
end

local function filetypes()
	vim.filetype.add({
		extension = {
			ctx = function(path)
				if path:find("/%.cortex/") then return "conf" end
				return "cortex"
			end,
			tex = function(path, bufnr)
				if M.options.tex_in_projects and in_project(path) then return "cortex" end
				return require("vim.filetype.detect").tex(path, bufnr)
			end,
		},
	})
	-- treesitter parses notes as LaTeX; CorTeX's syntax is highlighted on top (below)
	pcall(vim.treesitter.language.register, "latex", "cortex")
end

--- THE LANGUAGE SERVER

local function server()
	vim.lsp.config("cortex", {
		cmd = M.options.cmd,
		filetypes = { "cortex" },
		root_markers = { ".cortex" },
	})
	vim.lsp.enable("cortex")
end

--- HIGHLIGHTING: a decoration provider, so only the lines on screen are looked at, every redraw

local groups = {
	CortexHeaderKey = "@property",
	CortexHeaderValue = "@string",
	CortexSeparator = "@punctuation.delimiter",
	CortexBracket = "@punctuation.bracket",
	CortexMarker = "@operator",
	CortexTarget = "@markup.link",
	CortexLabel = "@constant",
	CortexText = "@markup.link.label",
	CortexHeading = "@markup.heading",
	CortexBullet = "@markup.list",
	CortexOff = "@comment",
}

-- per buffer, refreshed when it changes: where the header ends, which lines are in `% cortex off`, and the project's extended setting
local cache = {}

local function extended(root)
	if not root then return false end
	local file = io.open(root .. "/.cortex/settings.config", "r")
	if not file then return false end
	local text = file:read("*a")
	file:close()
	for line in text:gmatch("[^\n]+") do
		local key, value = line:match("^%s*(%S+)%s+(%S+)")
		if key == "extended" or key == "extended-syntax" or key == "ExtendedSyntax" then return value == "true" end
	end
	return false
end

local function separator(line)
	return line:match("^%s*%-%-%-%-%-+%s*$") or line:match("^%s*=====+%s*$")
end

local function analyse(buf)
	local tick = vim.api.nvim_buf_get_changedtick(buf)
	local known = cache[buf]
	if known and known.tick == tick then return known end
	local lines = vim.api.nvim_buf_get_lines(buf, 0, -1, false)
	local info = { tick = tick, header_end = -1, body_start = 0, off = {}, extended = known and known.extended }
	if info.extended == nil then info.extended = extended(in_project(vim.api.nvim_buf_get_name(buf))) end
	local separators = 0
	for i, line in ipairs(lines) do
		if separator(line) then
			separators = separators + 1
			if separators == 1 then info.header_end = i - 1 end
			info.body_start = i
			if separators == 2 then break end
		end
	end
	local off = false
	for i = info.body_start + 1, #lines do
		local switch = lines[i]:lower():match("^%s*%%+%s*cortex%s+(o[nf]f?)%s*$")
		if switch == "off" then off = true elseif switch == "on" then off = false elseif off then info.off[i - 1] = true end
	end
	cache[buf] = info
	return info
end

local function mark(buf, row, from, to, group)
	if to > from then
		vim.api.nvim_buf_set_extmark(buf, ns, row, from, { end_col = to, hl_group = group, ephemeral = true, priority = 110 })
	end
end

-- [[markers target#label|text]] on one line (0-based columns); never \[[ and never in a % comment
local function links(buf, row, line)
	local i = 1
	while i <= #line do
		local c = line:sub(i, i)
		if c == "\\" then
			i = i + 2
		elseif c == "%" then
			return
		elseif line:sub(i, i + 1) == "[[" then
			local close = line:find("]]", i + 2, true)
			if not close then return end
			local open = i - 1
			mark(buf, row, open, open + 2, "CortexBracket")
			mark(buf, row, close - 1, close + 1, "CortexBracket")
			local inner_start = i + 2
			local markers = line:sub(inner_start, close - 1):match("^[!~+]*")
			mark(buf, row, inner_start - 1, inner_start - 1 + #markers, "CortexMarker")
			local s = inner_start + #markers
			local bar = line:find("|", s, true)
			if bar and bar > close then bar = nil end
			local target_end = (bar or close) - 1
			local hash = line:find("#", s, true)
			if hash and hash > target_end then hash = nil end
			mark(buf, row, s - 1, (hash or target_end + 1) - 1, "CortexTarget")
			if hash then mark(buf, row, hash - 1, target_end, "CortexLabel") end
			if bar then mark(buf, row, bar, close - 1, "CortexText") end
			i = close + 2
		else
			i = i + 1
		end
	end
end

local function line_marks(buf, row, info)
	local line = vim.api.nvim_buf_get_lines(buf, row, row + 1, false)[1]
	if not line then return end
	if row < info.body_start and separator(line) then return mark(buf, row, 0, #line, "CortexSeparator") end
	if row < info.header_end then
		local key_end = line:find(":", 1, true)
		if key_end then
			mark(buf, row, 0, key_end - 1, "CortexHeaderKey")
			mark(buf, row, key_end, #line, "CortexHeaderValue")
		end
		return
	end
	if row < info.body_start then return end -- the local preamble: plain LaTeX
	if info.off[row] then return mark(buf, row, 0, #line, "CortexOff") end
	if info.extended then
		local indent, hashes = line:match("^(%s*)(#+%*?) ")
		if hashes then mark(buf, row, #indent, #line, "CortexHeading") end
		local bullet_indent, stars = line:match("^(%s*)(%*+) ")
		if not stars then bullet_indent, stars = line:match("^(%s*)(%*+[EI][NT][UE][MM]) ") end
		if stars then mark(buf, row, #bullet_indent, #bullet_indent + #stars, "CortexBullet") end
	end
	links(buf, row, line)
end

-- the colours, set again whenever the colour scheme changes (which clears them)
local function colours()
	for group, link in pairs(groups) do vim.api.nvim_set_hl(0, group, { link = link, default = true }) end
	local links = M.options.link_colours
	if links then
		vim.api.nvim_set_hl(0, "CortexLinkResolved", { fg = links.resolved, default = true })
		vim.api.nvim_set_hl(0, "CortexLinkUnresolved", { fg = links.unresolved, default = true })
	end
	-- the server's semantic tokens: one type, link, with modifiers saying whether it resolves (and what kind it is)
	vim.api.nvim_set_hl(0, "@lsp.typemod.link.resolved.cortex", { link = "CortexLinkResolved", default = true })
	vim.api.nvim_set_hl(0, "@lsp.typemod.link.unresolved.cortex", { link = "CortexLinkUnresolved", default = true })
end

local function highlighting()
	vim.api.nvim_set_decoration_provider(ns, {
		on_win = function(_, _, buf)
			if vim.bo[buf].filetype ~= "cortex" then return false end
		end,
		on_line = function(_, _, buf, row)
			line_marks(buf, row, analyse(buf))
		end,
	})
	vim.api.nvim_create_autocmd("BufWipeout", { callback = function(args) cache[args.buf] = nil end })
end

--- NEW NOTES

-- a title from a file name: dashes and underscores as spaces, each word capitalised (chocolate-cake: Chocolate Cake)
local function title_of(file)
	local words = vim.fn.fnamemodify(file, ":t:r"):gsub("[-_]+", " ")
	return vim.trim((words:gsub("(%w)(%w*)", function(first, rest) return first:upper() .. rest end)))
end

-- Opens the note at path (relative to the working directory, as :edit takes it, or with from_file to the current file's folder; .ctx
-- added unless it ends .ctx or .tex). A new one is
-- given its header, a title made from its file name, an empty alias line and the separator, but isn't saved: a note abandoned unsaved
-- never exists. Its folders are made when it's first saved. An existing file is just opened
function M.new_note(path, from_file)
	path = vim.trim(path or "")
	if path == "" then return vim.notify("CortexNote: give the note's path", vim.log.levels.WARN) end
	if not (path:match("%.ctx$") or path:match("%.tex$")) then path = path .. ".ctx" end
	path = vim.fn.expand(path)
	local current = vim.api.nvim_buf_get_name(0)
	if from_file and current ~= "" and not vim.startswith(path, "/") then path = vim.fs.dirname(current) .. "/" .. path end
	local file = vim.fs.normalize(vim.fn.fnamemodify(path, ":p"))
	local exists = vim.uv.fs_stat(file) ~= nil

	vim.cmd.edit(vim.fn.fnameescape(file))
	if exists then return end
	local buf = vim.api.nvim_get_current_buf()
	vim.api.nvim_buf_set_lines(buf, 0, -1, false, { "title: " .. title_of(file), "alias: ", "---------------", "" })
	vim.api.nvim_win_set_cursor(0, { 4, 0 })
	vim.api.nvim_create_autocmd("BufWritePre", {
		buffer = buf,
		once = true,
		callback = function() vim.fn.mkdir(vim.fs.dirname(file), "p") end,
	})
end

-- Completes :CortexNote's path from where it will be taken: the current file's folder with !, else the working directory. Folders
-- end in /, so completion carries on into them
local function complete_note(lead, line)
	local current = vim.api.nvim_buf_get_name(0)
	local bang = line:match("^[%s:]*%a+!") ~= nil
	local base = (vim.startswith(lead, "/") or vim.startswith(lead, "~")) and ""
		or ((bang and current ~= "") and vim.fs.dirname(current) or vim.fn.getcwd()) .. "/"
	local out = {}
	for _, match in ipairs(vim.fn.glob(base .. lead .. "*", false, true)) do
		local shown = base == "" and match or match:sub(#base + 1)
		if lead:sub(1, 1) == "~" then shown = vim.fn.fnamemodify(shown, ":~") end
		if vim.fn.isdirectory(match) == 1 then shown = shown .. "/" end
		table.insert(out, shown)
	end
	return out
end

--- SETUP

function M.setup(options)
	M.options = vim.tbl_deep_extend("force", M.options, options or {})
	filetypes()
	server()
	colours()
	vim.api.nvim_create_autocmd("ColorScheme", { group = vim.api.nvim_create_augroup("cortex-colours", { clear = true }), callback = colours })
	if M.options.highlight then highlighting() end
	-- :CortexNote a/b/chocolate opens a/b/chocolate.ctx; a new one gets its header, with the title Chocolate. With !, the path is from
	-- the current file's folder rather than the working directory
	vim.api.nvim_create_user_command("CortexNote", function(args) M.new_note(args.args, args.bang) end,
		{ nargs = 1, bang = true, complete = complete_note, desc = "Open a CorTeX note, with its header written if it's new" })

	local group = vim.api.nvim_create_augroup("cortex", { clear = true })
	vim.api.nvim_create_autocmd("FileType", {
		group = group,
		pattern = "cortex",
		callback = function(args)
			vim.bo[args.buf].commentstring = "% %s"
			if M.options.format_on_save then
				vim.api.nvim_create_autocmd("BufWritePre", {
					group = group,
					buffer = args.buf,
					callback = function()
						vim.lsp.buf.format({ bufnr = args.buf, name = "cortex", async = false })
					end,
				})
			end
		end,
	})
end

return M
