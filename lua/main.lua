local function draw_addr_field( gui, ctx, font, v )
	local hex
	v.addr = v.addr or 0
	if v.offset then
		hex = string.format( "%X", v.offset )
		if v.generated == true then
			nk.label( ctx, hex, nk.TEXT_LEFT )
		else
			hex = nk.edit_string(ctx,nk.EDIT_FIELD,hex,10)
			v.offset = tonumber( hex, 16 )
		end
	end
	
	hex = string.format( "%X", v.addr )
	if v.generated == true or v.offset then
		nk.label(ctx,hex,nk.TEXT_LEFT)
	else
		hex = nk.edit_string(ctx,nk.EDIT_FIELD,hex,20)
		v.addr = tonumber( hex, 16 )
	end
	return v
end
local function draw_type_field( gui, ctx, font, v )
	local types = { "signed", "unsigned", "bits", "text", "bytes" }
	local k,x
	v.Type = v.Type or "bytes"
	for k = 1,#types,1 do
		if types[k] == v.Type then
			x = k
			break
		end
	end
	k = x
	if not k or k > #types then k = #types end
	v.Type = types[k]
	if v.count then
		nk.label( ctx, "", nk.TEXT_LEFT )
	elseif v.generated then
		nk.label( ctx, v.Type, nk.TEXT_LEFT )
	else
		k = nk.combo( ctx,
			types, k, pad_height(font,v.Type),
			{	-- width & height of dropdown box
				pad_width(font,v.Type) * 2,
				pad_height(font,v.Type) * (#types + 1)
			}
		)
		v.Type = types[k]
	end
	return v
end
local function draw_size_field(gui,ctx,font,v)
	local x
	x = v.size or 1
	if v.generated then
		nk.label(ctx,"",nk.TEXT_LEFT)
		nk.label(ctx,"",nk.TEXT_LEFT)
	else
		if nk.button(ctx,nil,"-") then
			x = x - 1
		end
		if nk.button(ctx,nil,"+") then
			x = x + 1
		end
		if x < 1 then x = 1 end
		if v.Type == "bits" then
			if x > 64 then x = 64 end
		elseif v.Type ~= "text" and v.Type ~= "bytes" then
			if x > 8 then x = 8 end
		end
	end
	nk.label( ctx, tostring(x), nk.TEXT_RIGHT )
	v.size = x
	return v
end

local function draw_edit_field(gui,ctx,font,v)
	local tmp, maxlen = 1
	v.edited = false
	v.as_text = v.as_text or ""
	if v.Type == "string" then
		maxlen = v.size + 1
	elseif v.Type == "bytes" then
		maxlen = (v.size * 3) + 1
	elseif v.Type == "signed" or v.Type == "unsigned" then
		maxlen = v.size * 3
	else
		maxlen = 1
	end
	tmp = nk.edit_string(ctx,nk.EDIT_FIELD,v.as_text,maxlen) or ""
	if tmp ~= v.as_text then
		v.edited = true
	end
	v.as_text = tmp
	if v.Type == "signed" or v.Type == "unsigned" then
		tmp = tonumber(v.as_text) or 0
		maxlen,tmp = gasp.tointbytes(v.Type,v.size,tmp)
		--[
		if gui.cheat.endian == "Big" then
			maxlen,tmp = gasp.flipbytes("table",v.size,tmp)
		end
		--]]
	else
		maxlen,tmp = gasp.tointbytes(v.Type,v.size,v.as_text)
	end
	v.as_bytes = tmp
	return v
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
	v.size = v.size or 1
	k = v.size * 4
	if not v.as_text then
		if gui.cheat_addr == v.addr then
			v.as_text = gui.cheat_text
		-- Read the value of the app and display that
		elseif gui.handle and gui.handle:valid() == true then
			v.as_text = ""
			tmp = gui.handle:glance( v.addr, v.size )
			if tmp then
				if v.Type == "signed" then
					--[[
					if gui.cheat.endian == "Big" then
						k, tmp = gasp.flipbytes(v.size,tmp)
					end
					--]]
					v.as_text = "" .. gasp.bytes2int("table",v.size,tmp)
				elseif v.Type == "bytes" then
					v.as_text = gasp.totxtbytes("table",v.size,tmp)
				elseif v.Type == "string" then
					v.as_text = gasp.bytes2txt("table",v.size,tmp)
				end
			end
		else
			v.as_text = ""
		end
	end
	v = gui.draw_edit_field(gui,ctx,font,v)
	if v.edited == true then
		gui.cheat_text = v.as_text
		gui.cheat_addr = v.addr
		if gui.handle and gui.handle:valid() == true then
			gui.handle:change( v.addr, "table", v.size, v.as_bytes )
		end
	elseif v.addr == gui.cheat_addr then
		gui.cheat_addr = nil
		gui.cheat_text = nil
	end
	v.as_text = nil
	return gui, v
end

