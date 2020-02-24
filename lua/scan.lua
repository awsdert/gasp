return function (gui,ctx,now,prv)
	local font = get_font(gui)
	local scan = gui.scan or {}
	local list, tmp, i, v
	scan.as_text = scan.as_text or ""
	scan.from = scan.from or 0
	scan.upto = scan.upto or 0x7FFFFFFF
	scan.Type = scan.Type or "signed"
	scan.limit = scan.limit or 100
	done = scan.done or { count = 0, from = 0, upto = scan.from }
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
	scan = gui.draw_edit_field(gui,ctx,font,scan)
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
	if nk.button( ctx, nil, "Clear" ) then
		done = {
			count = 0,
			from = 0,
			upto = scan.from,
			list = {},
			found = 0,
			added =0,
			increment = math.ceil((scan.upto - scan.from) / 0x16)
		}
	end
	if nk.button( ctx, nil, "Scan" ) then
		done.first_draw = nil
		done.count = done.count + 1
		done.from = 0
		done.upto = scan.from
		done.list = done.list or {}
		done.found = 0
		done.added = 0
		done.increment = math.ceil((scan.upto - scan.from) / 0x16)
	end
	scan.done = done
	gui.scan = scan
	if done.count > 0 and not gui.quit then
		if not gui.handle or gui.handle:valid() == false then
			return gui.use_ui(gui,ctx,"cfg-proc",now)
		end
		if gui.handle:doing_scan() or
			gui.handle:done_scans() == done.count then
			if gui.handle:scan_done_upto() < done.upto then
				list = done.list
			else
				list = {}
				done.added = 0
				done.upto, tmp, done.found =
					gui.handle:get_scan_list(done.count - 1)
				for i = 1,done.found,1 do
					if done.added == scan.limit then break end
					done.added = done.added + 1
					list[done.added] = {
						generated = true,
						method = "=",
						addr = tmp[i] or 0,
						Type = scan.Type,
						size = scan.size
					}
				end
			end
		else
			if not done.first_draw then
				done.first_draw = true
				return gui
			end
			list = done.list
			i = nil
			done.from = done.upto
			if done.from > scan.from then
				done.from = done.from - scan.size
			end
			done.upto = done.upto + done.increment
			if done.upto > scan.upto then done.upto = scan.upto end
			if tmp and done.upto < scan.upto then
				v = nil
				tmp, v = gui.handle:aobscan(
					"table", scan.size, scan.as_bytes,
					done.from, done.upto,
					false, scan.limit, done.count - 1 )
				if tmp and v then
					for i = 1,v,1 do
						if done.added == scan.limit then break end
						done.added = done.added + 1
						list[done.added] = {
							generated = true,
							method = "=",
							addr = tmp[i] or 0,
							Type = scan.Type,
							size = scan.size
						}
					end
					tmp = nil
				end
			end
		end
		done.list = list
	end
	if done.list then
		list = done.list
		nk.layout_row_dynamic( ctx, pad_height( font, "%" ), 1 )
		tmp = gui.handle:scan_done_upto()
		nk.progress( ctx, tmp, scan.upto, nk.FIXED )
		tmp = string.format( "Scanned upto 0x%X", tmp )
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		tmp = string.format( "Showing %d of %d Results",
			done.added, gui.handle:scan_found() )
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		for i = 1,done.added,1 do
			gui = gui.draw_cheat( gui, ctx, font, list[i] )
		end
	end
	scan.done = done
	gui.scan = scan
	return gui
end
