-- Function definitions
if not game then
	print("Hello! Checking for bugs...")
	function locate( name ) return nil end
	function aobscan( scan, find, size ) return nil end
	glut = {
		api = "fakeglut",
		RGB = 0x0,
		RGBA = 0x0,
		DEPTH = 0x0,
		SINGLE = 0x0,
		DOUBLE = 0x0,
		Init = function () return 0 end,
		InitWindowSize = function (w,h) return 0 end,
		InitWindowPosition = function (x,y) return 0 end,
		InitDisplayMode = function(mode) return 0 end,
		CloseFunc = function() return 0 end,
		DisplayFunc = function() return 0 end,
		IdleFunc = function() return 0 end,
		ReshapeFunc = function() return 0 end,
		KeyboardFunc = function() return 0 end,
		SpecialFunc = function() return 0 end,
		CreateWindow = function(title) return -1 end,
		MainLoop = function() end
	}
end
function tonum(val,base)
	local function _tonum(num,b)
		if num == nil then return 0x0
		elseif type(num) == "integer" then return num
		elseif type(num) == "number" then return num
		else return tonumber(num,b) end
	end
	base = _tonum(base,10)
	if base == 0 then base = 10 end
	return (_tonum(val,base))
end
function tostr(val)
	local txt = "\0"
	if type(val) == "table" then
		txt = txt .. "{\n"
		for i,v in next,val do
			txt = txt .. "  member '" .. i
				.. "' = " .. (tostr(v)) .. "\n"
		end
		return txt .. "}"
	elseif val then
		return "" .. val
	else return "nil" end
end
function nextchar( txt )
	local i = 0
	return function()
		i = i + 1
		if i > #txt then return nil end
		return txt:sub(i,i)
	end
end
function Bytes( txt )
	local bytes = ""
	local v = 0x0
	for c in nextchar(txt) do
		if not c then break
		elseif c == ' ' then
			bytes = bytes .. v
			v = 0x0
		else
			v = v * 0x10
			v = v + tonum(c,16)
		end
	end
	if v ~= 0 then bytes = bytes .. v end
	return { text = txt, list = bytes }
end
function names2index(list)
	local l = {}
	if type(list) == "list" then
		for i,v in next,t do
			l[v] = i
		end
	end
	return l
end
-- Base data to search for
game_exe_names = {"ffxv_sv.exe","ffxv_s.exe","ffxv.exe"}
game_bytes = {
	party_general = {
		bytes = Bytes("ED 77 B2 40 00 00 70 42 64"),
		offsets = {
			gil = -0x170,
			ap = -0x164
		}
	},
	field_members = {
		bytes = Bytes("30 40 65 0E 00 00 00 00 40 40 65 0E"),
		offsets = {
			noctis_xp = 0x28E10
		}
	}
}
-- Do NOT add to this outside of scripts
game_areas = {}
--[[
#main_window must not be 0 when this is used,
wait for below calls to be made
]]
main_window = 0
-- Create graphical options that need special handling here
function misc_oninit(frame)
end
function onidle()
end
-- Underlying types
game_types = names2index({
	bytes, ascii, utf8, utf16, utf32, wchar_t,
	char, short, int, long, llong,
	int8_t, int16_t, int32_t, int64_t, intptr_t,
	size_t, time_t
})
ctrl_types = names2index({
	frame, bar,
	menu, option,
	tree, branch, leaf,
	list, item, record,
	label, button, text
})
char_width = 10
char_height = 10
line_spacing = 10
function drawText( font, text, x, y )
	local tx = x
	for c in nextchar(text) do
		if c == 13 then
			x = tx
			y = y + char_height + line_spacing
		else
			gl.RasterPos2i(x,y)
			glut.BitmapCharacter(font,c)
			x = x + char_width
		end
	end
end
--[[
print("Detecting " .. glut.api .. " abilities...");
for i in next,glut do
	print("Seen " .. i)
end
]]
glut.Init()
glut.InitDisplayMode(glut.DEPTH|glut.RGBA|glut.DOUBLE)
glut.InitWindowPosition( 150, 150 )
glut.InitWindowSize( 480, 640 )
main_window = glut.CreateWindow("Final Fantasy XV Cheat App Window")
glut.DisplayFunc(
	function()
		gl.Clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT)
		--glut.PostRedisplay()
		--gl.Flush()
		glut.SwapBuffers()
	end
)
--[[glut.ReshapeFunc( function () end )
glut.KeyboardFunc( function (key,x,y) end )
glut.SpecialFunc(
	function(key,x,y)
		print( "key = ".. key .. ", x = " .. x .. "y == " .. y )
		local wx = glut.get(glut.WINDOW_X)
		local wy = glut.get(glut.WINDOW_Y)
		if key == 27 then
			glut.destroyWindow(main_window)
		elseif key == glut.KEY_UP then
			glut.positionWindow(wx,wy-10)
		elseif key == glut.KEY_DOWN then
			glut.positionWindow(wx,wy-10)
		elseif key == glut.KEY_LEFT then
			glut.positionWindow(wx-10,wy)
		elseif key == glut.KEY_RIGHT then
			glut.positionWindow(wx+10,wy)
		end
		glut.postRedisplay()
	end
)
glut.CloseFunc(
	function ()
		--glut.DisplayFunc(nil)
		--glut.IdleFunc(nil)
		--glut.ReshapeFunc(nil)
		--glut.KeyboardFunc(nil)
		--glut.SpecialFunc(nil)
		main_window = -1
		party_general_window = -1
	end
)
--]]
--part_general_window = glut.CreateSubWindow( main_window, 10, 10, 50, 50 )
glut.MainLoop()
-- Find & hook the game
print("Looking for game...")
game_pid = nil
for i,v in next,game_exe_names do
	if type(i) == "number" then
		print("Attempt " .. i .. " using '" .. v .. "'")
		game_pid = locate(v)
		if game_pid then break end
	end
end
print( "game_pid = " .. tostr(game_pid))
if not game_pid then
	print("Failed to get game pid, aborting...")
	if not game then
		print("Abort skipped to check for bugs")
	else
		exit(0)
	end
end
-- Use previously filled info to fill game areas
print("Looking for game areas...")
for i,v in next,game_bytes do
	local bytes = v.bytes.list
	print("Attempt to find " .. i .. " using '" .. bytes .. "'")
	game_areas[i] = {}
	game_areas[i].from = nil
	game_areas[i].offset = (aobscan({
		base=0,
		stop=0x7FFFFFF,
		hook=game_hook
	},bytes,#bytes))
	game_areas[i].size = 9
	game_areas[i].readas = game_types.bytes
	game_areas[i].ctrlas = ctrl_types.label
end
if not game then
	print("No bugs found, goodbye")
end
