local function draw_addr_field( ctx, font, v )
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
	GUI.keep_cheat = v
end
local function draw_type_field( ctx, font, v )
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
	GUI.keep_cheat = v
end
local function draw_size_field(ctx,font,v)
	local x
	if not v then
		print( debug.traceback() )
	end
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
	GUI.keep_cheat = v
end

local function draw_edit_field(ctx,font,v)
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
		if GUI.cheat and GUI.cheat.endian == "Big" then
			maxlen,tmp = gasp.flipbytes("table",v.size,tmp)
		end
		--]]
	else
		maxlen,tmp = gasp.tointbytes(v.Type,v.size,v.as_text)
	end
	v.as_bytes = tmp
	return v
end

local function draw_cheat_edit(ctx,font,v)
	local text, tmp, k, x, test
	-- Editing in progress
	v.size = v.size or 1
	k = v.size * 4
	if not v.as_text then
		if GUI.cheat_addr == v.addr then
			v.as_text = GUI.cheat_text
		-- Read the value of the app and display that
		elseif GUI.handle and GUI.handle:valid() == true then
			v.as_text = ""
			tmp = GUI.handle:glance( v.addr, v.size )
			if tmp then
				if v.Type == "signed" then
					--[[
					if GUI.cheat.endian == "Big" then
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
	GUI.draw_edit_field(ctx,font,v)
	v = GUI.keep_cheat
	if v.edited == true then
		GUI.cheat_text = v.as_text
		GUI.cheat_addr = v.addr
		if GUI.handle and GUI.handle:valid() == true then
			GUI.handle:change( v.addr, "table", v.size, v.as_bytes )
		end
	elseif v.addr == GUI.cheat_addr then
		GUI.cheat_addr = nil
		GUI.cheat_text = nil
	end
	v.as_text = nil
	GUI.keep_cheat = v
end

local function draw_cheats( ctx, font, v )
	local prv, now, nxt, text, i, j, k, x, tmp, bytes, test, list
	tmp = GUI.idc
	GUI.idc = GUI.idc + 1
	nk.layout_row_dynamic( ctx, pad_height( font, v.desc ), 1 )
	if type(v.active) ~= "boolean" then v.active = true end
	v.active = nk.check( ctx, v.desc, v.active )
	GUI.keep_cheat = v
	if v.active == false then
		return
	end
	text = "Count"
	v.prev = v.prev or {}
	if v.list then
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
			k.generated = x.generated
			GUI.keep_cheat = nil
			GUI.draw_cheat(ctx,font,k)
			k = GUI.keep_cheat
			v.list[j] = k
		end
		GUI.keep_cheat = v
		return
	end

	v.change_all = nil
	if v.split then
		list = {}
		bytes = 0
		for j,x in pairs(v.split) do
			k = {}
			GUI.keep_cheat = nil
			k.method = x.method or v.method
			k.desc = v.desc .. "#All:" .. x.desc
			k.offset = x.offset or bytes
			k.addr = v.addr + k.offset
			k.Type = x.Type
			k.size = x.size
			k.list = x.list
			k.count = x.count
			k.split = x.split
			GUI.draw_cheat(ctx,font,k)
			GUI.keep_cheat.desc = x.desc
			list[j] = GUI.keep_cheat
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
		for i = 1,v.count,1 do
			text = v.desc .. '#' .. i
			if v.split then
				bytes = 0
				for j,x in pairs(v.split) do
					k = {}
					x = v.prev[i * j] or x
					GUI.keep_cheat = nil
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
					GUI.draw_cheat(ctx,font,k)
					GUI.keep_cheat.desc = x.desc
					list[i * j] = GUI.keep_cheat
					bytes = bytes + k.size
				end
			else
				k = {}
				x = v.prev[i] or x
				GUI.keep_cheat = nil
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
				GUI.draw_cheat( ctx, font, k )
				GUI.keep_cheat.desc = x.desc
				list[i] = GUI.keep_cheat
			end
			tmp = tmp + v.size
		end
		v.prev = list
	end
	GUI.keep_cheat = v
end

local function draw_cheat(ctx,font,v)
	local prv, now, nxt, text, j, k, x, tmp, bytes, test
	
	if v.desc == nil then
		v.desc = "???"
	end
	
	GUI.keep_cheat = nil
	
	if v.prev or v.list or v.count or v.split then
		GUI.draw_cheats( ctx, font, v )
		v = GUI.keep_cheat
		return
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
	v.active = nk.check( ctx, v.desc, v.active )
	
	GUI.draw_addr_field(ctx,font,v)
	GUI.draw_type_field(ctx,font,GUI.keep_cheat)
	GUI.draw_size_field(ctx,font,GUI.keep_cheat)
	GUI.draw_cheat_edit(ctx,font,GUI.keep_cheat)
end
local function draw_all_cheats(ctx)
	local i, v, id, text, j, k, tmp, font
	if not GUI.cheat then
		return
	end
	font = get_font()
	text = "Game"
	nk.layout_row_dynamic( ctx, pad_height( font, text ), 2 )
	nk.label(ctx,text,nk.TEXT_LEFT)
	if GUI.cheat.emu then
		nk.label(ctx,GUI.cheat.emu.desc,nk.TEXT_LEFT)
	elseif GUI.cheat.app then
		nk.label(ctx,GUI.cheat.app.desc,nk.TEXT_LEFT)
	else
		nk.label(ctx,"Undescribed",nk.TEXT_LEFT)
	end
	GUI.cheat.endian = GUI.cheat.endian or gasp.get_endian()
	text = "Endian"
	nk.label(ctx,text,nk.TEXT_LEFT)
	nk.label(ctx,GUI.cheat.endian,nk.TEXT_LEFT)
	GUI.cheat.desc = "Cheats"
	GUI.draw_cheat(ctx,font,GUI.cheat)
	GUI.cheat = GUI.keep_cheat
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
	if mkdir(GUI.cfg.cheatdir) then
		return GUI.cfg.cheatdir
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


return function(ctx)
	local font = get_font()
	local text, file, dir, tmp, i, v
	GUI.draw_reboot(ctx)
	GUI.draw_addr_field = draw_addr_field
	GUI.draw_type_field = draw_type_field
	GUI.draw_size_field = draw_size_field
	GUI.draw_edit_field = draw_edit_field
	GUI.draw_cheat = draw_cheat
	GUI.draw_cheats = draw_cheats
	GUI.draw_cheat_edit = draw_cheat_edit
	nk.layout_row_dynamic( ctx, pad_height(font,text), 1)
	for i,v in pairs(GUI.draw) do
		if i > 1 then
			if nk.button(ctx, nil, v.desc) then
				 GUI.which = i
			end
		end
	end
	--[[ Slider Example with Custom width
	text = "Volume:"
	nk.layout_row_begin(ctx, 'static', pad_height(font,text), 2)
	nk.layout_row_push(ctx, pad_width(font,text))
	nk.label(ctx,text, nk.TEXT_LEFT)
	nk.layout_row_push(ctx, pad_width(font,text))
	GUI.cfg.volume = nk.slider(ctx, 0, GUI.cfg.volume, 1.0, 0.1)
	nk.layout_row_end(ctx)
	--]]
	if GUI.handle and GUI.handle:valid() == true then
		text = "Hooked:"
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label( ctx, text, nk.TEXT_LEFT )
		nk.label( ctx, GUI.noticed.name, nk.TEXT_LEFT )
		nk.layout_row_dynamic(ctx,pad_height(font,text),1)
		if nk.button( ctx, nil, "Unhook" ) then
			GUI.handle:term()
			GUI.donothook = true
		end
	elseif GUI.noticed then
		text = "Selected:"
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label( ctx, text, nk.TEXT_LEFT )
		nk.label( ctx, GUI.noticed.name, nk.TEXT_LEFT )
		nk.layout_row_dynamic(ctx,pad_height(font,text),1)
		if nk.button( ctx, nil, "Hook" ) then
			GUI.donothook = false
			hook_process()
		end
	else
		nk.layout_row_dynamic(ctx,pad_height(font,text),1)
		nk.label( ctx, "Nothing selected", nk.TEXT_LEFT )
		
	end
	return draw_all_cheats( ctx )
end
