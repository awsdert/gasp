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

local function calc_cheat_bounds(font,text,prev)
	local t = { x = 0, y = 0,
		w = pad_width(font,text),
		h = pad_height(font,text)
	}
	if prev then
		t.x = t.x + t.w
	end
	return t
end

local function draw_cheat_edit(gui,ctx,font,v)
	local text, tmp, k, x, test
	-- Editing in progress
	if gui.cheat_addr == v.addr then
		text = gui.cheat_text or ""
		k = (v.signed or v.bytes or 0) * 3
		if k == 0 then k = 1 end
	--[[ Only a cheat with count should generate this and only
	when the value of the top cheat is edited ]]
	elseif v.value then
		text = v.value
		k = (v.signed or v.bytes or 0) * 3
		if k == 0 then k = 1 end
	-- Read the value of the app and display that
	elseif gui.handle and gui.handle:valid() == true then
		if v.signed then
			k = v.signed * 3
			text = gui.handle:read( v.addr, v.signed )
			if text then
				--[[
				if gui.cheat.endian == "Big" then
					text = gasp.flipbytes(v.signed,text)
				end
				--]]
				text = "" .. gasp.bytes2int(v.signed,text)
			else text = "" end
		elseif v.bytes then
			k = v.bytes * 3
			test = 'bytes'
			text = gui.handle:read( v.addr, v.bytes )
			if #text > 0 then text = gasp.totxtbytes(v.bytes,text)
			else text = "" end
		else
			text = ""
			k = 1
		end
	else
		text = ""
		k = 1
	end
	v.edited = nil
	tmp = nk.edit_string(ctx,nk.EDIT_FIELD,text,k) or ""
	if gui.handle and gui.handle:valid() == true then
		if tmp ~= text then
			v.edited = tmp
			gui.cheat_text = tmp
			gui.cheat_addr = v.addr
			if v.signed then
				tmp = gasp.int2bytes(tointeger(tmp) or 0,v.signed)
				if gui.cheat.endian == "Big" then
					tmp = gasp.flipbytes(v.signed,tmp)
				end
				k = v.signed
			elseif v.bytes then
				tmp = gasp.tointbytes(tmp)
				k = v.bytes
			else
				tmp = {}
				k = 0
			end
			gui.handle:write( v.addr, k, tmp )
		elseif v.addr == gui.cheat_addr and tmp == gui.cheat_text then
			gui.cheat_addr = nil
			gui.cheat_text = nil
		end
	end
	return gui
end

