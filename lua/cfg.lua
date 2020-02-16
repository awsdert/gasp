cfg = {
	fps = 30,
	volume = 0.5,
	clipboard = false,
	anti_aliasing = false,
	cheatdir = '/mnt/HOME/cheats/gasp',
	window = {
		width = 480,
		height = 640,
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
	use = "default",
	sizes = {}
}
-- These serve as multipliers against base_size
cfg.font.sizes["default"] = 1
cfg.font.sizes["xx-large"] = 1.6
cfg.font.sizes["x-large"] = 1.4
cfg.font.sizes["large"] = 1.2
cfg.font.sizes["medium"] = 1
cfg.font.sizes["small"] = 0.8
cfg.font.sizes["x-small"] = 0.6
cfg.font.sizes["xx-small"] = 0.4
return cfg
