local add_label = function(name,text)
	local used = (_G.pref.font.use == name)
	local font = _G.fonts[name]
	if used == false then
		nk.style_push_font( _G.gui.ctx, font)
	end
	nk.layout_row_static( _G.gui.ctx,
		pad_height(font,text), pad_width(font,text), #text )
	nk.label( _G.gui.ctx,text,nk.WINDOW_BORDER)
	if used == false then
		nk.style_pop_font( _G.gui.ctx )
	end
	return gui
end

local add_button = function(name,text)
	local used = (_G.pref.font.use == name)
	local font = get_font(name)
	if used == false then
		nk.style_push_font( _G.gui.ctx, font)
	end
	if nk.button( _G.gui.ctx, nil, text) then
		 _G.pref.font.use = name
	end
	if used == false then
		nk.style_pop_font( _G.gui.ctx )
	end
	return gui
end

local function update_fonts()
	if _G.gui.font.file ~= used.file then
		-- atlas needs to be respawned, window will be restarted
		_G.gui.reboot = gasp.set_reboot_gui(true)
		glfw.set_window_should_close( gui.window, true)
	end
end

return function(now,prv)
	local font = get_font()
	local xxlarge = get_font("xx-large")
	local used = _G.pref.font
	_G.GUI.draw_reboot()
	_G.GUI.draw_goback(prv,update_fonts)
	add_label(_G.pref.font.use,"Change font size to:")
	nk.layout_row_dynamic( _G.gui.ctx, pad_height(xxlarge,""), 7 )
	add_button( gui, 'xx-large', 'XX-Large')
	add_button( gui, 'x-large', 'X-Large')
	add_button( gui, 'large', 'Large')
	add_button( gui, 'medium', 'Medium')
	add_button( gui, 'small', 'Small')
	add_button( gui, 'x-small', 'X-Small')
	add_button( gui, 'xx-small', 'XX-Small')
	nk.layout_row_dynamic( _G.gui.ctx, pad_height(font,""), 1 )
	return gui
end
