-- #!/usr/bin/env lua
-- Simple example that shows how to change font when drawing widgets.
-- Uses the GLFW/OpenGL rear shipped with MoonNuklear.

cfg = require("cfg")
gl = require("moongl")
glfw = require("moonglfw")
nk = require("moonnuklear")
rear = require("moonnuklear.glbackend")

GUI = {
	--[[ ID Counter, since nuklear insists on unique IDs
	Once used increment by 1 ready for next usage]]
	idc = 0,
	which = "main",
	draw = {},
	fonts = {}
}
local R, G, B, A

local function set_bgcolor(color)
   cfg.colors.bg = color or cfg.colors.bg
   return table.unpack(cfg.colors.bg)
end
R, G, B, A = set_bgcolor()

-- GL/GLFW inits
glfw.version_hint(3, 3, 'core')
GUI.window = glfw.create_window(
	cfg.window.width, cfg.window.height, cfg.window.title )
glfw.make_context_current(GUI.window)
gl.init()

-- Initialize the rear
GUI.ctx = rear.init(GUI.window, {
   vbo_size = cfg.sizes.gl_vbo,
   ebo_size = cfg.sizes.gl_ebo,
   anti_aliasing = cfg.anti_aliasing,
   clipboard = cfg.clipboard,
   callbacks = true
})

atlas = rear.font_stash_begin()
for name,size in pairs(cfg.font.sizes) do
	size = cfg.font.base_size * size
	GUI.fonts[name] = atlas:add( size, cfg.font.file )
end
function get_font(gui,name)
	if type(name) ~= "string" or not gui.fonts[name] then
		name = cfg.font.use
	end
	return gui.fonts[name]
end
rear.font_stash_end( GUI.ctx, (get_font(GUI)) )

glfw.set_key_callback(GUI.window,
function (window, key, scancode, action, shift, control, alt, super)
   if key == 'escape' and action == 'press' then
      glfw.set_window_should_close(window, true)
   end
   rear.key_callback(window, key, scancode, action, shift, control, alt, super)
end)

collectgarbage()
collectgarbage('stop')

function scandir(path)
    if gasp.path_isdir( path ) == 1 then
		return gasp.path_files(path)
	end
end

function pad_width(font,text)
	return math.ceil((font:width(font:height(),text)) * 1.2)
end

function pad_height(font,text)
	return math.ceil((font:height(text)) * 1.2)
end
function add_tree_node(ctx,text,selected,id,isparent)
	local ok
	if isparent == true then
		selected =
			nk.selectable(
			ctx, nil, text, nk.TEXT_LEFT,
			(selected or false) )
		return true, selected
	end
	ok, selected =
		nk.tree_element_push(
		ctx, nk.TREE_NODE, text, nk.MAXIMIZED,
		(selected or false), id )
	if ok then
		nk.tree_element_pop(ctx)
		return true, selected
	end
	return false, selected
end
local function bytes2int(bytes)
	local int = 0
	local i,v
	for i,v in pairs(bytes) do
		int = int << 8
		int = int | v
	end
	return int
end
local function draw_cheats(gui,ctx,root)
	local i, v, id, text, j, k, tmp, font
	font = get_font(gui)
	id = gui.idc
	gui.idc = gui.idc + 1
	gui.cheat = root
	if nk.tree_push( ctx, nk.TREE_NODE,
		"Cheats", nk.MAXIMIZED, id ) then
		if root.endian then
			text = "Endian"
			nk.layout_row_dynamic(ctx,pad_height(font,text),3)
			nk.label(ctx,text,nk.TEXT_LEFT)
			nk.label(ctx,root.endian,nk.TEXT_LEFT)
		end
		if root.list then
			for i,v in pairs(root.list) do
				nk.layout_row_dynamic(ctx,pad_height(font,"="),3)
				nk.label(ctx,(v.method or "="),nk.TEXT_LEFT)
				nk.label(ctx,(v.desc or "{???}"),nk.TEXT_LEFT)
				if v.signed then
					text = gui.handle:read( v.addr or 0, v.signed )
					if text then
						--[[Flip Big Endian to Little Endian,
							no native awareness yet]]
						if root.endian == "Big" then
							j,k = 1,#text
							while j < k do
								tmp = text[j]
								text[j] = text[k]
								text[k] = tmp
							end
						end
						text = "" .. (bytes2int(text))
					else text = "" end
				elseif v.bytes then
					text = gui.handle:read( v.addr or 0, v.bytes )
					if text then text = gasp.totxtbytes(text)
					else text = "" end
				else
					text = ""
				end
				nk.edit_string(ctx,nk.EDIT_SIMPLE,text,100)
			end
		end
		nk.tree_pop(ctx)
	end
	return gui
