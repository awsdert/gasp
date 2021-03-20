loadlib = package.loadlib
function trace()
	return print(debug.traceback())
end
debug.sethook(trace,"*")

-- Used to recover where possible
_G.forced_reboot = false
function calc_percentage( num, dem )
	return (((0.0 + num) / (0.0 + dem)) * 100.0)
end

function Require(path)
	if not path then
		print(debug.traceback())
		return nil
	end
	local func, err = loadfile(path)
	if not func then
		print(debug.traceback())
		print(err)
		print("Tried to load '" .. path .. "'")
		return nil
	end
	return func, pcall( func )
end

function init()
	local func, ok, err, v, tmp
	
	_G.asked4_reboot = true
	while _G.asked4_reboot == true do
		_G.asked4_reboot = false
		-- For a different interface just change the file below
		tmp = scriptspath() .. "/gui_mode.lua"
		-- Prevent lua from exiting gasp without good reason
		func, ok, err, v = Require( tmp )
		if not ok then
			print( err )
			-- Avoid fully restarting gasp
			if _G.asked4_reboot == false then
				-- Avoid a never closing app
				if _G.forced_reboot == true then
					return
				end
			end
			--[[ We're forcing a reboot, this prevents consective
			forced reboots in the event the bug occurs in the main
			script file above ]]
			_G.forced_reboot = true
		end
	end
end

function tointeger(val)
	return math.floor(tonumber(val) or 0)
end
function toboolean(val)
	if val == true or val == false then return val end
	if not val then return false end
	return true
end

function app_data()
	return (os.getenv("GASP_PATH") or ".")
end

function scriptspath()
	return app_data() .. '/lua'
end

function mkdir(path)
	if not path then return end
	local ret = gasp.path_isdir( path )
	if ret == 1 then
		return path
	elseif ret == 0 then
		io.popen("mkdir '" .. path .. "'")
		if gasp.path_isdir( path ) == 1 then
			return path
		end
	end
end

function cheatspath()
	local path = app_data()
	if gasp.path_isdir( path .. '/cheats' ) then
		return path .. '/cheats'
	end
	mkdir(path)
	path = path .. '/cheats'
	mkdir(path)
	return path
end

function has_ext( path, ext )
	local i, j, p, e
	if not path or not ext or #path <= #ext then
		return false
	end
	j = #path - (#ext - 1)
	for i = 1,#ext,1 do
		p = path:sub(j,j)
		e = ext:sub(i,i)
		if p ~= e then
			return false
		end
		j = j + 1
	end
	return true
end

init()
