/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998, 1999, 2003 by Ralf Baechle
 *
 * FIXME: For some of the supported machines this is dead wrong.
 */
#ifndef _ASM_TIMEX_H
#define _ASM_TIMEX_H

#include <asm/mipsregs.h>

/*
 * Standard way to access the cycle counter.
 * Currently only used on SMP for scheduling.
 *
 * Only the low 32 bits are available as a continuously counting entity.
 * But this only means we'll force a reschedule every 8 seconds or so,
 * which isn't an evil thing.
 *
 * We know that all SMP capable CPUs have cycle counters.
 */

typedef unsigned int cycles_t;
extern cycles_t cacheflush_time;

static inline cycles_t get_cycles (void)
{
	return read_c0_count();
}

extern __u32 get_htscl(void);
extern __u64 get_tscll(void);

#define rdtsc(low, high) \
		high = get_htscl(); \
		low = read_c0_count();

#define rdtscl(low) \
		low = read_c0_count();

#define rdtscll(val) \
		val = get_tscll();

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif /*  _ASM_TIMEX_H */
