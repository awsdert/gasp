local function draw_desc_field( ctx, font, v )
	local text = '+ ' .. v.desc
	if v.active == true then
		text = '- ' .. v.desc
	end
	if v.desc == "Player" then
		print( "1: " .. tostring(v.active) )
		if _G.Player then
			print( "2: " .. tostring(_G.Player.active) )
		end
		_G.Player = v
	end
	if v.is_group then
		if nk.button( ctx, nil, text ) then
			v.active = not v.active
		end
	else
		v.active = nk.checkbox( ctx, v.desc, v.active )
	end
	if v.desc == "Player" then
		print( "3: " .. tostring(v.active) )
	end
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
end
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
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
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
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
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
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
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
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
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
	v = rebuild_cheat( GUI.keep_cheat )
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
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
end

local function draw_cheats( ctx, font, v )
	local prv, now, nxt, text, i, j, k, x, tmp, bytes, test, list, all
	v = rebuild_cheat(v)
	tmp = GUI.idc
	GUI.idc = GUI.idc + 1
	nk.layout_row_dynamic( ctx, pad_height( font, v.desc ), 1 )
	v = GUI.draw_desc_field( ctx, font, v )
	if v.active == false then
		return rebuild_cheat(v)
	end
	text = "Count"
	v.prev = v.prev or {}
	if v.list then
		list = {}
		list[#(v.list)] = {}
		for j,x in pairs(v.list) do
			k = rebuild_cheat(x)
			if x.offset then
				k.addr = v.addr + x.offset
			else
				k.addr = x.addr
			end
			GUI.keep_cheat = nil
			list[j] = GUI.draw_cheat(ctx,font,rebuild_cheat(k))
		end
		v.list = list
		GUI.keep_cheat = rebuild_cheat(v)
		return rebuild_cheat(v)
	end

	if v.split then
		bytes = 0
		for j,x in pairs(v.split) do
			k = rebuild_cheat(x)
			GUI.keep_cheat = nil
			k.method = x.method or v.method
			k.desc = v.desc .. "#All:" .. k.desc
			k.offset = x.offset or bytes
			k.addr = v.addr + k.offset
			k = GUI.draw_cheat(ctx,font,rebuild_cheat(k))
			k.desc = x.desc
			v.split[j] = rebuild_cheat(k)
			if k.edited == true then
				v.all = k
			end
			bytes = bytes + k.size
		end
		GUI.keep_cheat = nil
	end
	
	if v.count then
		list = {}
		list[v.count * #(v.split or {1})] = {}
		tmp = v.addr
		for i = 1,v.count,1 do
			text = v.desc .. '#' .. i
			if v.split then
				bytes = 0
				for j,x in pairs(v.split) do
					x = v.prev[i * j] or x
					k = rebuild_cheat(x)
					GUI.keep_cheat = nil
					k.desc = text .. ':' .. (x.desc or '???')
					k.addr = tmp + bytes
					k.offset = x.offset or bytes
					k.generated = true
					if all and k.offset == all.offset then
						k.edited = true
						k.as_text = all.as_text
						k.as_bytes = all.as_bytes
					end
					k.list = x.list
					k.count = x.count
					k.split = x.split
					k = GUI.draw_cheat(ctx,font,rebuild_cheat(k))
					k.desc = x.desc
					list[i * j] = rebuild_cheat(k)
					bytes = bytes + k.size
				end
			else
				if v.prev[i] then
					k = rebuild_cheat(v.prev[i])
				else
					k = rebuild_cheat(v)
					k.prev = nil
					k.list = nil
					k.count = nil
					k.split = nil
					k.active = false
				end
				GUI.keep_cheat = nil
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
				k = GUI.draw_cheat( ctx, font, rebuild_cheat(k) )
				k.desc = x.desc
				list[i] = rebuild_cheat(k)
			end
			tmp = tmp + v.size
		end
		v.prev = list
	end
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
end

local function draw_cheat(ctx,font,v)
	local prv, now, nxt, text, j, k, x, tmp, bytes, test
	v = rebuild_cheat(v)
	
	GUI.keep_cheat = nil
	
	if v.prev or v.list or v.count or v.split then
		v = GUI.draw_cheats( ctx, font, rebuild_cheat(v) )
		GUI.keep_cheat = rebuild_cheat(v)
		return rebuild_cheat(v)
	end
	
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
	
	v = GUI.draw_desc_field(ctx,font,rebuild_cheat(v))
	v = GUI.draw_addr_field(ctx,font,rebuild_cheat(v))
	v = GUI.draw_type_field(ctx,font,rebuild_cheat(v))
	v = GUI.draw_size_field(ctx,font,rebuild_cheat(v))
	v = GUI.draw_cheat_edit(ctx,font,rebuild_cheat(v))
	return rebuild_cheat(v)
end
local function draw_all_cheats(ctx,font,v)
	local i, id, text, j, k, tmp
	if not v then
		print(debug.traceback())
		error("root cheat was nil")
		return
	end
	v = rebuild_cheat(v)
	text = "Game"
	nk.layout_row_dynamic( ctx, pad_height( font, text ), 2 )
	nk.label(ctx,text,nk.TEXT_LEFT)
	if v.emu then
		nk.label(ctx,v.emu.desc,nk.TEXT_LEFT)
	elseif v.app then
		nk.label(ctx,v.app.desc,nk.TEXT_LEFT)
	else
		nk.label(ctx,"Undescribed",nk.TEXT_LEFT)
	end
	v.endian = v.endian or gasp.get_endian()
	text = "Endian"
	nk.label(ctx,text,nk.TEXT_LEFT)
	nk.label(ctx,v.endian,nk.TEXT_LEFT)
	v.desc = "Cheats"
	v = GUI.draw_cheat(ctx,font,rebuild_cheat(v))
	GUI.keep_cheat = rebuild_cheat(v)
	return rebuild_cheat(v)
end

local function list_cheat_files()
	local text = find_cheat_dir()
	return scandir( text ), text
end


return function(ctx)
	local font = get_font()
	local text, file, dir, tmp, i, v
	GUI.draw_reboot(ctx)
	GUI.draw_desc_field = draw_desc_field
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
	if not GUI.handle then
		if GUI.noticed then
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
		if hook_process() == false then
			return
		end
	end
	if hook_process() == true then
		text = "Hooked:"
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label( ctx, text, nk.TEXT_LEFT )
		nk.label( ctx, GUI.noticed.name, nk.TEXT_LEFT )
		nk.layout_row_dynamic(ctx,pad_height(font,text),1)
		if nk.button( ctx, nil, "Unhook" ) then
			GUI.handle:term()
			GUI.donothook = true
		end
	end
	v = draw_all_cheats( ctx, font, autoload() )
	GUI.cheat = rebuild_cheat(v)
	GUI.keep_cheat = rebuild_cheat(v)
end
