return function (ctx,now,prv)
	local font = get_font()
	local scan = GUI.scan or {}
	local list, tmp, i, v
	GUI.keep_cheat = nil
	scan.as_text = scan.as_text or ""
	scan.from = scan.from or 0
	scan.upto = scan.upto or 0x7FFFFFFF
	scan.Type = scan.Type or "signed"
	scan.limit = scan.limit or 100
	scan.done = scan.done or { count = 0, from = 0, upto = scan.from }
	scan.TypeSelected = scan.TypeSelected or 1
	scan = rebuild_cheat(scan)
	
	done = scan.done
	GUI.draw_reboot(ctx)
	GUI.draw_goback(ctx,now,prv)
	
	nk.layout_row_dynamic( ctx, pad_height(font,"Type"), 2 )
	nk.label( ctx, "Type:", nk.TEXT_LEFT )
	scan = GUI.draw_type_field(ctx,font,scan)
	
	nk.layout_row_dynamic( ctx, pad_height(font,"Size:"), 4 )
	nk.label( ctx, "Size:", nk.TEXT_LEFT )
	scan = GUI.draw_size_field(ctx,font,scan)
	
	nk.layout_row_dynamic( ctx, pad_height(font,"Find:"), 2 )
	nk.label( ctx, "Find:", nk.TEXT_LEFT )
	scan = GUI.draw_edit_field(ctx,font,scan)
	
	if GUI.cheat and GUI.cheat.app and GUI.cheat.app.regions then
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
			for i=1,#(GUI.cheat.app.regions),1 do
				list[i+1] = GUI.cheat.app.regions[i]
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
	tmp = GUI.draw_addr_field( ctx, font, { addr = scan.from } )
	scan.from = tmp.addr
	
	nk.label( ctx, "Upto:", nk.TEXT_LEFT )
	tmp = GUI.draw_addr_field( ctx, font, { addr = scan.upto } )
	scan.upto = tmp.addr
	
	if nk.button( ctx, nil, "Clear" ) then
		done = {
			count = 0,
			from = 0,
			upto = scan.from,
			found = 0,
			added = 0,
			increment = math.ceil((scan.upto - scan.from) / 0x16)
		}
	end
	if nk.button( ctx, nil, "Scan" ) then
		done.first_draw = nil
		done.count = done.count + 1
		done.from = 0
		done.upto = scan.from
		done.found = 0
		done.added = 0
		done.increment = math.ceil((scan.upto - scan.from) / 0x16)
	end
	scan.done = done
	GUI.scan = scan
	if done.count > 0 and not GUI.quit then
		if not GUI.handle or GUI.handle:valid() == false then
			return GUI.use_ui(ctx,"cfg-proc",now)
		end
		if GUI.handle:doing_scan() or
			GUI.handle:done_scans() == done.count then
			if GUI.handle:scan_done_upto() < done.upto then
				list = done.list
			else
				list = {}
				done.added = 0
				done.upto, tmp, done.found =
					GUI.handle:get_scan_list(done.count - 1)
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
				tmp, v = GUI.handle:aobscan(
					"table", scan.size, scan.as_bytes,
					scan.from, scan.upto,
					true, scan.limit, done.count - 1 )
				done.from = scan.from
				done.upto = scan.upto
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
	if done.count > 0 then
		list = done.list
		nk.layout_row_dynamic( ctx, pad_height( font, "%" ), 1 )
		
		if GUI.handle:doing_scan() == true then
			tmp = "Scan thread is running"
		else
			tmp = "Scan thread is not running"
		end
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		
		tmp = GUI.handle:scan_done_upto()
		nk.progress( ctx, tmp, scan.upto, nk.FIXED )
		
		tmp = string.format( "Scanned upto 0x%X", tmp )
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		
		tmp = string.format( "Showing %d of %d Results",
			done.added, GUI.handle:scan_found() )
		
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		for i = 1,done.added,1 do
			GUI.draw_cheat( ctx, font, list[i] )
		end
	end
	scan.done = done
	GUI.scan = scan
	GUI.keep_cheat = nil
end
