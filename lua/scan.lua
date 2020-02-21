return function (gui,ctx,now,prv)
	local font = get_font(gui)
	local scan = gui.scan or {}
	local list, tmp, i, v
	scan.find = scan.find or ""
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
	if nk.button( ctx, nil, "Clear" ) then
		done = {
			count = 0,
			from = 0,
			upto = scan.from,
			list = {},
			found = 0,
			increment = math.floor((scan.upto - scan.from) / 10)
		}
	end
	if nk.button( ctx, nil, "Scan" ) then
		done.first_draw = nil
		done.count = done.count + 1
		done.from = 0
		done.upto = scan.from
		done.list = done.list or {}
		done.found = 0
		done.increment = math.floor((scan.upto - scan.from) / 10)
	end
	scan.done = done
	gui.scan = scan
	if done.count > 0 and not gui.quit then
		nk.layout_row_dynamic( ctx, pad_height( font, "%" ), 1 )
		nk.progress( ctx,
			done.upto - scan.from, scan.upto - scan.from, nk.FIXED )
		if not gui.handle or gui.handle:valid() == false then
			return gui.use_ui(gui,ctx,"cfg-proc",now)
		end
		if not done.first_draw then
			done.first_draw = true
			return gui
		end
		list = done.list
		i = nil
		if scan.Type == "text" then
			tmp, i = gasp.str2bytes( scan.find )
		elseif scan.Type == "bytes" then
			tmp = scan.find
		elseif scan.Type == "signed" then
			tmp, i = gasp.int2bytes( scan.find )
			--[[
			if gui.cheat.endian == "Big" then
				tmp = gasp.flipbytes( i, tmp )
			end
			--]]
		else
			tmp = nil
		end
		done.from = done.upto
		if done.from > scan.from then
			done.from = done.from - scan.size
		end
		done.upto = done.upto + done.increment
		if done.upto > scan.upto then done.upto = scan.upto end
		if tmp and done.upto < scan.upto then
			v = nil
			if i then
				tmp, v = gui.handle:aobscan(
					i, tmp, done.from, done.upto,
					false, scan.limit, done.count - 1 )
			else
				tmp, v = gui.handle:aobscan(
					tmp, done.from, done.upto,
					false, scan.limit, done.count - 1 )
			end
			if tmp and v then
				for i = 1,v,1 do
					if done.found == scan.limit then break end
					done.found = done.found + 1
					list[done.found] = {
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
		for i = 1,done.found,1 do
			gui = gui.draw_cheat( gui, ctx, font, list[i] )
		end
	end
	scan.done = done
	gui.scan = scan
	return gui
end
