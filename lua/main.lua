local function draw_addr_field( gui, ctx, font, v )
	local hex
	if v.offset then
		hex = string.format( "%X", v.offset )
		-- Give user a chance to edit the offset
		hex = nk.edit_string(ctx,nk.EDIT_FIELD,hex,10)
		v.offset = tonumber( hex, 16 )
	end
	
	hex = string.format( "%X", v.addr )
	if v.generated == true or v.offset then
		-- Generated addresses should not be editable
		nk.label(ctx,hex,nk.TEXT_LEFT)
	else
		-- Give user a chance to edit the address
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
	if gui.cheat_addr == v.addr then
		text = gui.cheat_text or ""
	--[[ Only a cheat with count should generate this and only
	when the value of the top cheat is edited ]]
	elseif v.value then
		text = v.value
	-- Read the value of the app and display that
	elseif gui.handle and gui.handle:valid() == true then
		if v.Type == "signed" then
			text = gui.handle:read( v.addr, v.size )
			if text then
				--[[
				if gui.cheat.endian == "Big" then
					text = gasp.flipbytes(v.size,text)
				end
				--]]
				text = "" .. gasp.bytes2int(v.size,text)
			else text = "" end
		elseif v.Type == "bytes" then
			test = 'bytes'
			text = gui.handle:read( v.addr, v.size )
			if #text > 0 then text = gasp.totxtbytes(v.size,text)
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
			if v.Type == "signed" then
				tmp = gasp.int2bytes(tointeger(tmp) or 0,v.size)
				if gui.cheat.endian == "Big" then
					tmp = gasp.flipbytes(v.size,tmp)
				end
			elseif v.Type == "bytes" then
				tmp = gasp.tointbytes(tmp)
			else
				tmp = {}
			end
			gui.handle:write( v.addr, v.size, tmp )
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
	nk.label(ctx,v.desc or "{???}",nk.TEXT_LEFT)
	
	
	v = gui.draw_addr_field(gui,ctx,font,v)
	v = gui.draw_type_field(gui,ctx,font,v)
	v = gui.draw_size_field(gui,ctx,font,v)
	
	if v.list then
		nk.label( ctx, "", nk.TEXT_LEFT )
		for j,x in pairs(v.list) do
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
			k.Type = x.Type
			k.size = x.size
			gui,k = draw_cheat(gui,ctx,font,k)
			if not k.generated then
				v.list[j] = k
			end
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
			k.Type = x.Type
			k.size = x.size
			k.generated = true
			gui = draw_cheat(gui,ctx,font,k)
			k = nil
			bytes = bytes + (x.size or v.size or 1)
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
					k.Type = x.Type
					k.size = x.size or 1
					k.generated = true
					if k[test] == v[test] then
						k.value = v.edited
					end
					gui = draw_cheat(gui,ctx,font,k)
					bytes = bytes + k.size
					k = nil
				end
			else
				k = {}
				k.method = x.method or v.method
				k.desc = text
				k.addr = tmp
				k.value = v.edited
				k.Type = x.Type or "bytes"
				k.size = v.size or 1
				k.generated = true
				gui = draw_cheat( gui, ctx, font, k )
				k = nil
			end
			j = j + 1
		end
	end
	v.edited = nil
	return gui, v
end
local function draw_cheats(gui,ctx)
	local i, v, id, text, j, k, tmp, font
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
				gui,v = draw_cheat(gui,ctx,font,v)
				gui.cheat.list[i] = v
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
	gui.draw_cheat = draw_cheat
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
	end
	gui = draw_cheats( gui, ctx )
	return gui
end