local function draw_cheats( gui, ctx, font, v )
	local prv, now, nxt, text, i, j, k, x, tmp, bytes, test, list
	tmp = gui.idc
	gui.idc = gui.idc + 1
	nk.layout_row_dynamic( ctx, pad_height( font, v.desc ), 1 )
	if type(v.active) ~= "boolean" then v.active = true end
	v.active = nk.checkbox( ctx, v.desc, v.active )
	if v.active ~= true then return gui, v end
	text = "Count"
	if v.list then
		list = {}
		for j,x in pairs(v.list) do
			k = {}
			k.desc = x.desc
			k.offset = x.offset
			if x.offset then
				k.addr = v.addr + x.offset
			else
				k.addr = x.addr
			end
			k.Type = x.Type
			k.size = x.size
			k.list = x.list
			k.count = x.count
			k.split = x.split
			gui, k = gui.draw_cheat(gui,ctx,font,k)
			list[j] = k
		end
		v.list = list
		return gui, v
	end

	v.change_all = nil
	if v.split then
		list = {}
		bytes = 0
		for j,x in pairs(v.split) do
			k = {}
			k.method = x.method or v.method
			k.desc = v.desc .. "#All:" .. x.desc
			k.offset = x.offset or bytes
			k.addr = v.addr + k.offset
			k.Type = x.Type
			k.size = x.size
			k.list = x.list
			k.count = x.count
			k.split = x.split
			gui, k = gui.draw_cheat(gui,ctx,font,k)
			k.desc = x.desc
			list[j] = k
			if k.edited == true then
				v.change_all = k
			end
			bytes = bytes + k.size
		end
		v.split = list
	end
	
	if v.count then
		list = {}
		tmp = v.addr
		v.prev_list = v.prev_list or {}
		for i = 1,v.count,1 do
			text = v.desc .. '#' .. i
			if v.split then
				bytes = 0
				for j,x in pairs(v.split) do
					if v.prev_list[i * j] then
						x = v.prev_list[i * j]
					end
					k = {}
					k.method = x.method
					k.desc = text .. ':' .. (x.desc or '???')
					k.addr = tmp + bytes
					k.offset = x.offset or bytes
					k.Type = x.Type
					k.size = x.size or 1
					k.generated = true
					if v.change_all and
						k.offset == v.change_all.offset then
						k.edited = true
						k.as_text = v.change_all.as_text
						k.as_bytes = v.change_all.as_bytes
					end
					k.list = x.list
					k.count = x.count
					k.split = x.split
					gui, k = gui.draw_cheat(gui,ctx,font,k)
					k.desc = x.desc
					bytes = bytes + k.size
					list[i * j] = k
				end
			else
				k = {}
				k.method = v.method
				k.desc = text
				k.addr = tmp
				k.edited = v.edited
				k.as_text = v.as_text
				k.Type = v.Type or "bytes"
				k.size = v.size or 1
				k.generated = true
				k.list = x.list
				k.count = x.count
				k.split = x.split
				gui, k = gui.draw_cheat( gui, ctx, font, k )
				list[i] = k
			end
			tmp = tmp + v.size
		end
		v.prev_list = list
	end
	return gui, v
end

local function draw_cheat(gui,ctx,font,v)
	local prv, now, nxt, text, j, k, x, tmp, bytes, test
	
	if v.desc == nil then
		v.desc = "???"
	end
	
	if v.list or v.count or v.split then
		return gui.draw_cheats( gui, ctx, font, v )
	end
	
	if type(v.active) ~= "boolean" then v.active = false end
	
	text = v.desc
	nk.layout_row_template_begin(ctx, pad_height(font,"="))
	nk.layout_row_template_push_static(ctx, pad_width(font,"="))
	nk.layout_row_template_push_dynamic(ctx)
	if v.offset then
		nk.layout_row_template_push_static(ctx, pad_width(font,"0000"))
	end
	nk.layout_row_template_push_static(ctx, pad_width(font,"00000000"))
	nk.layout_row_template_push_static(ctx, pad_width(font,"unsigned"))
	nk.layout_row_template_push_static(ctx, pad_width(font,"-"))
	nk.layout_row_template_push_static(ctx, pad_width(font,"+"))
	nk.layout_row_template_push_static(ctx, pad_width(font,"100"))
	nk.layout_row_template_push_static(ctx, pad_width(font,"00 00 00 00 "))
	nk.layout_row_template_end(ctx)

	-- Method of changing the value
	nk.label(ctx,v.method or "=",nk.TEXT_LEFT)
	
	-- Description of the cheat
	v.active = nk.checkbox( ctx, v.desc, v.active )
	
	v = gui.draw_addr_field(gui,ctx,font,v)
	v = gui.draw_type_field(gui,ctx,font,v)
	v = gui.draw_size_field(gui,ctx,font,v)
	return gui.draw_cheat_edit(gui,ctx,font,v)
end
local function draw_all_cheats(gui,ctx)
	local i, v, id, text, j, k, tmp, font
	if not gui.cheat then
		return gui
	end
	font = get_font(gui)
	text = "Game"
	nk.layout_row_dynamic( ctx, pad_height( font, text ), 2 )
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
	gui.cheat.desc = "Cheats"
	gui, tmp = gui.draw_cheat(gui,ctx,font,gui.cheat)
	gui.cheat = tmp
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
function get_cheat_dir(gui)
	local text = (os.getenv("PWD") or os.getenv("CWD")) .. '/cheats'
	if mkdir(text) then
		return text
	end
	if mkdir(gui.cfg.cheatdir) then
		return gui.cfg.cheatdir
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


return function(gui,ctx)
	local font = get_font(gui)
	local text, file, dir, tmp, i, v
	gui = gui.draw_reboot(gui,ctx)
	gui.draw_addr_field = draw_addr_field
	gui.draw_type_field = draw_type_field
	gui.draw_size_field = draw_size_field
	gui.draw_edit_field = draw_edit_field
	gui.draw_cheat = draw_cheat
	gui.draw_cheats = draw_cheats
	gui.draw_cheat_edit = draw_cheat_edit
	nk.layout_row_dynamic( ctx, pad_height(font,text), 1)
	for i,v in pairs(gui.draw) do
		if i > 1 then
			if nk.button(ctx, nil, v.desc) then
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
	gui.cfg.volume = nk.slider(ctx, 0, gui.cfg.volume, 1.0, 0.1)
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
		return gui
	end
	return draw_all_cheats( gui, ctx )
end