local function draw_cheat(gui,ctx,font,v)
	local prv, now, nxt, text, j, k, x, tmp, bytes, test
	
	nk.layout_row_template_begin(ctx, pad_height(font,"="))
	nk.layout_row_template_push_static(ctx, pad_width(font,"="))
	nk.layout_row_template_push_dynamic(ctx)
	if v.offset then
		nk.layout_row_template_push_static(ctx, pad_width(font,"0000"))
	end
	nk.layout_row_template_push_static(ctx, pad_width(font,"0000000000000000"))
	nk.layout_row_template_push_static(ctx, pad_width(font,"00 00 00 00 "))
	nk.layout_row_template_end(ctx)
	
	-- Method of changing the value
	nk.label(ctx,v.method or "=",nk.TEXT_LEFT)
	
	-- Description of the cheat
	nk.label(ctx,v.desc or "{???}",nk.TEXT_LEFT)
	
	if v.offset then
		text = string.format( "%X", v.offset )
		-- Give user a chance to edit the offset
		text  = nk.edit_string(ctx,nk.EDIT_FIELD | nk.TEXT_RIGHT,text,16)
		v.offset = tonumber( text, 16 )
	end
	
	text = string.format( "%X", v.addr )
	if v.generated == true or v.offset then
		-- Generated addresses should not be editable
		nk.label(ctx,text,nk.TEXT_LEFT)
	else
		-- Give user a chance to edit the address
		text  = nk.edit_string(ctx,nk.EDIT_FIELD | nk.TEXT_RIGHT,text,16)
		v.addr = tonumber( text, 16 )
	end
	
	if v.list then
		nk.label( ctx, "", nk.TEXT_LEFT )
		for k,x in pairs(v.list) do
			k = {}
			k.desc = x.desc
			k.addr = x.addr
			k.offset = x.offset
			if x.offset then
				k.addr = v.addr + x.offset
				k.generated = true
			else
				k = x.addr
			end
			k.signed = x.signed
			k.bytes = x.bytes
			gui = draw_cheat(gui,ctx,font,k)
			k = nil
		end
		return gui
	end

	if v.split then
		nk.label(ctx,"",nk.TEXT_LEFT)
		bytes = 0
		for k,x in pairs(v.split) do
			k = {}
			k.method = x.method or v.method
			k.desc = v.desc .. "#All:" .. x.desc
			k.addr = v.addr + bytes
			k.signed = x.signed
			k.bytes = x.bytes
			k.generated = true
			gui = draw_cheat(gui,ctx,font,k)
			k = nil
			bytes = bytes + (x.signed or x.bytes or v.size or 1)
		end
	else
		gui = draw_cheat_edit(gui,ctx,font,v)
	end
	
	if v.count then
		j = 0
		while j < v.count do
			text = v.desc .. '#' .. (j + 1)
			tmp = v.addr + (j * v.size)
			if v.split then
				bytes = 0
				for k,x in pairs(v.split) do
					k = {}
					k.method = x.method
					k.desc = text .. ':' .. (x.desc or '???')
					k.addr = tmp + bytes
					k.signed = x.signed
					k.bytes = x.bytes
					k.generated = true
					if k[test] == v[test] then
						k.value = v.edited
					end
					gui = draw_cheat(gui,ctx,font,k)
					bytes = bytes + (x.signed or x.bytes or 1)
					k = nil
				end
			else
				k = {}
				k.method = x.method or v.method
				k.desc = text
				k.addr = tmp
				k.value = v.edited
				k.signed = v.signed
				k.bytes = v.bytes
				k.generated = true
				gui = draw_cheat( gui, ctx, font, k )
				k = nil
			end
			j = j + 1
		end
	end
	v.edited = nil
	return gui
end
local function draw_cheats(gui,ctx)
	local i, v, id, text, j, k, tmp, font
	glfw.wait_events_timeout(cfg.rps or 0.5)
	font = get_font(gui)
	id = gui.idc
	gui.idc = gui.idc + 1
	if not gui.cheat then
		gui.cheat = {}
	end
	if nk.tree_push( ctx, nk.TREE_NODE,
		"Cheats", nk.MAXIMIZED, id ) then
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		text = "Game"
		nk.label(ctx,text,nk.TEXT_LEFT)
		if gui.cheat.emu then
			nk.label(ctx,gui.cheat.emu.desc,nk.TEXT_LEFT)
		elseif gui.cheat.app then
			nk.label(ctx,gui.cheat.app.desc,nk.TEXT_LEFT)
		else
			nk.label(ctx,"Undescribed",nk.TEXT_LEFT)
		end
		gui.cheat.endian = gui.cheat.endian or gasp.get_endian()
		text = "Endian"
		nk.label(ctx,text,nk.TEXT_LEFT)
		nk.label(ctx,gui.cheat.endian,nk.TEXT_LEFT)
		if gui.cheat.list then
			for i,v in pairs(gui.cheat.list) do
				gui = draw_cheat(gui,ctx,font,v)
			end
		end
		nk.tree_pop(ctx)
	end
	return gui
end
function mkdir(path)
	if not path then return end
	local ret = gasp.path_isdir( path )
	if ret == 1 then
		return path
	elseif ret == 0 then
		io.popen("mkdir '" .. path .. "'")
		if gasp.path_isdir( path ) == 1 then
			return path
		end
	end
