local function example(gui,ctx,prv)
	local font = get_font(gui)
	local ok, selected
	local text = "Noticed Processes"
	local glance = class_proc_glance.new()
	if glance then
		local notice = glance:init()
		if notice then
			nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
			if nk.tree_push( ctx, nk.TREE_NODE,
			text, nk.MAXIMIZED, gui.idc ) then
				gui.idc = gui.idc + 1
				while notice do
					text = "" .. notice.entryId  .. " " .. notice.name
					gui.selected[gui.idc] = nk.selectable(
						ctx, nil, text, nk.TEXT_LEFT,
						(gui.selected[gui.idc] or false) )
					gui.idc = gui.idc + 1
					notice = glance:next()
				end
			end
			nk.tree_pop(ctx)
		end
		glance:term()
	end
	nk.layout_row_dynamic(ctx, pad_height(font,text), 1 )
	if nk.button(ctx, nil, "Done") then
		 gui.which = prv
	end
	return gui
end
return function(gui,ctx,prv)
	local font = get_font(gui)
	local ok, selected
	local text = "Noticed Processes"
	local glance = proc_locate_name("gasp")
	if glance and #glance > 0 then
		nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
		if nk.tree_push( ctx, nk.TREE_NODE,
		text, nk.MAXIMIZED, gui.idc ) then
			gui.idc = gui.idc + 1
			nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
			for i,notice in pairs(glance) do
				text = "" .. notice.entryId .. " " .. notice.name
				gui.selected[gui.idc] = nk.selectable(
					ctx, nil, text, nk.TEXT_LEFT,
					(gui.selected[gui.idc] or false) )
				gui.idc = gui.idc + 1
			end
			nk.tree_pop(ctx)
		end
	end
	nk.layout_row_dynamic(ctx, pad_height(font,text), 1 )
	if nk.button(ctx, nil, "Done") then
		 gui.which = prv
	end
	return gui
end
