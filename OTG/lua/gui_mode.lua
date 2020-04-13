gl = require("moongl")
glfw = require("moonglfw")
nk = require("moonnuklear")
rear = require("moonnuklear.glbackend")

function rebuild_cheat(v)
	if not v then return end
	local _t = {
		app = v.app,
		emu = v.emu,
		as_text = v.as_text,
		as_bytes = v.as_bytes,
		edited = v.edited,
		endian = v.endian,
		method = v.method or "=",
		desc = v.desc or "???",
		addr = v.addr or 0,
		size = v.size or 1,
		Type = v.Type or "bytes",
		offset = v.offset,
		-- Group only stuff
		generated = v.generated,
		list = v.list,
		prev = v.prev,
		count = v.count,
		split = v.split,
		is_group = toboolean(v.list or v.count or v.split or v.prev),
		-- Scan related
		from = v.from,
		upto = v.upto,
		done = v.done,
		limit = v.limit,
		found = v.found,
		added = v.added,
		increment = v.increment,
		TypeSelected = v.TypeSelected,
		region = v.region,
		regions = v.regions,
		region_options = v.region_options
	}
	if v.active == nil then
		_t.active = _t.is_group
	else
		_t.active = toboolean(v.active)
	end
	local mt = {
		__index = _t,
		__newindex = function (t,k,val)
			_t[k] = val
			return v
		end
	}
	local _r = {}
	setmetatable(_r,mt)
	return _r
end

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
	if font == nil or text == nil then
		print(debug.traceback())
	end
	text = "" .. text
	return math.ceil((font:width(font:height(),text)) * 1.2)
end

function pad_height(font,text)
	if font == nil or text == nil then
		print(debug.traceback())
	end
	text = "" .. text
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
	if not GUI.donothook then
		if GUI.noticed then
			if not GUI.handle then
				GUI.handle = gasp.new_handle()
			end
			if not GUI.handle then
				return false
			end
			if GUI.handle:valid() == true then
				return true
			end
			return GUI.handle:init( GUI.noticed.entryId )
		end
	end
	return false
end

function autoload()
	local tmp, v, ok, err, func
	v = GUI.cheat
	if v then return rebuild_cheat(v) end
	if GUI.cheatfile then
		tmp = cheatspath() .. "/" .. GUI.cheatfile
		func, ok, err, v = Require( tmp )
		if not ok then
			print( tostring(err) )
			v = nil
		else
			v = rebuild_cheat(func())
		end
		if v and v.app then
			GUI.cfg.find_process =
				v.app.name or GUI.cfg.find_process
			if not GUI.donothook then
				print( "Searching for process '"
					.. GUI.cfg.find_process .. "'")
				tmp = gasp.locate_app(GUI.cfg.find_process)
				print( "Found " .. (#tmp) .. " matching processes" )
				-- Auto hook process
				if #tmp == 1 then
					GUI.noticed = tmp[1]
					hook_process()
				end
			end
		end
	end
	if v then
		GUI.cheat = v
	end
	return v
end

local function draw_all(ctx)
	local push_font = (GUI.cfg.font.use ~= "default")
	local tmp
	GUI.quit = glfw.window_should_close( GUI.window )
	if push_font == true then
		nk.style_push_font( ctx, get_font() )
	end
	if nk.window_begin(ctx, "Show",
		{0,0,GUI.cfg.window.width,GUI.cfg.window.height},
		nk.WINDOW_BORDER
	) then
		if not GUI.cheat then
			GUI.cheat = autoload()
		end
		GUI.keep_cheat = nil
		GUI.draw[GUI.which].func(
			GUI.ctx, GUI.which, GUI.previous )
		if type(GUI.keep_cheat) == "table" then
			GUI.cheat = rebuild_cheat( GUI.keep_cheat )
		end
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
		if GUI.keep_cheat then
			GUI.cheat = rebuild_cheat( GUI.keep_cheat )
		end
	end
	rear.shutdown()
	
	GUI.window = nil
	GUI.ctx = nil
	atlas = nil
	
end

GUI.draw_reboot = function(ctx)
	local text = "Reboot GUI"
	nk.layout_row_dynamic( ctx, pad_height(get_font(),text), 2)
	if nk.button( ctx, nil, text ) then
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
	local path = scriptspath()
	local func, ok, err, v = Require( path .. "/" .. file )
	if not ok then
		func = GUI.draw_fallback
	end
	GUI.draw[#(GUI.draw) + 1] = { name = name, desc = desc, func = func() }
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
