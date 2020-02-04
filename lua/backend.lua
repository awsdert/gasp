#!/usr/bin/env lua
-- Backend for the hello.lua example.
-- This is actually a wrapper to the GLFW/OpenGL painter shipped with MoonNuklear,
-- that can be found in moonnuklear/glpainter.lua.
-- The main purpose of this wrapper is to remove 'painter noise' from the examples.

local p = {
	gl = require("moongl"),
	glfw = require("moonglfw"),
	nk = require("moonnuklear"),
	painter = require("glbackend"),
	window, ctx, atlas, font_dir, def_font
}
local TITLE = "MoonNuklear Window"
local W, H = 1200, 800 -- window width and height
local BGCOLOR = {0.10, 0.18, 0.24, 1.0}
local R, G, B, A = table.unpack(BGCOLOR)

-------------------------------------------------------------------------------
local function init(width, height, title, anti_aliasing, font_path, parameters)
-------------------------------------------------------------------------------
-- Initializes the painter.
-- width, height: initial dimensions of the window
-- title: window title
-- anti_aliasing: if =true then AA is turned on
-- font_path: optional full path filename of a ttf file
-- parameters: optional parameters for painter.init
	font_dir = font_path
	local para = parameters or {}
	para.vbo_size =  para.vbo_size or 512*1024
	para.ebo_size =  para.ebo_size or 128*1024
	para.anti_aliasing = para.anti_aliasing or anti_aliasing
	para.clipboard = para.clipboard or true
	para.callbacks = para.callbacks or true
	para.font_size = para.font_size or 24
	-- GL/GLFW inits
	p.glfw.version_hint(3, 3, 'core')
	W, H = width, height
	p.window = p.glfw.create_window(W, H, title or TITLE)
	p.glfw.make_context_current(p.window)
	p.gl.init()

	p.ctx = p.painter.init(p.window, para)

	p.atlas = p.painter.font_stash_begin()
	p.def_font = p.atlas:add(para.font_size, p.font_dir)
	p.painter.font_stash_end(p.ctx, p.def_font)

	p.glfw.set_key_callback(p.window,
	function (window, key, scancode, action, shift, control, alt, super)
		if key == 'escape' and action == 'press' then
			p.glfw.set_window_should_close(window, true)
		end
		painter.key_callback(window, key, scancode, action, shift, control, alt, super)
	end)

	return p.ctx
end

function set_bgcolor(color)
	BGCOLOR = color or BGCOLOR
	R, G, B, A = table.unpack(BGCOLOR)
end

-------------------------------------------------------------------------------
local function loop(guifunc, bgcolor, fps)
-------------------------------------------------------------------------------
-- Enters the main event loop.
-- guifunc: a function implementing the front-end of the application.
-- bgcolor: background color (optional, defaults to BGCOLOR)
-- fps: desired frames per seconds (optional, defaults to 30)
--
-- The guifunc is executed at each frame as guifunc(ctx), where ctx is
-- the Nuklear context.
--
	local fps = fps or 30 -- frames per second
	if bgcolor then set_bgcolor(bgcolor) end

	collectgarbage()
	collectgarbage('stop')

	while not p.glfw.window_should_close(p.window) do
		p.glfw.wait_events_timeout(1/fps) --glfw.poll_events()
		p.painter.new_frame()

		-- Draw the GUI ----------------
		guifunc(p.ctx)
		--------------------------------

		-- Render
		W, H = p.glfw.get_window_size(p.window)
		p.gl.viewport(0, 0, W, H)
		p.gl.clear_color(R, G, B, A)
		p.gl.clear('color')
		p.painter.render()
		p.glfw.swap_buffers(p.window)
		collectgarbage()
	end
	p.painter.shutdown()
end

return {
	init = init,
	loop = loop,
	p = p,
	get_window_size = function()
		return p.glfw.get_window_size(window)
	end,
	get_window_pos = function()
		return p.glfw.get_window_pos(window)
	end,
	set_bgcolor = set_bgcolor
}

