-- #!/usr/bin/env lua
-- Simple example that shows how to change font when drawing widgets.
-- Uses the GLFW/OpenGL rear shipped with MoonNuklear.

cfg = require("cfg")
gl = require("moongl")
glfw = require("moonglfw")
nk = require("moonnuklear")
rear = require("moonnuklear.glbackend")

GUI = {
	which = "main",
	draw = {},
	fonts = {}
}
local R, G, B, A

local function set_bgcolor(color)
   cfg.colors.bg = color or cfg.colors.bg
   return table.unpack(cfg.colors.bg)
end
R, G, B, A = set_bgcolor()

-- GL/GLFW inits
glfw.version_hint(3, 3, 'core')
GUI.window = glfw.create_window(
	cfg.window.width, cfg.window.height, cfg.window.title )
glfw.make_context_current(GUI.window)
gl.init()

-- Initialize the rear
GUI.ctx = rear.init(GUI.window, {
   vbo_size = cfg.sizes.gl_vbo,
   ebo_size = cfg.sizes.gl_ebo,
   anti_aliasing = cfg.anti_aliasing,
   clipboard = cfg.clipboard,
   callbacks = true
})

atlas = rear.font_stash_begin()
for name,size in pairs(cfg.font.sizes) do
	size = cfg.font.base_size * size
	GUI.fonts[name] = atlas:add( size, cfg.font.file )
end
function get_font(gui,name)
	if type(name) ~= "string" or not gui.fonts[name] then
		name = cfg.font.use
	end
	return gui.fonts[name]
end
rear.font_stash_end( GUI.ctx, (get_font(GUI)) )

glfw.set_key_callback(GUI.window,
function (window, key, scancode, action, shift, control, alt, super)
   if key == 'escape' and action == 'press' then
      glfw.set_window_should_close(window, true)
   end
   rear.key_callback(window, key, scancode, action, shift, control, alt, super)
end)

collectgarbage()
collectgarbage('stop')

function pad_width(font,text)
	return math.ceil((font:width(font:height(),text)) * 1.2)
end

function pad_height(font,text)
	return math.ceil((font:height(text)) * 1.2)
end
GUI.which = "main"
GUI.draw["main"] = function(gui,ctx)
	local font = get_font(gui)
	local text
	text = "Change Font"
	nk.layout_row_static( ctx,
		pad_height(font,text), pad_width(font,text), 1)
	if nk.button(ctx, nil, text) then
		 -- ... event handling ...
		 gui.which = "cfg-font"
	end
	-- Slider Example with Custom width
	text = "Volulme:"
	nk.layout_row_begin(ctx, 'static',
		pad_height(font,text), 2)
	nk.layout_row_push(ctx, pad_width(font,text))
	nk.label(gui.ctx,text, nk.TEXT_LEFT)
	nk.layout_row_push(ctx, pad_width(font,text))
	cfg.volume = nk.slider(ctx, 0, cfg.volume, 1.0, 0.1)
	nk.layout_row_end(ctx)
	return gui
end
GUI.draw["cfg-font"] = require("cfg.font")
local function draw_all(gui,ctx)
	local push_font = (cfg.font.use ~= "default")
	if push_font == true then
		nk.style_push_font( ctx, get_font(gui) )
	end
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	if nk.window_begin(ctx, "Show",
		{0,0,cfg.window.width,cfg.window.height},
		nk.WINDOW_BORDER | nk.WINDOW_SCALABLE
	) then
		gui = gui.draw[gui.which](gui,gui.ctx,"main")
	end
	nk.window_end(ctx)
	if push_font == true then
		nk.style_pop_font(ctx)
	end
	-- Render
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	gl.viewport(0, 0, cfg.window.width, cfg.window.height)
	gl.clear_color(R, G, B, A)
	gl.clear('color')
	rear.render()
	glfw.swap_buffers(gui.window)
	collectgarbage()
	return gui
end
while not glfw.window_should_close(GUI.window) do
	glfw.wait_events_timeout(1/cfg.fps)
	rear.new_frame()
	GUI = draw_all(GUI,GUI.ctx)
end
rear.shutdown()
