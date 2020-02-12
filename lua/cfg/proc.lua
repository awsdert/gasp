return function(gui,ctx,prv)
	local ok, selected
	if nk.tree_push( ctx, nk.TREE_NODE,
		"Noticed Processes", "no", gui.idc ) then
		gui.idc = gui.idc + 1
		local glance = class_proc_glance()
		if type(glance) == "userdata" then
			local notice = glance:init("gasp")
			while notice do
				ok, selected = tree_element_push(
					ctx, nk.TREE_NODE,
					"" .. notice.id .. notice.name,
					"0", gui.idc)
				if ok then
					gui.idc = gui.idc + 1
					notice = glance:next()
				else break end
			end
			glance:term()
		end
	end
	nk.tree_pop()
	return gui
end
