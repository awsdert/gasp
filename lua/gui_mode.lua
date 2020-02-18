gl = require("moongl")
glfw = require("moonglfw")
nk = require("moonnuklear")
rear = require("moonnuklear.glbackend")

GUI = {
	cfg = require("cfg"),
	--[[ ID Counter, since nuklear insists on unique IDs
	Once used increment by 1 ready for next usage]]
	idc = 0,
	which = 1,
	previous = 1
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
		gui = gui.draw[gui.which].func(gui,gui.ctx,gui.which,gui.previous)
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

local function boot_window()
	local gui = _G['GUI']
	-- Forces reset
	gui.window = glfw.create_window(
		gui.cfg.window.width,
		gui.cfg.window.height,
		gui.cfg.window.title )
	_G['main_window'] = GUI.window
	glfw.make_context_current(gui.window)
	gl.init()

	-- Initialize the rear
	gui.ctx = rear.init(gui.window, {
		vbo_size = gui.cfg.sizes.gl_vbo,
		ebo_size = gui.cfg.sizes.gl_ebo,
		anti_aliasing = gui.cfg.anti_aliasing,
		clipboard = gui.cfg.clipboard,
		callbacks = true
	})

	atlas = rear.font_stash_begin()
	for name,size in pairs(gui.cfg.font.sizes) do
		size = gui.cfg.font.base_size * size
		gui.fonts[name] = atlas:add( size, gui.cfg.font.file )
		-- Needed for memory editor later
		gui.fonts["mono-" .. name] =
			atlas:add( size, gui.cfg.font.mono )
	end
	rear.font_stash_end( gui.ctx, (get_font(gui)) )
	
	glfw.set_key_callback(gui.window,gui.global_cb)
	
	collectgarbage()
	collectgarbage('stop')
	
	while not glfw.window_should_close(gui.window) do
		gui.cfg.window.width, gui.cfg.window.height =
			glfw.get_window_size(gui.window)
		glfw.wait_events_timeout(1/(gui.cfg.fps or 30))
		rear.new_frame()
		gui.idc = 0
		gui = draw_all(gui,gui.ctx)
	end
	rear.shutdown()
	
	_G['main_window'] = nil
	gui.window = nil
	gui.ctx = nil
	atlas = nil
	return gui
end

GUI.draw_reboot = function(gui,ctx)
	nk.layout_row_dynamic( ctx, pad_height(get_font(gui),text), 2)
	if nk.button( ctx,nil, "Reboot GUI" ) then
		gui.reboot = gasp.toggle_reboot_gui()
	else
		gui.reboot = gasp.get_reboot_gui()
	end
	nk.label( ctx, tostring(gui.reboot), nk.TEXT_RIGHT )
	return gui
end

GUI.use_ui = function(gui,ctx,name,now)
	local i,v
	for i,v in pairs(gui.draw) do
		if v.name == name then
			gui.previous = now
			gui.which = i
			return gui
		end
	end
	return gui
end
	
GUI.add_ui = function( gui, name, desc, file )
	local path = (os.getenv("PWD") or os.getenv("CWD") or ".") .. "/lua"
	local tmp = dofile( path .. "/" .. file )
	if not tmp then
		print( debug.traceback() )
		tmp = gui.draw_fallback
	end
	gui.draw[#(gui.draw) + 1] = { name = name, desc = desc, func = tmp }
	return gui
end

GUI.draw_goback = function(gui,ctx,now,prv,func)
	nk.layout_row_dynamic( ctx, pad_height(get_font(gui),"Done"), 1 )
	if nk.button( ctx, nil, "Go Back" ) then
		gui.which = prv
		if prv == gui.previous then
			gui.previous = 1
		end
		if func then
			return func(gui)
		end
	end
	return gui
end

GUI.draw_fallback = function( gui, ctx, now, prv )
	gui = gui.draw_reboot(gui,ctx)
	gui = gui.draw_goback(gui,ctx,now,prv)
	return gui
end

GUI.selected = {}
GUI.reboot = true
GUI.forced_reboot = false
while GUI.reboot == true do
	
	GUI.draw = {}
	GUI.fonts = {}
	
	GUI = GUI.add_ui( GUI, "main", "Main", "main.lua" )
	GUI = GUI.add_ui( GUI, "cfg-font", "Change Font", "cfg/font.lua" )
	GUI = GUI.add_ui( GUI, "cfg-proc", "Hook process", "cfg/proc.lua" )
	GUI = GUI.add_ui( GUI, "scan", "Scan memory", "scan.lua" )
	GUI = GUI.add_ui( GUI, "cheatfile", "Load cheat file", "cfg/cheatfile.lua" )
	
	GUI.reboot = gasp.set_reboot_gui(false)
	local ok, tmp = pcall( boot_window, gui )
	if ok then
		GUI = tmp
		GUI.forced_reboot = false
	else
		glfw.set_window_should_close(_G['main_window'], true)
		rear.shutdown()
		print( tostring(tmp) )
		if not GUI.reboot and not GUI.forced_reboot then
			-- Try again, might not be main window that caused failure
			GUI.reboot = gasp.set_reboot_gui(true)
			GUI.forced_reboot = true
			GUI.previous = 1
			GUI.which = 1
		end
	end
	GUI.draw = nil
	GUI.fonts = nil
end
