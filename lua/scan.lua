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
	
	if done.count == 0 then
		nk.layout_row_dynamic( ctx, pad_height(font,"Type"), 2 )
		nk.label( ctx, "Type:", nk.TEXT_LEFT )
		scan = GUI.draw_type_field(ctx,font,scan)
		
		nk.layout_row_dynamic( ctx, pad_height(font,"Size:"), 4 )
		nk.label( ctx, "Size:", nk.TEXT_LEFT )
		scan = GUI.draw_size_field(ctx,font,scan)
	end
	
	nk.layout_row_dynamic( ctx, pad_height(font,"Find:"), 2 )
	nk.label( ctx, "Find:", nk.TEXT_LEFT )
	scan = GUI.draw_edit_field(ctx,font,scan)
	
	if done.count == 0 and
		GUI.cheat and GUI.cheat.app and GUI.cheat.app.regions then
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
	if done.count == 0 then
		tmp = GUI.draw_addr_field( ctx, font, { addr = scan.from } )
		scan.from = tmp.addr
	else
		nk.label( ctx, string.format( "0x%X", scan.from ), nk.TEXT_LEFT )
	end
		
	nk.label( ctx, "Upto:", nk.TEXT_LEFT )
	if done.count == 0 then
		tmp = GUI.draw_addr_field( ctx, font, { addr = scan.upto } )
		scan.from = tmp.addr
	else
		nk.label( ctx, string.format( "0x%X", scan.upto ), nk.TEXT_LEFT )
	end
	if done.count == 0 then
		nk.layout_row_dynamic( ctx, pad_height(font,"Dump"), 3 )
		if nk.button( ctx, nil, "Dump" ) then
			done = {
				count = 1,
				just_dump = true,
				from = scan.from,
				upto = scan.from,
				addr = scan.from,
				found = 0,
				increment = math.ceil((scan.upto - scan.from) / 0x16)
			}
		end
	else
		nk.layout_row_dynamic( ctx, pad_height(font,"Type"), 2 )
	end
	
	if nk.button( ctx, nil, "Scan" ) then
		done = {
			count = done.count + 1,
			from = scan.from,
			upto = scan.from,
			addr = scan.from,
			found = 0,
			increment = math.ceil((scan.upto - scan.from) / 0x16)
		}
		if not GUI.handle or GUI.handle:valid() == false then
			return GUI.use_ui(ctx,"cfg-proc",now)
		end
		if GUI.handle:thread_active() == false then
			if done.just_dump == true then
				GUI.handle:dump( scan.from,upto, true, scan.limit )
				
			else
				GUI.handle:bytescan(
					"table", scan.size, scan.as_bytes,
					scan.from, scan.upto,
					true, scan.limit, done.count - 1 )
			end
		end
	end
	
	if nk.button( ctx, nil, "Clear" ) then
		done = {
			count = 0,
			from = scan.from,
			upto = scan.from,
			addr = scan.from,
			found = 0,
			increment = math.ceil((scan.upto - scan.from) / 0x16)
		}
	end
	
	scan.done = done
	GUI.scan = scan
	
	if done.count > 0 then
		list = nil
		if GUI.handle:dumping() then
			list = done.list
		elseif GUI.handle:scan_done_upto() < done.upto then
			list = done.list
		else
			done.added = 0
			done.addr, done.found, tmp =
				GUI.handle:get_scan_list(done.count - 1,scan.limit)
			if done.found > 0 then
				list = {}
				for i = 1,done.found,1 do
					if i == scan.limit then break end
					list[i] = rebuild_cheat(scan)
					list[i].generated = true
					list[i].addr = tmp[i] or 0
					-- Prevent unintended subgroups
					list[i].count = nil
					list[i].list = nil
					list[i].prev = nil
					list[i].split = nil
					list[i].is_group = false
					list[i].active = false
				end
			end
			done.added = i;
			done.upto = done.upto + done.increment
			if done.upto > scan.upto then done.upto = scan.upto end
		end
		done.list = list
	end
	
	if done.count > 0 then
	
		nk.layout_row_dynamic( ctx, pad_height( font, "%" ), 1 )
		
		if GUI.handle:dumping() == true then
			tmp = "Thread is dumping"
		elseif GUI.handle:scanning() == true then
			tmp = "Scan thread is scanning"
		else
			tmp = "Thread is not running"
		end
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		
		tmp = GUI.handle:scan_done_upto()
		nk.progress( ctx, tmp, scan.upto, nk.FIXED )
		
		tmp = string.format( "Scanned upto 0x%X", tmp )
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		
		done.desc = string.format( "Showing %d of %d Results",
			#(done.list or {}), done.found )
		
		nk.label( ctx, done.desc, nk.TEXT_LEFT )
		
		if done.list then
			GUI.draw_cheat( ctx, font, done )
		end
	end
	scan.done = done
	GUI.scan = scan
	GUI.keep_cheat = nil
end
