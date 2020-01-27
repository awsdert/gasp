nk = require("moonnuklear")
nkgl = require('backend')
op = 'large'
value = 0.6
local function pad(font,text)
	return math.ceil((font:width(font:height(),text)) + 5)
end
local para = { font_size = 24 }
function main(ctx)
	local text, atlas, font
	if nk.window_begin(ctx, "Show", {50, 50, 220, 220},
		nk.WINDOW_BORDER|nk.WINDOW_MOVABLE|nk.WINDOW_CLOSABLE,
		'/usr/share/fonts/TTF') then
		-- fixed widget pixel width
		text = "Hook PID"
		font = nkgl.p.def_font
		nk.layout_row_static(ctx, 40, (pad(font,text)), 1)
		if nk.button(ctx, nil, text) then
			 -- ... event handling ...
			 print("Hook pid")
		end
		-- fixed widget window ratio width
		text = 'small'
		nk.layout_row_dynamic(ctx, (pad(font,text)) * 2, 2)
		if nk.option(ctx, text, op == text) then
			op = text
			para.font_size = 16
		end
		text = 'large'
		if nk.option(ctx, text, op == text) then
			op = text
			para.font_size = 24
		end
			-- custom widget pixel width
			nk.layout_row_begin(ctx, 'static', 30, 2)
			nk.layout_row_push(ctx, 50)
			nk.label(ctx, "Volume:", nk.TEXT_LEFT)
			nk.layout_row_push(ctx, 110)
			value = nk.slider(ctx, 0, value, 1.0, 0.1)
			nk.layout_row_end(ctx)
	 end
	 nk.window_end(ctx)
end
print("Creating context...\n")
mainFrame = nkgl.init(640,480,"Gasp Window",true,nil)
if mainFrame then
	print("Hooking main loop...\n")
	nkgl.loop(main, {.13, .29, .53, 1}, 30)
else
	print("Failed to create context!\n")
end
