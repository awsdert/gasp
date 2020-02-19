return function (gui,ctx,now,prv)
	local font = get_font(gui)
	local scan = gui.scan or {}
	local list, tmp, i, v
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
	if gui.cheat and gui.cheat.app and gui.cheat.app.regions then
		if scan.regions then
			list = scan.regions
			tmp = scan.region_options
		else
			list = {}
			tmp = {}
			list[1] = { desc = "Default", from = 0, upto = 0x7FFFFFFF }
			tmp[1] =
				string.format( "%X - %X %s",
				list[1].from, list[1].upto, list[1].desc )
			for i=1,#(gui.cheat.app.regions),1 do
				list[i+1] = gui.cheat.app.regions[i]
				tmp[i+1] =
					string.format( "%X - %X %s",
					list[i+1].from, list[i+1].upto, list[i+1].desc )
			end
		end
		scan.regions = list
		scan.region_options = tmp
		scan.region = nk.combo(
			ctx, tmp, scan.region or 1,
			pad_height( font, tmp[1] ),
			{
				pad_width( font, tmp[1] ) * 2,
				pad_height( font, tmp[1] ) * 2
			}
		)
		if nk.button( ctx, nil, "Use" ) then
			scan.from = list[scan.region].from
			scan.upto = list[scan.region].upto
		end
	end
	nk.label( ctx, "From:", nk.TEXT_LEFT )
	tmp = gui.draw_addr_field( gui, ctx, font, { addr = scan.from } )
	scan.from = tmp.addr
	nk.label( ctx, "Upto:", nk.TEXT_LEFT )
	tmp = gui.draw_addr_field( gui, ctx, font, { addr = scan.upto } )
	scan.upto = tmp.addr
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
