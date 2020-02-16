return function(gui,ctx,prv)
	local font = get_font(gui), ok, glance, id, i, v, selected
	local text = "Noticed Processes"
	if type(gui.glance) ~= "table" then
		gui.glance = {}
	end
	nk.layout_row_dynamic(ctx, pad_height(font,text), 2)
	nk.label( ctx, "Process", nk.TEXT_LEFT )
	cfg.find_process = nk.edit_string(
		ctx, nk.EDIT_SIMPLE, (cfg.find_process or ""), 100 )
	nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
	if #(cfg.find_process) > 0 then
		glance = gasp.locate_app(cfg.find_process)
	else
		glance = gasp.locate_app()
		--glance = get_all_apps()
	end
	gui.idc = gui.idc or 0
	if glance and #glance > 0 then
		nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
		if nk.tree_push( ctx, nk.TREE_NODE,
		text, nk.MAXIMIZED, gui.idc ) then
			gui.idc = gui.idc + 1
			for i = 1,#glance,1 do
				notice=glance[i]
				id = gui.idc
				text = "" .. notice.entryId .. " " .. notice.cmdl
				if gui.noticed then
					selected = (gui.noticed.entryId == notice.entryId)
				else selected = false end
				ok, selected = add_tree_node(ctx,text,selected,gui.idc)
				gui.idc = gui.idc + 1
				if ok then
					if selected then
						gui.noticed = notice
					end
				else break end
			end
			nk.tree_pop(ctx)
		end
	end
	if nk.button(ctx, nil, "Done") then
		gui.which = prv
		gui = hook_process(gui)
	end
	return gui
end
