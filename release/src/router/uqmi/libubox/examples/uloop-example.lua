#!/usr/bin/env lua

local uloop = require("uloop")
uloop.init()

-- timer example 1
local timer
function t()
	print("1000 ms timer run");
	timer:set(1000)
end
timer = uloop.timer(t)
timer:set(1000)

-- timer example 2
uloop.timer(function() print("2000 ms timer run"); end, 2000)

-- timer example 3
uloop.timer(function() print("3000 ms timer run"); end, 3000):cancel()

-- process
function p1(r)
	print("Process 1 completed")
	print(r)
end

function p2(r)
	print("Process 2 completed")
	print(r)
end

uloop.timer(
	function()
		uloop.process("uloop_pid_test.sh", {"foo", "bar"}, {"PROCESS=1"}, p1)
	end, 1000
)
uloop.timer(
	function()
		uloop.process("uloop_pid_test.sh", {"foo", "bar"}, {"PROCESS=2"}, p2)
	end, 2000
)

uloop.run()

