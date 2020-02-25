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

function get_font(name)
	if type(name) ~= "string" or not GUI.fonts[name] then
		name = GUI.cfg.font.use
	end
	return GUI.fonts[name]
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


function hook_process()
	if GUI.noticed then
		if not GUI.handle then
			GUI.handle = gasp.new_handle()
		end
		if not GUI.handle or GUI.handle:valid() == true then
			return
		end
		GUI.handle:init( GUI.noticed.entryId )
	end
end

local function draw_all(ctx)
	local push_font = (GUI.cfg.font.use ~= "default")
	local tmp
	GUI.quit = glfw.window_should_close( GUI.window )
	if not GUI.donothook then
		tmp = gasp.locate_app(GUI.cfg.find_process)
		-- Auto hook process
		if #tmp == 1 then
			GUI.noticed = tmp[1]
			hook_process()
		end
		tmp = nil
	end
	if push_font == true then
		nk.style_push_font( ctx, get_font() )
	end
	if nk.window_begin(ctx, "Show",
		{0,0,GUI.cfg.window.width,GUI.cfg.window.height},
		nk.WINDOW_BORDER
	) then
		GUI.draw[GUI.which].func(
			GUI.ctx, GUI.which, GUI.previous )
	end
	nk.window_end(ctx)
	if push_font == true then
		nk.style_pop_font(ctx)
	end
	gl.viewport(0, 0, GUI.cfg.window.width, GUI.cfg.window.height)
	gl.clear_color(R, G, B, A)
	gl.clear('color')
	rear.render()
	glfw.swap_buffers(GUI.window)
	collectgarbage()
	
end

local function boot_window()
	local tmp
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
	rear.font_stash_end( GUI.ctx, (get_font()) )
	
	glfw.set_key_callback(GUI.window,GUI.global_cb)
	
	collectgarbage()
	collectgarbage('stop')
	
	while not glfw.window_should_close(GUI.window) do
		GUI.cfg.window.width, GUI.cfg.window.height =
			glfw.get_window_size(GUI.window)
		glfw.wait_events_timeout(1/(GUI.cfg.fps or 30))
		rear.new_frame()
		GUI.idc = 0
		draw_all(GUI.ctx)
	end
	rear.shutdown()
	
	GUI.window = nil
	GUI.ctx = nil
	atlas = nil
	
end

GUI.draw_reboot = function(ctx)
	nk.layout_row_dynamic( ctx, pad_height(get_font(),text), 2)
	if nk.button( ctx,nil, "Reboot GUI" ) then
		GUI.reboot = gasp.toggle_reboot_gui()
	else
		GUI.reboot = gasp.get_reboot_gui()
	end
	nk.label( ctx, tostring(GUI.reboot), nk.TEXT_RIGHT )
	
end

GUI.use_ui = function(ctx,name,now)
	local i,v
	for i,v in pairs(GUI.draw) do
		if v.name == name then
			GUI.previous = now
			GUI.which = i
			
		end
	end
	
end
	
GUI.add_ui = function( name, desc, file )
	local path = (os.getenv("PWD") or os.getenv("CWD") or ".") .. "/lua"
	local tmp = dofile( path .. "/" .. file )
	if not tmp then
		print( debug.traceback() )
		tmp = GUI.draw_fallback
	end
	GUI.draw[#(GUI.draw) + 1] = { name = name, desc = desc, func = tmp }
	
end

GUI.draw_goback = function(ctx,now,prv,func)
	nk.layout_row_dynamic( ctx, pad_height(get_font(),"Done"), 1 )
	if nk.button( ctx, nil, "Go Back" ) then
		GUI.which = prv
		if prv == GUI.previous then
			GUI.previous = 1
		end
		if func then
			return func()
		end
	end
	
end

GUI.draw_fallback = function( ctx, now, prv )
	GUI.draw_reboot(ctx)
	GUI.draw_goback(ctx,now,prv)
end

GUI.selected = {}
GUI.reboot = true
GUI.forced_reboot = false
while GUI.reboot == true do
	GUI.draw = {}
	GUI.fonts = {}
	
	GUI.add_ui( "main", "Main", "main.lua" )
	GUI.add_ui( "cfg-font", "Change Font", "cfg/font.lua" )
	GUI.add_ui( "cfg-proc", "Hook process", "cfg/proc.lua" )
	GUI.add_ui( "scan", "Scan memory", "scan.lua" )
	GUI.add_ui( "cheatfile", "Load cheat file", "cfg/cheatfile.lua" )
	
	GUI.reboot = gasp.set_reboot_gui(false)
	GUI.window = nil
	boot_window()
	GUI.draw = nil
	GUI.fonts = nil
end
