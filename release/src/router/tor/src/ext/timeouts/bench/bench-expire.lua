#!/usr/bin/env lua

local bench = require"bench"
local aux = require"bench-aux"

local lib = ... or aux.optenv("BENCH_L", "bench-wheel.so")
local limit = tonumber(aux.optenv("BENCH_N", 1000000))
local step  = tonumber(aux.optenv("BENCH_S", limit / 100))
-- expire 1/1000 * #timeouts per clock update
local exp_step = tonumber(aux.optenv("BENCH_E", 0.0001)) 
local verbose = aux.toboolean(os.getenv("BENCH_V", false))

local B = require"bench".new(lib, count)

for i=0,limit,step do
	-- add i timeouts
	local fill_elapsed, fill_count, fill_last  = aux.time(B.fill, B, i)

	-- expire timeouts by iteratively updating clock. exp_step is the
	-- approximate number of timeouts (as a fraction of the total number
	-- of timeouts) that will expire per update.
	local exp_elapsed, exp_count = aux.time(B.expire, B, fill_count, math.floor(fill_last * exp_step))
	assert(exp_count == i)
	assert(B:empty())
	local exp_rate = i > 0 and i / exp_elapsed or 0

	local fmt = verbose and "%d\t%f\t(%d/s)\t(fill:%f)" or "%d\t%f"
	aux.say(fmt, i, exp_elapsed, exp_rate, fill_elapsed)
end