end

GUI.which = "main"
GUI.draw["main"] = function(gui,ctx)
	local font = get_font(gui)
	local text, file, dir, tmp
	text = "Change Font"
	nk.layout_row_dynamic( ctx, pad_height(font,text), 2)
	if nk.button(ctx, nil, text) then
		 -- ... event handling ...
		 gui.which = "cfg-font"
	end
	text = "Hook Process"
	if nk.button(ctx, nil, text) then
		 -- ... event handling ...
		 gui.which = "cfg-proc"
	end
	-- Slider Example with Custom width
	text = "Volume:"
	nk.layout_row_begin(ctx, 'static', pad_height(font,text), 2)
	nk.layout_row_push(ctx, pad_width(font,text))
	nk.label(ctx,text, nk.TEXT_LEFT)
	nk.layout_row_push(ctx, pad_width(font,text))
	cfg.volume = nk.slider(ctx, 0, cfg.volume, 1.0, 0.1)
	nk.layout_row_end(ctx)
	if gui.handle then
		text = "Hooked:"
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label( ctx, text, nk.TEXT_LEFT )
		nk.label( ctx, gui.noticed.name, nk.TEXT_LEFT )
		text = os.getenv("PWD") or os.getenv("CWD")
		dir = scandir( text .. "/cheats" )
		if not dir then
			text = cfg.cheatdir or os.getenv("GASP_PATH")
			dir = gasp.path_isdir(text)
			if dir == 0 then
				io.popen('mkdir "' .. text .. '/"')
			elseif dir == -1 then
				error("Couldn't access '" .. text .. '/"' )
				nk.shutdown()
				return nil
			end
			if text ~= cfg.cheatdir then
				text = text .. "/cheats"
			end
			dir = gasp.path_isdir(text)
			if dir == 0 then
				io.popen('mkdir "' .. text .. '"')
			elseif dir == -1 then
				error("Couldn't access '" .. text .. '"' )
				nk.shutdown()
				return nil
			end
			dir = gasp.path_files(text)
		end
		if #dir > 0 then
			nk.layout_row_dynamic(ctx,pad_height(font,text),2)
			nk.label(ctx, "Cheat files in:", nk.TEXT_LEFT)
			nk.label(ctx, text, nk.TEXT_LEFT)
			nk.layout_row_dynamic( ctx, pad_height(font,text),1)
			for i,v in pairs(dir) do
				if gasp.path_isfile(text .. '/' .. v) == 1 then
					if gui.cheatfile then
						i = (gui.cheatfile == v)
					else
						i = false
					end
					i = nk.selectable( ctx, nil, v, nk.TEXT_LEFT, i )
					if i == true then
						gui.cheatfile = v
					end
				end
			end
			if gui.cheatfile then
				tmp = package.path
				package.path = text .. '/?.lua;' .. tmp
				if gui.oldcheatfile == gui.cheatfile then
					gui = draw_cheats(gui,ctx,gui.cheat or
						dofile(text .. "/" .. gui.cheatfile))
				else
					gui.oldcheatfile = gui.cheatfile
					gui = draw_cheats(gui,ctx,
						dofile(text .. "/" .. gui.cheatfile))
				end
				package.path = tmp
			end
		end
	end
	return gui
end
GUI.draw["cfg-font"] = require("cfg.font")
GUI.draw["cfg-proc"] = require("cfg.proc")
local function draw_all(gui,ctx)
	local push_font = (cfg.font.use ~= "default")
	if push_font == true then
		nk.style_push_font( ctx, get_font(gui) )
	end
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	if nk.window_begin(ctx, "Show",
		{0,0,cfg.window.width,cfg.window.height},
		nk.WINDOW_BORDER | nk.WINDOW_SCALABLE
	) then
		gui = gui.draw[gui.which](gui,gui.ctx,"main")
	end
	nk.window_end(ctx)
	if push_font == true then
		nk.style_pop_font(ctx)
	end
	-- Render
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	gl.viewport(0, 0, cfg.window.width, cfg.window.height)
	gl.clear_color(R, G, B, A)
	gl.clear('color')
	rear.render()
	glfw.swap_buffers(gui.window)
	collectgarbage()
	return gui
end
GUI.selected = {}
while not glfw.window_should_close(GUI.window) do
	glfw.wait_events_timeout(1/cfg.fps)
	rear.new_frame()
	GUI.idc = 0
	GUI = draw_all(GUI,GUI.ctx)
end
rear.shutdown()
