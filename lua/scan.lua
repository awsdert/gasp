return function (ctx,now,prv)
	local font = get_font()
	local scan = GUI.scan or {}
	local list, tmp, i, v, p
	local function range2str(from,upto,desc)
		return ((string.format( "%X - %X %s", from, upto, desc ))
			or "(nil)")
	end
	GUI.keep_cheat = nil
	scan.as_text = scan.as_text or ""
	scan.from = scan.from or 0
	scan.upto = scan.upto or gasp.ptrlimit()
	scan.Type = scan.Type or "signed"
	scan.limit = scan.limit or 100
	scan.done = scan.done or { count = 0, from = 0, upto = scan.from }
	scan.TypeSelected = scan.TypeSelected or 1
	scan = rebuild_cheat(scan)
	
	done = scan.done
	done.count = done.count or 0
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
	
	if done.count == 0 then
	
		list = {
			txt = {},
			raw = {
				from = 0, upto = gasp.ptrlimit(), desc = "Default"
			}
		}
		
		if GUI.cheat and GUI.cheat.app then
			tmp = GUI.cheat.app.regions or {}
			for i = 1,#tmp,1 do
				v = tmp[i]
				list.raw[i+1] = v
			end
		end
		
		tmp = list.raw
		for i = 1,#tmp,1 do
			v = tmp[i]
			list.txt[i] = range2str( v.from, v.upto, v.desc )
		end
		
		if not scan.region or scan.region < 1 then
			scan.region = 1
		end
		
		tmp = list.txt
		v = tmp[1]
		scan.region = nk.combo(
			ctx, tmp, scan.region,
			pad_height( font, v ),
			{
				pad_width( font, v ) * 2,
				pad_height( font, v ) * 2
			}
		)
		
		if nk.button( ctx, nil, "Use" ) then
			v = list.raw[scan.region]
			scan.from = v.from
			scan.upto = v.upto
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
		scan.upto = tmp.addr
	else
		nk.label( ctx, string.format( "0x%X", scan.upto ), nk.TEXT_LEFT )
	end
	
	nk.layout_row_dynamic( ctx, pad_height(font,"Type"), 3 )
	
	if done.count == 0 then
		if nk.button( ctx, nil, "Dump" ) then
			done = {
				count = 1,
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
				GUI.handle:dump( scan.from,upto, true, scan.limit )
			end
		end
	else
		nk.label( ctx, "Dump", nk.TEXT_CENTERED )
	end
	
	if not GUI.handle or GUI.handle:thread_active() == false then
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
				GUI.handle:bytescan(
					"table", scan.size, scan.as_bytes,
					scan.from, scan.upto,
					true, scan.limit, done.count - 1 )
			end
		end
	else
		nk.label( ctx, "Scan", nk.TEXT_CENTERED )
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
		if GUI.handle then
			GUI.handle:killscan()
		end
	end
	
	scan.done = done
	GUI.scan = scan
	
	p = {
		active = false,
		dumping = false,
		scanning = false,
		addr = 0,
		value = 0.0,
		found = 0
	} 
	if GUI.handle then
		p.active, p.dumping, p.scanning, p.addr, p.value, p.found =
			GUI.handle:percentage_done()
	end
	
	if done.count > 0 then
	
		nk.layout_row_dynamic( ctx, pad_height( font, "%" ), 1 )
		
		if p.dumping == true then
			tmp = "Thread is dumping, " .. p.value .. "% done"
		elseif p.scanning == true then
			tmp = "Scan thread is scanning, " .. p.value .. "% done"
		else
			tmp = "Thread is not running"
		end
		tmp =  tmp .. ", " .. p.found .. " found"
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		
		nk.progress( ctx, math.floor(p.value), 100, nk.FIXED )
		
		if scan.dumping == true then
			tmp = string.format( "Dumping from 0x%X", p.addr )
		else
			tmp = string.format( "Scanned upto 0x%X", p.addr )
		end
		nk.label( ctx, tmp, nk.TEXT_LEFT )
		
		list = {}
		if GUI.handle:dumping() == false and not done.use then
			if p.addr < done.upto then
				list = done.list or {}
			else
				done.addr, done.found, tmp =
					GUI.handle:get_scan_list(done.count - 1,scan.limit)
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
				if done.upto == scan.upto then
					done.use = true
				end
				done.upto = done.upto + done.increment
				if done.upto > scan.upto then done.upto = scan.upto end
			end
		else
			list = done.list or {}
		end
				
		if #list > 0 then
			done.desc = string.format( "Showing upto %d of %d Results",
				#list, done.found )
			nk.label( ctx, done.desc, nk.TEXT_LEFT )
		
			done.list = list
			done = GUI.draw_cheats( ctx, font, { list = list } )
		end
	end
	scan.done = done
	GUI.scan = scan
	GUI.keep_cheat = nil
end
