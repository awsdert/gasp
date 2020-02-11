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
return cfg
