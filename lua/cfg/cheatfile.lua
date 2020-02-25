return function(ctx,now,prv)
	local font = get_font(GUI)
	local text = get_cheat_dir(GUI)
	local dir = scandir(text)
	GUI.draw_reboot(ctx)
	GUI.draw_goback(ctx,now,prv)
	GUI.cheat = nil
	if dir and #dir > 0 then
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label(ctx, "Cheat files in:", nk.TEXT_LEFT)
		nk.label(ctx, text, nk.TEXT_LEFT)
		nk.layout_row_dynamic( ctx, pad_height(font,text), 1 )
		
		for i,v in pairs(dir) do
			if gasp.path_isfile(text .. '/' .. v) == 1 and
				v:match("lua") then
				if GUI.cheatfile then
					i = (GUI.cheatfile == v)
				else
					i = false
				end
				i = nk.selectable( ctx, nil, v, nk.TEXT_LEFT, i )
				if i == true then
					GUI.cheatfile = v
				end
			end
		end
	end
	if nk.button( ctx, nil, "Done" ) then
		GUI.which = prv
		if GUI.cheatfile then
			GUI.cheat = dofile(text .. "/" .. GUI.cheatfile)
		end
	end
	if GUI.cheat and GUI.cheat.app then
		-- Auto fill search box when the process list is opened
		GUI.cfg.find_process =
			GUI.cheat.app.name or GUI.cfg.find_process
		dir = gasp.locate_app(GUI.cfg.find_process)
		-- Auto hook process
		if #dir == 1 then
			GUI.noticed = dir[1]
			return hook_process(GUI)
		else
			return GUI.use_ui(ctx,"cfg-proc",now)
		end
	end
	return GUI
end
