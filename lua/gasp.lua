-- #!/usr/bin/env lua
-- Simple example that shows how to change font when drawing widgets.
-- Uses the GLFW/OpenGL rear shipped with MoonNuklear.

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
function gasp_add_font(size)
	if type(size) == "number" then
		size = cfg.font.base_size * size
	else
		size = cfg.font.base_size
	end
	return atlas:add( size, cfg.font.file )
end
gui.font_size_mul = {}
gui.font_size_mul["xx-large"] = 1.6
gui.font_size_mul["x-large"] = 1.4
gui.font_size_mul["large"] = 1.2
gui.font_size_mul["medium"] = 1.0
gui.font_size_mul["small"] = 0.8
gui.font_size_mul["x-small"] = 0.6
gui.font_size_mul["xx-small"] = 0.4
gui.fonts = {}
gui.fonts["xx-large"] = gasp_add_font(gui.font_size_mul["xx-large"])
gui.fonts["x-large"] = gasp_add_font(gui.font_size_mul["x-large"])
gui.fonts["large"] = gasp_add_font(gui.font_size_mul["xx-large"])
gui.fonts["medium"] = gasp_add_font(gui.font_size_mul["medium"])
gui.fonts["small"] = gasp_add_font(gui.font_size_mul["small"])
gui.fonts["x-small"] = gasp_add_font(gui.font_size_mul["x-small"])
gui.fonts["xx-small"] = gasp_add_font(gui.font_size_mul["xx-small"])
gasp_add_font = nil
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
		add_button( 'x-small', 'Small')
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
while not glfw.window_should_close(gui.window) do
	glfw.wait_events_timeout(1/cfg.fps)
	rear.new_frame()
	draw_all()
end
rear.shutdown()
--[[
nk = require("moonnuklear")
nkgl = require('backend')
op = 'large'
value = 0.6
gasp = gasp or {}
local function pad(font,text)
	return math.ceil((font:width(font:height(),text)) + 5)
end
local noto = { font_name = "Noto Sans CJK JP Regular", font_size = 24 }
function main(ctx)
	local text, atlas, font
	local flags = nk.WINDOW_BORDER|nk.WINDOW_MOVABLE|nk.WINDOW_CLOSABLE
	local proc_locate_name = gasp.proc_locate_name or function() return "Fake Find DQB" end
	if nk.window_begin(ctx, "Show", {50, 50, 220, 220}, flags ) then
		-- fixed widget pixel width
		text = "Find DQB"
		font = nkgl.p.def_font
		nk.layout_row_static(ctx, 40, (pad(font,text)), 1)
		if nk.button(ctx, nil, text) then
			 -- ... event handling ...
			 print(proc_locate_name())
		end
		-- fixed widget window ratio width
		text = 'small'
		nk.layout_row_dynamic(ctx, (pad(font,text)) * 2, 2)
		if nk.option(ctx, text, op == text) then
			op = text
			noto.font_size = 20
		end
		text = 'large'
		if nk.option(ctx, text, op == text) then
			op = text
			noto.font_size = 24
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
mainFrame = nkgl.init(
	640,480,"Gasp Window",true,'/usr/share/fonts/TTF',noto)
if mainFrame then
	nkgl.loop(main, {.13, .29, .53, 1}, 30)
end
]]
