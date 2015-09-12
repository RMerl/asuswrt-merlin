/*
 * HND SiliconBackplane ARM core software interface.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: hndarm.h 437561 2013-11-19 08:31:38Z $
 */

#ifndef _hndarm_h_
#define _hndarm_h_

#include <sbhndarm.h>

extern void *hndarm_armr;
extern uint32 hndarm_rev;


extern void si_arm_init(si_t *sih);

extern void enable_arm_irq(void);
extern void disable_arm_irq(void);
extern void enable_arm_fiq(void);
extern void disable_arm_fiq(void);
extern void enable_nvic_ints(uint32 which);
extern void disable_nvic_ints(uint32 which);

extern uint32 get_arm_cyclecount(void);
extern void set_arm_cyclecount(uint32 ticks);

#ifdef	__ARM_ARCH_7R__
extern uint32 get_arm_perfcount_enable(void);
extern void set_arm_perfcount_enable(uint32 which);
extern uint32 set_arm_perfcount_disable(void);

extern uint32 get_arm_perfcount_sel(void);
extern void set_arm_perfcount_sel(uint32 which);

extern uint32 get_arm_perfcount_event(void);
extern void set_arm_perfcount_event(uint32 which);

extern uint32 get_arm_perfcount(void);
extern void set_arm_perfcount(uint32 which);

extern void enable_arm_cyclecount(void);
extern void disable_arm_cyclecount(void);

extern uint32 get_arm_data_fault_status(void);
extern uint32 get_arm_data_fault_address(void);

extern uint32 get_arm_instruction_fault_status(void);
extern uint32 get_arm_instruction_fault_address(void);
#endif	/* __ARM_ARCH_7R__ */

extern uint32 get_arm_inttimer(void);
extern void set_arm_inttimer(uint32 ticks);

extern uint32 get_arm_intmask(void);
extern void set_arm_intmask(uint32 ticks);

extern uint32 get_arm_intstatus(void);
extern void set_arm_intstatus(uint32 ticks);

extern uint32 get_arm_firqmask(void);
extern void set_arm_firqmask(uint32 ticks);

extern uint32 get_arm_firqstatus(void);
extern void set_arm_firqstatus(uint32 ticks);

extern void arm_wfi(si_t *sih);
extern void arm_jumpto(void *addr);

extern void traptest(void);

extern uint32 si_arm_sflags(si_t *sih);
extern uint32 si_arm_disable_deepsleep(bool disable);

#ifdef BCMOVLHW
#define	BCMOVLHW_ENAB(sih)		TRUE

extern int si_arm_ovl_remap(si_t *sih, void *virt, void *phys, uint size);
extern int si_arm_ovl_reset(si_t *sih);
extern bool si_arm_ovl_vaildaddr(si_t *sih, void *virt);
extern bool si_arm_ovl_isenab(si_t *sih);
extern bool si_arm_ovl_int(si_t *sih, uint32 pc);
#else
#define	BCMOVLHW_ENAB(sih)		FALSE

#define si_arm_ovl_remap(a, b, c, d)	do {} while (0)
#define si_arm_ovl_reset(a)		do {} while (0)
#define si_arm_ovl_int(a, b)		FALSE
#endif

int32 si_arm_clockratio(si_t *sih, uint8 div);

#endif /* _hndarm_h_ */
