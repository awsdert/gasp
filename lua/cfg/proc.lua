local function add_tree_node(ctx,text,selected,id,isparent)
	local ok
	if isparent == true then
		selected =
			nk.selectable(
			ctx, nil, text, nk.TEXT_LEFT,
			(selected or false) )
		return true, selected
	end
	ok, selected =
		nk.tree_element_push(
		ctx, nk.TREE_NODE, text, nk.MAXIMIZED,
		(selected or false), id )
	if ok then
		nk.tree_element_pop(ctx)
		return true, selected
	end
	return false, selected
end
local function get_all_apps(underId)
	if type(underId) ~= "integer" or underId < 0 then
		underId = 0
	end
	local t = {}
	local i = 1
	local glance = class_proc_glance.new()
	if glance then
		t[i] = glance:init(underId)
		while t[i] do
			i = i + 1
			t[i] = glance:next()
		end
		glance:term()
	end
	return t
end
local function list_all_apps(gui,ctx,prv)
	local font = get_font(gui)
	local ok, selected
	local text = "Noticed Processes"
	local glance = class_proc_glance.new()
	if type(gui.glance) ~= "table" then
		gui.glance = {}
	end
	if glance then
		local notice = glance:init()
		if notice then
			nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
			if nk.tree_push( ctx, nk.TREE_NODE,
			text, nk.MAXIMIZED, gui.idc ) then
				gui.idc = gui.idc + 1
				while notice do
					text = "" .. notice.entryId  .. " " .. notice.name
					ok, selected = add_tree_node(
						ctx,text,gui.idc,gui.glance[notice.entryId])
					gui.idc = gui.idc + 1
					gui.glance[notice.entryId] = selected
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
		glance = proc_locate_name(cfg.find_process)
	else
		glance = get_all_apps()
	end
	gui.idc = gui.idc or 0
	if glance and #glance > 0 then
		nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
		if nk.tree_push( ctx, nk.TREE_NODE,
		text, nk.MAXIMIZED, gui.idc ) then
			i = 1
			gui.idc = gui.idc + 1
			for i,notice in pairs(glance) do
				id = gui.idc
				text = "" .. notice.entryId .. " " .. notice.name
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
		if gui.noticed then
			gui.handle = class_proc_handle.new()
			if not gui.handle:init( gui.noticed.entryId ) then
				gui.handle = nil
			end
		end
	end
	return gui
end
