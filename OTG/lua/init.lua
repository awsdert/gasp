loadlib = package.loadlib
function _trace()
	dump_lua_stack()
	return debug.traceback()
end
function trace()
	return print(_trace())
end
debug.sethook(trace,"*")

-- Used to recover where possible
_G.forced_reboot = false
function calc_percentage( num, dem )
	return (((0.0 + num) / (0.0 + dem)) * 100.0)
end

function _RequireLib(dir,name,report_attempts,error_on_failure)
	local paths, p, v, ret, err = { name, name .. ".so" }
	for p, v in pairs(paths) do
		if not ret then
			local path = dir .. "/" .. v
			ret, err = loadlib(path,'luaopen_' .. name)
			if report_attempts then
				print("Trying " .. path)
			end
		end
	end
	if error_on_failure and not ret then
		dump_lua_stack()
		error( err .. _trace() )
	end
	return ret, err
end

function _RequireLua( name, report_attempts )
	local dir = os.getenv("GASP_PATH")
	local paths, p, v, ret, err =
	{
		name
		, name .. ".lua"
		, "lua/" .. name
		, "lua/" .. name .. ".lua"
	}
	for p, v in pairs(paths) do
		if not ret then
			local path = dir .. "/" .. v
			ret, err = loadfile(path)
			if report_attempts then
				print("Trying " .. path)
			end
		end
	end
	if not ret then
		dump_lua_stack()
		error( (err or "nil\n") .. _trace() )
	end
	return pcall( ret ), err, ret
end

function Require(name,report_attempts,onlylibs)
	if not name then
		trace()
		return nil
	end
	local ret, err = _RequireLib
	(
		os.getenv("GASP_PATH") .. "/lib", name, report_attempts, onlylibs
	)
	if not ret then 
		return _RequireLua( name, report_attempts )
	end
	return ret, err
end

function init()
	local ret, err, func
	
	_G.asked4_reboot = true
	while _G.asked4_reboot == true do
		_G.asked4_reboot = false
		-- Prevent lua from exiting gasp without good reason
		ret, err, func = Require( "gui_mode", false )
		if not func then
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
	local path = os.getenv("GASP_PATH") .. "/cheats"
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
