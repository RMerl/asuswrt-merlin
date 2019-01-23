#!/usr/bin/env lua

local bench = require"bench"
local aux = require"bench-aux"

local lib = ... or aux.optenv("BENCH_L", "bench-wheel.so")
local limit = tonumber(aux.optenv("BENCH_N", 1000000))
local step  = tonumber(aux.optenv("BENCH_S", limit / 100))
local exp_step = tonumber(aux.optenv("BENCH_E", 1.0))
local verbose = aux.toboolean(os.getenv("BENCH_V", false))

local B = bench.new(lib, count, nil, verbose)
local fill_count, fill_last = B:fill(limit)

for i=0,limit,step do
	local exp_elapsed, fill_elapsed, fill_rate

	-- expire all timeouts
	--exp_elapsed = aux.time(B.expire, B, fill_count, fill_last * exp_step)
	exp_elapsed = aux.time(B.del, B, 0, fill_count)
	assert(B:empty())

	-- add i timeouts
	fill_elapsed, fill_count, fill_last = aux.time(B.fill, B, i)
	assert(fill_count == i)
	fill_rate = fill_elapsed > 0 and (fill_count / fill_elapsed) or 0

	local fmt = verbose and "%d\t%f\t(%d/s)\t(exp:%f)" or "%d\t%f"
	aux.say(fmt, i, fill_elapsed, fill_rate, exp_elapsed)
end
