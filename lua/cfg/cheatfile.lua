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
			gui.cheat = dofile(text .. "/" .. gui.cheatfile)
		else
			gui.cheat = {}
		end
	end
	return gui
end
