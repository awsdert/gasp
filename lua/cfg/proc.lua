return function(ctx,now,prv)
	local font = get_font(), ok, glance, id, i, v, selected
	local text = "Noticed Processes"
	GUI.draw_reboot(ctx)
	GUI.draw_goback(ctx,now,prv)
	
	if type(GUI.glance) ~= "table" then
		GUI.glance = {}
	end
	
	nk.layout_row_dynamic(ctx, pad_height(font,text), 2)
	nk.label( ctx, "Process", nk.TEXT_LEFT )
	GUI.cfg.find_process = nk.edit_string(
		ctx, nk.EDIT_SIMPLE, (GUI.cfg.find_process or ""), 100 )
	
	nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
	if #(GUI.cfg.find_process) > 0 then
		glance = gasp.locate_app(GUI.cfg.find_process)
	else
		glance = gasp.locate_app()
	end
	
	GUI.idc = GUI.idc or 0
	if glance and #glance > 0 then
		nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
		if nk.tree_push( ctx, nk.TREE_NODE,
		text, nk.MAXIMIZED, GUI.idc ) then
			GUI.idc = GUI.idc + 1
			for i = 1,#glance,1 do
				v=glance[i]
				id = GUI.idc
				text = "" .. v.entryId .. " " .. v.cmdl
				if GUI.noticed then
					selected = (GUI.noticed.entryId == v.entryId)
				else selected = false end
				ok, selected = add_tree_node(ctx,text,selected,GUI.idc)
				GUI.idc = GUI.idc + 1
				if ok then
					if selected then
						GUI.noticed = v
					end
				else break end
			end
			nk.tree_pop(ctx)
		end
	end
end
