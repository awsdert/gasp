local function add_tree_node(gui,ctx)
	-- Will modify after upload of working example
end
local function list_all_apps(gui,ctx,prv)
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
	local font = get_font(gui), ok
	local text = "Noticed Processes"
	nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
	local glance = proc_locate_name("gasp")
	if glance and #glance > 0 then
		nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
		if nk.tree_push( ctx, nk.TREE_NODE,
		text, nk.MAXIMIZED, gui.idc ) then
			gui.idc = gui.idc + 1
			for i,notice in pairs(glance) do
				text = "" .. notice.entryId .. " " .. notice.name
				ok, gui.selected[gui.idc] =
					nk.tree_element_push(
					ctx, nk.TREE_NODE, text, nk.MAXIMIZED,
					(gui.selected[gui.idc] or false), gui.idc )
				gui.idc = gui.idc + 1
				if ok then nk.tree_element_pop(ctx)
				else break end
			end
			nk.tree_pop(ctx)
		end
	end
	if nk.button(ctx, nil, "Done") then
		 gui.which = prv
	end
	return gui
end
