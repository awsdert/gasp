-- #!/usr/bin/env lua
-- Simple example that shows how to change font when drawing widgets.
-- Uses the GLFW/OpenGL rear shipped with MoonNuklear.

function trace()
	return print(debug.traceback())
end
debug.sethook(trace,"*")

function tointeger(val)
	return math.floor(tonumber(val) or 0)
end

--[[ For other modes such as audio only just change the below to the
script you are using ]]
require("gui_mode")
