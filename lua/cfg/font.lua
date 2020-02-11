local gui = {}

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

local function draw_main(ctx)
	local normal_font = (cfg.font.use == 'medium')
	local bounds = {0,0,cfg.window.width,cfg.window.height}
	
	local add_label = function(name,text)
		local used = (cfg.font.use == name)
		local font = gui.fonts[name]
		if used == false then
			nk.style_push_font(ctx, font)
		end
		nk.layout_row_static(ctx,
			pad_height(font,text), pad_width(font,text), #text )
		nk.label(ctx,text,nk.WINDOW_BORDER)
		if used == false then
			nk.style_pop_font(ctx)
		end
	end
	
	local add_button = function(name,text)
		local used = (cfg.font.use == name)
		local font = gui.fonts[name]
		if used == false then
			nk.style_push_font(ctx, font)
		end
		nk.layout_row_static(ctx,
			pad_height(font,text), pad_width(font,text), #text )
		if nk.button(ctx, nil, text) then
			 cfg.font.use = name
		end
		if used == false then
			nk.style_pop_font(ctx)
		end
	end
	
	if normal_font == false then
		nk.style_push_font(ctx, gui.fonts[cfg.font.use])
	end
	
	if nk.window_begin(ctx, "Font Size",
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
		nk.style_pop_font(ctx)
	end
	nk.window_end(ctx)
end
local function draw_all(ctx)
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	draw_font_sizes_window()
	-- Render
	cfg.window.width, cfg.window.height =
		glfw.get_window_size(gui.window)
	gl.viewport(0, 0, cfg.window.width, cfg.window.height)
	gl.clear_color(table.unpack(cfg.colors.bg))
	gl.clear('color')
	rear.render()
	glfw.swap_buffers(gui.window)
	collectgarbage()
end
return function(ctx,default)
	-- GL/GLFW inits
	gui.window = glfw.create_window(
		cfg.window.width, cfg.window.height, cfg.window.title )
	glfw.make_context_current(gui.window)
	-- Initialize the rear
	if not gui.fonts["medium"] then
		local atlas = rear.font_stash_begin()
		for name,size in pairs(gui.font_size) do
			size = cfg.font.base_size * size
			if name == "medium" then
				gui.fonts[name] = default_font
			else
				gui.fonts[name] = atlas:add( size, cfg.font.file )
			end
		end
		rear.font_stash_end( ctx, gui.fonts.medium )
	end
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
		draw_all(ctx)
	end
	return gui.fonts[cfg.font.use]
end
