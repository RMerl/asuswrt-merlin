/*
 * HND SiliconBackplane ARM core software interface.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndarm.h,v 13.12 2008-03-28 19:05:46 Exp $
 */

#ifndef _hndarm_h_
#define _hndarm_h_

#include <sbhndarm.h>

extern void *hndarm_armr;
extern uint32 hndarm_rev;


extern void si_arm_init(si_t *sih);

extern void enable_arm_ints(uint32 which);
extern void disable_arm_ints(uint32 which);

extern uint32 get_arm_cyclecount(void);
extern void set_arm_cyclecount(uint32 ticks);

extern uint32 get_arm_inttimer(void);
extern void set_arm_inttimer(uint32 ticks);

extern uint32 get_arm_intmask(void);
extern void set_arm_intmask(uint32 ticks);

extern uint32 get_arm_intstatus(void);
extern void set_arm_intstatus(uint32 ticks);

extern void arm_wfi(si_t *sih);
extern void arm_jumpto(void *addr);

#endif /* _hndarm_h_ */
