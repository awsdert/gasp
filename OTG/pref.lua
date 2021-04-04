local pref = {
	fps = 60,
	volume = 0.5,
	clipboard = false,
	anti_aliasing = false,
	cheatdir = '/mnt/HOME/cheats/gasp',
	window = {
		width = 1024,
		height = 720,
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
		otg = "./OTG"
	}
}
pref.font = {
	-- *.ttf fonts only should be used here unless nuklear gets an upgrade
	dir = pref.dir.sys .. '/fonts',
	base_size = 40,
	use = "default",
	sizes = {}
}
pref.font.file = pref.font.dir .. "/noto/NotoSansDisplay-Regular.ttf"
pref.font.mono = pref.font.dir .. "/noto/NotoSansMono-Regular.ttf"
-- These serve as multipliers against base_size
pref.font.sizes["default"] = 1
pref.font.sizes["xx-large"] = 1.6
pref.font.sizes["x-large"] = 1.4
pref.font.sizes["large"] = 1.2
pref.font.sizes["medium"] = 1
pref.font.sizes["small"] = 0.8
pref.font.sizes["x-small"] = 0.6
pref.font.sizes["xx-small"] = 0.4
return pref
