/* vi: set sw=4 ts=4: */
/*
 * $RANDOM support.
 *
 * Copyright (C) 2009 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "random.h"

uint32_t FAST_FUNC
next_random(random_t *rnd)
{
	/* Galois LFSR parameter */
	/* Taps at 32 31 29 1: */
	enum { MASK = 0x8000000b };
	/* Another example - taps at 32 31 30 10: */
	/* MASK = 0x00400007 */

	uint32_t t;

	if (UNINITED_RANDOM_T(rnd)) {
		/* Can use monotonic_ns() for better randomness but for now
		 * it is not used anywhere else in busybox... so avoid bloat
		 */
		INIT_RANDOM_T(rnd, getpid(), monotonic_us());
	}

	/* LCG has period of 2^32 and alternating lowest bit */
	rnd->LCG = 1664525 * rnd->LCG + 1013904223;
	/* Galois LFSR has period of 2^32-1 = 3 * 5 * 17 * 257 * 65537 */
	t = (rnd->galois_LFSR << 1);
	if (rnd->galois_LFSR < 0) /* if we just shifted 1 out of msb... */
		t ^= MASK;
	rnd->galois_LFSR = t;
	/* Both are weak, combining them gives better randomness
	 * and ~2^64 period. & 0x7fff is probably bash compat
	 * for $RANDOM range. Combining with subtraction is
	 * just for fun. + and ^ would work equally well. */
	t = (t - rnd->LCG) & 0x7fff;

	return t;
}
