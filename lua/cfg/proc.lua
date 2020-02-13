local function add_tree_node(gui,ctx,text,isparent)
	local ok
	local id = (gui.idc or 0)
	gui.idc = id + 1
	if isparent == true then
		gui.selected[id] =
			nk.selectable(
			ctx, nil, text, nk.TEXT_LEFT,
			(gui.selected[id] or false) )
		return true, gui
	end
	ok, gui.selected[id] =
		nk.tree_element_push(
		ctx, nk.TREE_NODE, text, nk.MAXIMIZED,
		(gui.selected[id] or false), id )
	if ok then
		nk.tree_element_pop(ctx)
		return true, gui
	end
	return false, gui
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
	if glance then
		local notice = glance:init()
		if notice then
			nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
			if nk.tree_push( ctx, nk.TREE_NODE,
			text, nk.MAXIMIZED, gui.idc ) then
				gui.idc = gui.idc + 1
				while notice do
					text = "" .. notice.entryId  .. " " .. notice.name
					ok, gui = add_tree_node(gui,ctx,text)
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
	local font = get_font(gui), ok, glance, id, i, v
	local text = "Noticed Processes"
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
	if glance and #glance > 0 then
		nk.layout_row_dynamic(ctx, pad_height(font,text), 1)
		if nk.tree_push( ctx, nk.TREE_NODE,
		text, nk.MAXIMIZED, gui.idc ) then
			i = 1
			gui.process = {}
			gui.idc = gui.idc + 1
			for i,notice in pairs(glance) do
				id = gui.idc
				text = "" .. notice.entryId .. " " .. notice.name
				ok, gui = add_tree_node(gui,ctx,text,false)
				if ok then
					if gui.selected[id] then
						gui.process[i] = notice
						i = i + 1
					end
				else break end
			end
			nk.tree_pop(ctx)
		else gui.process = nil end
	else gui.process = nil end
	if nk.button(ctx, nil, "Done") then
		 gui.which = prv
		 gui.hooks = {}
		 for i,v in pairs(gui.process) do
			-- Hooking is not yet supported via lua
			gui.hooks[i] = nil
		 end
	end
	return gui
end
