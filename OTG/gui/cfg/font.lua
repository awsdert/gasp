local add_label = function(gui,name,text)
	local used = (gui.cfg.font.use == name)
	local font = gui.fonts[name]
	if used == false then
		nk.style_push_font(gui.ctx, font)
	end
	nk.layout_row_static(gui.ctx,
		pad_height(font,text), pad_width(font,text), #text )
	nk.label(gui.ctx,text,nk.WINDOW_BORDER)
	if used == false then
		nk.style_pop_font(gui.ctx)
	end
	return gui
end

local add_button = function(gui,name,text)
	local used = (gui.cfg.font.use == name)
	local font = gui.fonts[name]
	if used == false then
		nk.style_push_font(gui.ctx, font)
	end
	if nk.button(gui.ctx, nil, text) then
		 gui.cfg.font.use = name
	end
	if used == false then
		nk.style_pop_font(gui.ctx)
	end
	return gui
end

local function update_fonts(gui)
	if gui.font.file ~= used.file then
		-- atlas needs to be respawned, window will be restarted
		gui.reboot = gasp.set_reboot_gui(true)
		glfw.set_window_should_close(gui.window, true)
	end
	return gui
end

return function(gui,ctx,now,prv)
	local font = get_font(gui)
	local xxlarge = get_font(gui,"xx-large")
	local used = gui.cfg.font
	gui = gui.draw_reboot(gui,ctx)
	gui = gui.draw_goback(gui,ctx,prv,update_fonts)
	gui = add_label(gui,gui.cfg.font.use,"Change font size to:")
	nk.layout_row_dynamic(ctx, pad_height(xxlarge,text), 7 )
	gui = add_button( gui, 'xx-large', 'XX-Large')
	gui = add_button( gui, 'x-large', 'X-Large')
	gui = add_button( gui, 'large', 'Large')
	gui = add_button( gui, 'medium', 'Medium')
	gui = add_button( gui, 'small', 'Small')
	gui = add_button( gui, 'x-small', 'X-Small')
	gui = add_button( gui, 'xx-small', 'XX-Small')
	nk.layout_row_dynamic(ctx, pad_height(font,text), 1 )
	return gui
end
