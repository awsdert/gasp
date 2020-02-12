return function(gui,ctx,prv)
	local font = get_font(gui)
	local ok, selected
	local text = "Noticed Processes"
	local glance = class_proc_glance.new()
	if glance then
		local notice = glance:init("gasp")
		if notice then
			nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
			if nk.tree_push( ctx, nk.TREE_NODE,
			text, nk.MAXIMIZED, gui.idc ) then
				gui.idc = gui.idc + 1
				while notice do
					text = "" .. notice.entryId  .. " " .. notice.name
					ok, gui.selected[gui.idc] = nk.tree_element_push(
						ctx, nk.TREE_NODE, text, nk.MAXIMIZED,
						(gui.selected[gui.idc] or false), gui.idc )
					if ok then
						gui.idc = gui.idc + 1
						notice = glance:next()
					else break end
				end
				nk.tree_pop(ctx)
			end
		end
		glance:term()
	end
	nk.layout_row_dynamic(ctx, pad_height(font,text), 1 )
	if nk.button(ctx, nil, "Done") then
		 gui.which = prv
	end
	return gui
end