end
function get_cheat_dir()
	local text = (os.getenv("PWD") or os.getenv("CWD")) .. '/cheats'
	if mkdir(text) then
		return text
	end
	if mkdir(cfg.cheatdir) then
		return cfg.cheatdir
	end
	text = os.getenv("GASP_PATH")
	if mkdir(text) then
		text = text .. '/cheats'	
		if mkdir(text) then
			return text
		end
	end
	-- Should never reach here, also should handle in less drastic way
	nk.shutdown()
end
local function list_cheat_files()
	local text = find_cheat_dir()
	return scandir( text ), text
end
function hook_process(gui)
	if gui.noticed then
		if not gui.handle then
			gui.handle = gasp.new_handle()
		end
		if not gui.handle then
			return gui
		end
		if gui.handle:valid() == true then
			return gui
		end
		if gui.handle:init( gui.noticed.entryId ) == true then
			return gui
		end
	end
	return gui
end

GUI.which = "main"
GUI.draw["main"] = { desc = "Main", func = function(gui,ctx)
	local font = get_font(gui)
	local text, file, dir, tmp, i, v
	nk.layout_row_dynamic( ctx, pad_height(font,text), 1)
	for i,v in pairs(GUI.draw) do
		if i ~= "main" then
			text = v.desc
			if nk.button(ctx, nil, text) then
				 gui.which = i
			end
		end
	end
	--[[ Slider Example with Custom width
	text = "Volume:"
	nk.layout_row_begin(ctx, 'static', pad_height(font,text), 2)
	nk.layout_row_push(ctx, pad_width(font,text))
	nk.label(ctx,text, nk.TEXT_LEFT)
	nk.layout_row_push(ctx, pad_width(font,text))
	cfg.volume = nk.slider(ctx, 0, cfg.volume, 1.0, 0.1)
	nk.layout_row_end(ctx)
	--]]
	if gui.handle and gui.handle:valid() == true then
		text = "Hooked:"
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label( ctx, text, nk.TEXT_LEFT )
		nk.label( ctx, gui.noticed.name, nk.TEXT_LEFT )
		nk.layout_row_dynamic(ctx,pad_height(font,text),1)
		if nk.button( ctx, nil, "Unhook" ) then
			gui.handle:term()
			gui.donothook = true
		end
	elseif gui.noticed then
		text = "Selected:"
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label( ctx, text, nk.TEXT_LEFT )
		nk.label( ctx, gui.noticed.name, nk.TEXT_LEFT )
		nk.layout_row_dynamic(ctx,pad_height(font,text),1)
		if not gui.donothook then gui.donothook = false end
		if gui.donothook == true then
			if nk.button( ctx, nil, "Hook" ) then
				gui.donothook = false
				gui = hook_process(gui)
			end
		else
			nk.label( ctx, "Trying to hook...", nk.TEXT_LEFT );
			gui = hook_process(gui)
		end
	else
		nk.layout_row_dynamic(ctx,pad_height(font,text),1)
		nk.label( ctx, "Nothing selected", nk.TEXT_LEFT )
	end
	gui = draw_cheats( gui, ctx )
	return gui
end
}
GUI.draw["cfg-font"] = { desc = "Change Font", func = require("cfg.font") }
GUI.draw["cfg-proc"] = { desc = "Hook App", func = require("cfg.proc") }
GUI.draw["cfg-cheatfile"] = { desc = "Load Cheatfile", func = require("cfg.cheatfile") }
local function draw_all(gui,ctx)
	local push_font = (cfg.font.use ~= "default")
	if push_font == true then
		nk.style_push_font( ctx, get_font(gui) )
	end
	if nk.window_begin(ctx, "Show",
		{0,0,cfg.window.width,cfg.window.height}, nk.WINDOW_BORDER
	) then
		gui = gui.draw[gui.which].func(gui,gui.ctx,"main")
	end
	nk.window_end(ctx)
	if push_font == true then
		nk.style_pop_font(ctx)
	end
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
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(GUI.window)
	glfw.wait_events_timeout(1/cfg.fps)
	rear.new_frame()
	GUI.idc = 0
	GUI = draw_all(GUI,GUI.ctx)
end
rear.shutdown()
