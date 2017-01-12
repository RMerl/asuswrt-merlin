local bench = require"bench"
local clock = bench.clock

local aux = {}

local function time_return(begun, ...)
	local duration = clock() - begun
	return duration, ...
end

function aux.time(f, ...)
	local begun = clock()
	return time_return(begun, f(...))
end

function aux.say(...)
	print(string.format(...))
end

function aux.toboolean(s)
	return tostring(s):match("^[1TtYy]") and true or false
end

function aux.optenv(k, def)
	local s = os.getenv(k)

	return (s and #s > 0 and s) or def
end

return aux
