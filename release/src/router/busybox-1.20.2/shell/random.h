/* vi: set sw=4 ts=4: */
/*
 * $RANDOM support.
 *
 * Copyright (C) 2009 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#ifndef SHELL_RANDOM_H
#define SHELL_RANDOM_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

typedef struct random_t {
	/* Random number generators */
	int32_t galois_LFSR; /* Galois LFSR (fast but weak). signed! */
	uint32_t LCG;        /* LCG (fast but weak) */
} random_t;

#define UNINITED_RANDOM_T(rnd) \
	((rnd)->galois_LFSR == 0)

#define INIT_RANDOM_T(rnd, nonzero, v) \
	((rnd)->galois_LFSR = (nonzero), (rnd)->LCG = (v))

#define CLEAR_RANDOM_T(rnd) \
	((rnd)->galois_LFSR = 0)

uint32_t next_random(random_t *rnd) FAST_FUNC;

POP_SAVED_FUNCTION_VISIBILITY

#endif
