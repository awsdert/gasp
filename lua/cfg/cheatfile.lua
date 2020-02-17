return function(gui,ctx,prv)
	local font = get_font(gui)
	local text = get_cheat_dir()
	local dir = scandir(text)
	if dir and #dir > 0 then
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label(ctx, "Cheat files in:", nk.TEXT_LEFT)
		nk.label(ctx, text, nk.TEXT_LEFT)
		nk.layout_row_dynamic( ctx, pad_height(font,text),1)
		for i,v in pairs(dir) do
			if gasp.path_isfile(text .. '/' .. v) == 1 then
				if gui.cheatfile then
					i = (gui.cheatfile == v)
				else
					i = false
				end
				i = nk.selectable( ctx, nil, v, nk.TEXT_LEFT, i )
				if i == true then
					gui.cheatfile = v
				end
			end
		end
	end
	if nk.button( ctx, nil, "Done" ) then
		gui.which = prv
		if gui.cheatfile then
			gui.cheat = dofile(text .. "/" .. gui.cheatfile) or {}
		else
			gui.cheat = {}
		end
	end
	if gui.cheat.app then
		-- Auto fill search box when the process list is opened
		cfg.find_process =
			gui.cheat.app.name or cfg.find_process
		dir = gasp.locate_app(cfg.find_process)
		-- Auto hook process
		if #dir == 1 then
			gui.noticed = dir[1]
			gui = hook_process(gui)
		end
	end
	return gui
end
