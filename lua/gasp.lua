-- #!/usr/bin/env lua
-- Simple example that shows how to change font when drawing widgets.
-- Uses the GLFW/OpenGL rear shipped with MoonNuklear.

gl = require("moongl")
glfw = require("moonglfw")
nk = require("moonnuklear")
rear = require("moonnuklear.glbackend")
cfg_font = require("cfg.font")

gui = {}
cfg = {
	fps = 30,
	clipboard = false,
	anti_aliasing = false,
	window = {
		width = 1600,
		height = 900,
		title = "GASP - Gaming Assistive tech for Solo Play"
	},
	colors = {
		-- RGBA from 0.0 to 1.0
		bg = { 0.1, 0.1, 0.1, 1.0 }
	},
	sizes = {
		gl_vbo = 1024 * 512,
		gl_ebo = 1024 * 128
	},
	dir = {
		sys = "/usr/share",
		usr = '~/.config/.gasp',
		otg = "./otg"
	}
}
cfg.font = {
	-- *.ttf fonts only should be used here unless nuklear gets an upgrade
	file = cfg.dir.sys .. "/fonts/noto/NotoSansDisplay-Regular.ttf",
	base_size = 40,
	use = "medium"
}

local R, G, B, A
local atlas

local function set_bgcolor(color)
   cfg.colors.bg = color or cfg.colors.bg
   return table.unpack(cfg.colors.bg)
end
R, G, B, A = set_bgcolor()

-- GL/GLFW inits
glfw.version_hint(3, 3, 'core')
gui.window = glfw.create_window(
	cfg.window.width, cfg.window.height, cfg.window.title )
glfw.make_context_current(gui.window)
gl.init()

-- Initialize the rear
gui.ctx = rear.init(gui.window, {
   vbo_size = cfg.sizes.gl_vbo,
   ebo_size = cfg.sizes.gl_ebo,
   anti_aliasing = cfg.anti_aliasing,
   clipboard = cfg.clipboard,
   callbacks = true
})

-- Load fonts: if none of these are loaded a default font will be used
atlas = rear.font_stash_begin()
gui.font = atlas:add( cfg.font.base_size, cfg.font.file )
rear.font_stash_end( gui.ctx, gui.font )

glfw.set_key_callback(gui.window,
function (window, key, scancode, action, shift, control, alt, super)
   if key == 'escape' and action == 'press' then
      glfw.set_window_should_close(window, true)
   end
   rear.key_callback(window, key, scancode, action, shift, control, alt, super)
end)

collectgarbage()
collectgarbage('stop')

gui.window_flags = nk.WINDOW_BORDER | nk.WINDOW_SCALABLE

local function pad_width(text)
	local font = gui.font
	return math.ceil((font:width(font:height(),text)) * 1.2)
end

local function pad_height(text)
	local font = gui.font
	return math.ceil((font:height(text)) * 1.2)
end
function draw_all(ctx)
	local text
	local bounds = {0,0,cfg.window.width,cfg.window.height}
	if nk.window_begin(ctx, "Show", bounds, gui.window_flags ) then
		-- fixed widget pixel width
		text = "Change Font"
		nk.layout_row_static(ctx, 40, (pad_width(text)), 1)
		if nk.button(ctx, nil, text) then
			 -- ... event handling ...
			 gui.font = cfg_font()
		end
			-- custom widget pixel width
			nk.layout_row_begin(ctx, 'static', 30, 2)
			nk.layout_row_push(ctx, 50)
			nk.label(ctx, "Volume:", nk.TEXT_LEFT)
			nk.layout_row_push(ctx, 110)
			value = nk.slider(ctx, 0, 0.5, 1.0, 0.1)
			nk.layout_row_end(ctx)
	 end
	 nk.window_end(ctx)
end
while not glfw.window_should_close(gui.window) do
	glfw.wait_events_timeout(1/cfg.fps)
	rear.new_frame()
	draw_all(gui.ctx)
end
rear.shutdown()
