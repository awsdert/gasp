gl = require("moongl")
glfw = require("moonglfw")
nk = require("moonnuklear")
rear = require("moonnuklear.glbackend")

gui = {}
cfg = {
	fps = 30,
	clipboard = false,
	anti_aliasing = false,
	window = {
		width = 640,
		height = 480,
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

-- Load fonts: if none of these are loaded a default font will be used
gui.font_size = {}
gui.font_size["xx-large"] = 1.6
gui.font_size["x-large"] = 1.4
gui.font_size["large"] = 1.2
gui.font_size["medium"] = 1
gui.font_size["small"] = 0.8
gui.font_size["x-small"] = 0.6
gui.font_size["xx-small"] = 0.4
gui.fonts = {}
gui.window_flags = nk.WINDOW_BORDER | nk.WINDOW_SCALABLE

local function pad_width(font,text)
	return math.ceil((font:width(font:height(),text)) * 1.2)
end

local function pad_height(font,text)
	return math.ceil((font:height(text)) * 1.2)
end

function draw_font_sizes_window()
	local normal_font = (cfg.font.use == 'medium')
	local bounds = {0,0,cfg.window.width,cfg.window.height}
	
	local add_label = function(name,text)
		local used = (cfg.font.use == name)
		local font = gui.fonts[name]
		if used == false then
			nk.style_push_font(gui.ctx, font)
		end
		nk.layout_row_static(gui.ctx,
			pad_height(font,text), pad_width(font,text), #text )
		nk.label(gui.ctx,text,nk.WINDOW_BORDER)
		if used == false then
			nk.style_pop_font(gui.ctx)
		end
	end
	
	local add_button = function(name,text)
		local used = (cfg.font.use == name)
		local font = gui.fonts[name]
		if used == false then
			nk.style_push_font(gui.ctx, font)
		end
		nk.layout_row_static(gui.ctx,
			pad_height(font,text), pad_width(font,text), #text )
		if nk.button(gui.ctx, nil, text) then
			 cfg.font.use = name
		end
		if used == false then
			nk.style_pop_font(gui.ctx)
		end
	end
	
	if normal_font == false then
		nk.style_push_font(gui.ctx, gui.fonts[cfg.font.use])
	end
	
	if nk.window_begin(gui.ctx, "Font Size",
		bounds, gui.window_flags) then
		
		add_label(cfg.font.use,"Change font size to:")
		
		add_button( 'xx-large', 'XX-Large')
		add_button( 'x-large', 'X-Large')
		add_button( 'large', 'Large')
		add_button( 'medium', 'Medium')
		add_button( 'small', 'Small')
		add_button( 'x-small', 'X-Small')
		add_button( 'xx-small', 'XX-Small')
	end
	
	if normal_font == false then
		nk.style_pop_font(gui.ctx)
	end
	nk.window_end(gui.ctx)
end
function draw_all()
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	draw_font_sizes_window()
	-- Render
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	gl.viewport(0, 0, cfg.window.width, cfg.window.height)
	gl.clear_color(R, G, B, A)
	gl.clear('color')
	rear.render()
	glfw.swap_buffers(gui.window)
	collectgarbage()
end
return function()
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
	atlas = rear.font_stash_begin()
	for name,size in pairs(gui.font_size) do
		size = cfg.font.base_size * size
		gui.fonts[name] = atlas:add( size, cfg.font.file )
	end
	rear.font_stash_end( gui.ctx, gui.fonts.medium )
	glfw.set_key_callback(gui.window,
	function (window, key, scancode, action, shift, control, alt, super)
	   if key == 'escape' and action == 'press' then
		  glfw.set_window_should_close(window, true)
	   end
	   rear.key_callback(window, key, scancode, action, shift, control, alt, super)
	end)

	collectgarbage()
	collectgarbage('stop')

	while not glfw.window_should_close(gui.window) do
		glfw.wait_events_timeout(1/cfg.fps)
		rear.new_frame()
		draw_all()
	end
	rear.shutdown()
	return gui.fonts[cfg.font.use]
end
