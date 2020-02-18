return function (gui,ctx,prv)
	local font = get_font(gui)
	nk.layout_row_dynamic( ctx, pad_height(font,"Done"), 1 )
	if nk.button( ctx, nil, "Done" ) then
		gui.which = prv
	end
	return gui
end
