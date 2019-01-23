#!/usr/bin/env lua

local bench = require"bench"
local aux = require"bench-aux"

local lib = ... or aux.optenv("BENCH_L", "bench-wheel.so")
local limit = tonumber(aux.optenv("BENCH_N", 1000000))
local step  = tonumber(aux.optenv("BENCH_S", limit / 100))
local verbose = aux.toboolean(os.getenv("BENCH_V", false))

local B = bench.new(lib, count)

for i=0,limit,step do
	-- add i timeouts
	local fill_elapsed, fill_count = aux.time(B.fill, B, i, 60 * 1000000)
	assert(i == fill_count)

	--- delete i timeouts
	local del_elapsed = aux.time(B.del, B, 0, fill_count)
	assert(B:empty())
	local del_rate = i > 0 and i / del_elapsed or 0

	local fmt = verbose and "%d\t%f\t(%d/s)\t(fill:%f)" or "%d\t%f"
	aux.say(fmt, i, del_elapsed, del_rate, fill_elapsed)
end
