#!/usr/bin/env lua
-- Simple example that shows how to change font when drawing widgets.
-- Uses the GLFW/OpenGL backend shipped with MoonNuklear.

local glfw = require("moonglfw")
local gl = require("moongl")
local backend = require("moonnuklear.glbackend")
local nk = require("moonnuklear")

local TITLE = "Changing font" -- GLFW window title
local FPS = 30 -- desired frames per seconds
local W, H = 800, 640 -- window width and height
local BGCOLOR = {0.10, 0.18, 0.24, 1.0} -- background color
local VBO_SIZE, EBO_SIZE = 512*1024, 128*1024 -- size of GL buffers
local ANTI_ALIASING = true -- use anti-aliasing
local CLIPBOARD = false -- use clipboard
local DEFAULT_FONT_PATH = '/usr/share/fonts/TTF' --  full path filename of a ttf file (optional)

local R, G, B, A
local window, ctx, atlas

local function set_bgcolor(color)
   BGCOLOR = color or BGCOLOR
   R, G, B, A = table.unpack(BGCOLOR)
end
set_bgcolor()

-- GL/GLFW inits
glfw.version_hint(3, 3, 'core')
window = glfw.create_window(W, H, TITLE)
glfw.make_context_current(window)
gl.init()

-- Initialize the backend
ctx = backend.init(window, {
   vbo_size = VBO_SIZE,
   ebo_size = EBO_SIZE,
   anti_aliasing = ANTI_ALIASING,
   clipboard = CLIPBOARD,
   callbacks = true
})

-- Load fonts: if none of these are loaded a default font will be used
atlas = backend.font_stash_begin()
local default_font = atlas:add(13, DEFAULT_FONT_PATH)
local droid = atlas:add(14, "fonts/DroidSans.ttf")
local droid_big = atlas:add(28, "fonts/DroidSans.ttf")
local roboto = atlas:add(14, "fonts/Roboto-Regular.ttf")
local future = atlas:add(13, "fonts/kenvector_future_thin.ttf")
local clean = atlas:add(12, "fonts/ProggyClean.ttf")
local tiny = atlas:add(10, "fonts/ProggyTiny.ttf")
local cousine = atlas:add(13, "fonts/Cousine-Regular.ttf")
backend.font_stash_end(ctx, default_font)

-- Set the default font to 'cousine':
nk.style_set_font(ctx, cousine)

glfw.set_key_callback(window, function (window, key, scancode, action, shift, control, alt, super)
   if key == 'escape' and action == 'press' then
      glfw.set_window_should_close(window, true)
   end
   backend.key_callback(window, key, scancode, action, shift, control, alt, super)
end)

collectgarbage()
collectgarbage('stop')

local window_flags = nk.WINDOW_BORDER |
	nk.WINDOW_MOVABLE | nk.WINDOW_CLOSABLE | nk.WINDOW_SCALABLE

while not glfw.window_should_close(window) do
   glfw.wait_events_timeout(1/FPS) --glfw.poll_events()
   backend.new_frame()

   -- Draw the GUI --------------------------------------------
   if nk.window_begin(ctx, "GUI", {50, 50, 320, 220}, window_flags) then
      nk.layout_row_dynamic(ctx, 20, 1)
      nk.label(ctx, "Using default font 'cousine'", nk.TEXT_CENTERED)
      nk.style_push_font(ctx, droid)
      nk.label(ctx, "Using font 'droid'", nk.TEXT_CENTERED)
      nk.style_push_font(ctx, future)
      nk.label(ctx, "Using font 'future'", nk.TEXT_CENTERED)
      nk.style_push_font(ctx, droid_big)
      nk.label(ctx, "Using font 'droid_big'", nk.TEXT_CENTERED)
      nk.style_pop_font(ctx)
      nk.label(ctx, "Back to using font 'future'", nk.TEXT_CENTERED)
      nk.style_pop_font(ctx)
      nk.label(ctx, "Back to using font 'droid'", nk.TEXT_CENTERED)
      nk.style_pop_font(ctx)
      nk.label(ctx, "Back to using default font 'cousine'", nk.TEXT_CENTERED)
   end
   nk.window_end(ctx)
   ------------------------------------------------------------

   -- Render
   W, H = glfw.get_window_size(window)
   gl.viewport(0, 0, W, H)
   gl.clear_color(R, G, B, A)
   gl.clear('color')
   backend.render()
   glfw.swap_buffers(window)
   collectgarbage()
end

backend.shutdown()
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
