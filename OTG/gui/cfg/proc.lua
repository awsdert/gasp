return function( now,prv)
	local font = get_font(), ok, glance, id, i, v, selected
	local text = "Noticed Processes"
	_G.GUI.draw_reboot()
	_G.GUI.draw_goback(now,prv)
	
	if type(GUI.glance) ~= "table" then
		GUI.glance = {}
	end
	
	nk.layout_row_dynamic( _G.gui.ctx, pad_height(font,text), 2)
	nk.label( _G.gui.ctx, "Process", nk.TEXT_LEFT )
	_G.pref.find_process = nk.edit_string(
		_G.gui.ctx, nk.EDIT_SIMPLE, (_G.pref.find_process or ""), 100 )
	
	nk.layout_row_dynamic( _G.gui.ctx, pad_height(font,text), 1)
	if #(_G.pref.find_process) > 0 then
		glance = gasp.locate_app(_G.pref.find_process)
	else
		glance = gasp.locate_app()
	end
	
	GUI.idc = GUI.idc or 0
	if glance and #glance > 0 then
		nk.layout_row_dynamic( _G.gui.ctx, pad_height(font,text), 1)
		if nk.tree_push( _G.gui.ctx, nk.TREE_NODE,
		text, nk.MAXIMIZED, GUI.idc ) then
			GUI.idc = GUI.idc + 1
			for i = 1,#glance,1 do
				v=glance[i]
				id = GUI.idc
				text = "" .. v.entryId .. " " .. v.cmdl
				if GUI.noticed then
					selected = (GUI.noticed.entryId == v.entryId)
				else selected = false end
				ok, selected = add_tree_node( text,selected,GUI.idc)
				GUI.idc = GUI.idc + 1
				if ok then
					if selected then
						GUI.noticed = v
					end
				else break end
			end
			nk.tree_pop( _G.gui.ctx )
		end
	end
end
