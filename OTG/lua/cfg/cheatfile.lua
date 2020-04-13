return function(ctx,now,prv)
	local font = get_font()
	local path = cheatspath()
	local dir = scandir(path)
	local text
	
	GUI.draw_reboot(ctx)
	GUI.draw_goback(ctx,now,prv)
	
	if dir and #dir > 0 then
		text = "Cheat files in:"
		nk.layout_row_dynamic(ctx,pad_height(font,text),2)
		nk.label(ctx, text, nk.TEXT_LEFT)
		nk.label(ctx, path, nk.TEXT_LEFT)
		nk.layout_row_dynamic( ctx, pad_height(font,path), 1 )
		
		for i,v in pairs(dir) do
			if gasp.path_isfile(path .. '/' .. v) == 1 and
				has_ext(v,".lua") then
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
	
	if nk.button( ctx, nil, "Load" ) then
		GUI.which = prv
		autoload()
	end
end
