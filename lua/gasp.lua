-- #!/usr/bin/env lua
-- Simple example that shows how to change font when drawing widgets.
-- Uses the GLFW/OpenGL rear shipped with MoonNuklear.

function trace()
	return print(debug.traceback())
end
debug.sethook(trace,"*")

function tointeger(val)
	return math.floor(tonumber(val) or 0)
end

gl = require("moongl")
glfw = require("moonglfw")
nk = require("moonnuklear")
rear = require("moonnuklear.glbackend")

GUI = {
	cfg = require("cfg"),
	--[[ ID Counter, since nuklear insists on unique IDs
	Once used increment by 1 ready for next usage]]
	idc = 0,
	which = 1
}
local R, G, B, A

local function set_bgcolor(color)
   GUI.cfg.colors.bg = color or GUI.cfg.colors.bg
   return table.unpack(GUI.cfg.colors.bg)
end
R, G, B, A = set_bgcolor()

-- GL/GLFW inits
glfw.version_hint(3, 3, 'core')

function get_font(gui,name)
	if type(name) ~= "string" or not gui.fonts[name] then
		name = gui.cfg.font.use
	end
	return gui.fonts[name]
end

GUI.global_cb = function (window, key, scancode, action, shift, control, alt, super)
   if key == 'escape' and action == 'press' then
      glfw.set_window_should_close(window, true)
   end
   rear.key_callback(window, key, scancode, action, shift, control, alt, super)
end

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
	if isparent ~= true then
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
local function bytes2int(bytes,size)
	local int = 0
	local i = size
	while i > 0 do
		int = int << 8
		int = int | (bytes[i] or 0)
		i = i - 1
	end
	return int
end

local function draw_all(gui,ctx)
	local push_font = (gui.cfg.font.use ~= "default")
	if push_font == true then
		nk.style_push_font( ctx, get_font(gui) )
	end
	if nk.window_begin(ctx, "Show",
		{0,0,gui.cfg.window.width,gui.cfg.window.height}, nk.WINDOW_BORDER
	) then
		gui = gui.draw[gui.which].func(gui,gui.ctx,1)
	end
	nk.window_end(ctx)
	if push_font == true then
		nk.style_pop_font(ctx)
	end
	gl.viewport(0, 0, gui.cfg.window.width, gui.cfg.window.height)
	gl.clear_color(R, G, B, A)
	gl.clear('color')
	rear.render()
	glfw.swap_buffers(gui.window)
	collectgarbage()
	return gui
end

local function boot_window(gui)
	-- Forces reset
	GUI.window = glfw.create_window(
		GUI.cfg.window.width,
		GUI.cfg.window.height,
		GUI.cfg.window.title )
	glfw.make_context_current(GUI.window)
	gl.init()

	-- Initialize the rear
	GUI.ctx = rear.init(GUI.window, {
		vbo_size = GUI.cfg.sizes.gl_vbo,
		ebo_size = GUI.cfg.sizes.gl_ebo,
		anti_aliasing = GUI.cfg.anti_aliasing,
		clipboard = GUI.cfg.clipboard,
		callbacks = true
	})

	atlas = rear.font_stash_begin()
	for name,size in pairs(GUI.cfg.font.sizes) do
		size = GUI.cfg.font.base_size * size
		GUI.fonts[name] = atlas:add( size, GUI.cfg.font.file )
		-- Needed for memory editor later
		GUI.fonts["mono-" .. name] =
			atlas:add( size, GUI.cfg.font.mono )
	end
	rear.font_stash_end( GUI.ctx, (get_font(GUI)) )
	
	glfw.set_key_callback(GUI.window,GUI.global_cb)
	
	collectgarbage()
	collectgarbage('stop')
	
	while not glfw.window_should_close(GUI.window) do
		GUI.cfg.window.width, GUI.cfg.window.height =
			glfw.get_window_size(GUI.window)
		glfw.wait_events_timeout(1/(GUI.cfg.fps or 30))
		rear.new_frame()
		GUI.idc = 0
		GUI = draw_all(GUI,GUI.ctx)
	end
	rear.shutdown()
	
	GUI.window = nil
	GUI.ctx = nil
	atlas = nil
	return GUI
end

GUI.selected = {}
GUI.reboot = true
while GUI.reboot == true do
	local tmp = os.getenv("PWD") or os.getenv("CWD") or "."
	tmp =  tmp .. "/lua"
	GUI.draw = {}
	GUI.fonts = {}
	GUI.draw[1] = {
		desc = "Main",
		func = dofile( tmp .. "/main.lua" )
	}
	GUI.draw[2] = {
		desc = "Change Font",
		func = dofile( tmp .. "/cfg/font.lua")
	}
	GUI.draw[3] = {
		desc = "Hook App",
		func = dofile( tmp .. "/cfg/proc.lua")
	}
	GUI.draw[4] = {
		desc = "Scan Memory",
		func = dofile( tmp .. "/scan.lua")
	}
	GUI.draw[5] = {
		desc = "Load Cheatfile",
		func = dofile( tmp .. "/cfg/cheatfile.lua")
	}
	GUI.reboot = gasp.set_reboot_gui(false)
	GUI = boot_window()
	GUI.draw = nil
	GUI.fonts = nil
end
