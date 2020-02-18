return function (gui,ctx,now,prv)
	local font = get_font(gui)
	local scan = gui.scan or {}
	local i, v
	scan.find = scan.find or ""
	scan.from = scan.from or 0
	scan.upto = scan.upto or 0x7FFFFFFF
	scan.Type = scan.Type or "signed"
	scan.TypeSelected = scan.TypeSelected or 1
	gui = gui.draw_reboot(gui,ctx)
	gui = gui.draw_goback(gui,ctx,now,prv)
	nk.layout_row_dynamic( ctx, pad_height(font,"Type"), 2 )
	nk.label( ctx, "Type:", nk.TEXT_LEFT )
	scan, tmp = gui.draw_type_field(gui,ctx,font,scan)
	nk.layout_row_dynamic( ctx, pad_height(font,"Size:"), 4 )
	nk.label( ctx, "Size:", nk.TEXT_LEFT )
	scan, tmp = gui.draw_size_field(gui,ctx,font,scan)
	nk.layout_row_dynamic( ctx, pad_height(font,"Find:"), 2 )
	nk.label( ctx, "Find:", nk.TEXT_LEFT )
	scan.find = nk.edit_string( ctx, nk.EDIT_FIELD, scan.find, 30 )
	nk.label( ctx, "From:", nk.TEXT_LEFT )
	scan.from = tonumber(
		nk.edit_string(
			ctx, nk.EDIT_FIELD,
				string.format( "%X", scan.from ), 30 ), 16
	)
	nk.label( ctx, "Upto:", nk.TEXT_LEFT )
	scan.upto = tonumber(
		nk.edit_string(
			ctx, nk.EDIT_FIELD,
				string.format( "%X", scan.upto ), 30 ), 16
	)
	if nk.button( ctx, nil, "1st Scan" ) then
		if not gui.handle or gui.handle:valid() == false then
			return gui.use_ui(gui,ctx,"cfg-proc",now)
		end
		-- TODO: Implement reset
		-- gui.handle:aobinit( scan.text, from, upto, true, 1000 )
	end
	gui.scan = scan
	if nk.button( ctx, nil, "Scan" ) then
		if not gui.handle or gui.handle:valid() == false then
			return gui.use_ui(gui,ctx,"cfg-proc",now)
		end
		-- TODO: Modify aobscan then implement GUI usage
		-- Using parameters I remember offhand
		-- gui.handle:aobscan( scan.text, from, upto, true, 1000 )
	end
	gui = gui.draw_cheat( gui, ctx, font, { addr = 0 } )
	return gui
end
