/*
 * Performance event support - PowerPC classic/server specific definitions.
 *
 * Copyright 2008-2009 Paul Mackerras, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/types.h>
#include <asm/hw_irq.h>

#define MAX_HWEVENTS		8
#define MAX_EVENT_ALTERNATIVES	8
#define MAX_LIMITED_HWCOUNTERS	2

/*
 * This struct provides the constants and functions needed to
 * describe the PMU on a particular POWER-family CPU.
 */
struct power_pmu {
	const char	*name;
	int		n_counter;
	int		max_alternatives;
	unsigned long	add_fields;
	unsigned long	test_adder;
	int		(*compute_mmcr)(u64 events[], int n_ev,
				unsigned int hwc[], unsigned long mmcr[]);
	int		(*get_constraint)(u64 event_id, unsigned long *mskp,
				unsigned long *valp);
	int		(*get_alternatives)(u64 event_id, unsigned int flags,
				u64 alt[]);
	void		(*disable_pmc)(unsigned int pmc, unsigned long mmcr[]);
	int		(*limited_pmc_event)(u64 event_id);
	u32		flags;
	int		n_generic;
	int		*generic_events;
	int		(*cache_events)[PERF_COUNT_HW_CACHE_MAX]
			       [PERF_COUNT_HW_CACHE_OP_MAX]
			       [PERF_COUNT_HW_CACHE_RESULT_MAX];
};

/*
 * Values for power_pmu.flags
 */
#define PPMU_LIMITED_PMC5_6	1	/* PMC5/6 have limited function */
#define PPMU_ALT_SIPR		2	/* uses alternate posn for SIPR/HV */

/*
 * Values for flags to get_alternatives()
 */
#define PPMU_LIMITED_PMC_OK	1	/* can put this on a limited PMC */
#define PPMU_LIMITED_PMC_REQD	2	/* have to put this on a limited PMC */
#define PPMU_ONLY_COUNT_RUN	4	/* only counting in run state */

extern int register_power_pmu(struct power_pmu *);

struct pt_regs;
extern unsigned long perf_misc_flags(struct pt_regs *regs);
extern unsigned long perf_instruction_pointer(struct pt_regs *regs);

#define PERF_EVENT_INDEX_OFFSET	1

/*
 * Only override the default definitions in include/linux/perf_event.h
 * if we have hardware PMU support.
 */
#ifdef CONFIG_PPC_PERF_CTRS
#define perf_misc_flags(regs)	perf_misc_flags(regs)
#endif

/*
 * The power_pmu.get_constraint function returns a 32/64-bit value and
 * a 32/64-bit mask that express the constraints between this event_id and
 * other events.
 *
 * The value and mask are divided up into (non-overlapping) bitfields
 * of three different types:
 *
 * Select field: this expresses the constraint that some set of bits
 * in MMCR* needs to be set to a specific value for this event_id.  For a
 * select field, the mask contains 1s in every bit of the field, and
 * the value contains a unique value for each possible setting of the
 * MMCR* bits.  The constraint checking code will ensure that two events
 * that set the same field in their masks have the same value in their
 * value dwords.
 *
 * Add field: this expresses the constraint that there can be at most
 * N events in a particular class.  A field of k bits can be used for
 * N <= 2^(k-1) - 1.  The mask has the most significant bit of the field
 * set (and the other bits 0), and the value has only the least significant
 * bit of the field set.  In addition, the 'add_fields' and 'test_adder'
 * in the struct power_pmu for this processor come into play.  The
 * add_fields value contains 1 in the LSB of the field, and the
 * test_adder contains 2^(k-1) - 1 - N in the field.
 *
 * NAND field: this expresses the constraint that you may not have events
 * in all of a set of classes.  (For example, on PPC970, you can't select
 * events from the FPU, ISU and IDU simultaneously, although any two are
 * possible.)  For N classes, the field is N+1 bits wide, and each class
 * is assigned one bit from the least-significant N bits.  The mask has
 * only the most-significant bit set, and the value has only the bit
 * for the event_id's class set.  The test_adder has the least significant
 * bit set in the field.
 *
 * If an event_id is not subject to the constraint expressed by a particular
 * field, then it will have 0 in both the mask and value for that field.
 */
