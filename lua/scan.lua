return function (gui,ctx,prv)
	local font = get_font(gui)
	local scan = gui.scan or {
		from = 0, upto = 0x7FFFFFFF
	}
	gui = gui.draw_reboot(gui,ctx)
	gui = gui.draw_goback(gui,ctx,prv)
	nk.layout_row_dynamic( ctx, pad_height(font,"Scan"), 2 )
	nk.label( ctx, "From:", nk.TEXT_LEFT )
	scan.from = tonumber(
		nk.edit_string(
			ctx, nk.EDIT_FIELD, tostring( scan.from, 16 ), 30 ), 16
	)
	nk.label( ctx, "Upto:", nk.TEXT_LEFT )
	scan.upto = tonumber(
		nk.edit_string(
			ctx, nk.EDIT_FIELD, tostring( scan.upto, 16 ), 30 ), 16
	)
	if nk.button( ctx, nil, "1st Scan" ) then
		-- TODO: Implement reset
		-- gui.handle:aobinit( scan.text, from, upto, true, 1000 )
	end
	if nk.button( ctx, nil, "Scan" ) then
		-- TODO: Modify aobscan then implement GUI usage
		-- Using parameters I remember offhand
		-- gui.handle:aobscan( scan.text, from, upto, true, 1000 )
	end
	gui = gui.draw_cheat( gui, ctx, font, { addr = 0 } )
	gui.scan = scan
	return gui
end
