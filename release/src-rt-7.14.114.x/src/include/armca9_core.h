/*
 * ARM CA9 (iHost) definitions
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
 * $Id$
 */

#ifndef	_armca9_core_h_
#define	_armca9_core_h_

#if !defined(_LANGUAGE_ASSEMBLY) && !defined(__ASSEMBLY__)

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

typedef volatile struct {
	uint32	PAD[1024];
} armca9regs_t;

#endif	/* !defined(_LANGUAGE_ASSEMBLY) && !defined(__ASSEMBLY__) */

#define IHOST_PROC_CLK_WR_ACCESS			0x19000000
#define IHOST_PROC_CLK_POLICY_FREQ			0x19000008
#define IHOST_PROC_CLK_POLICY_CTL			0x1900000c
#define IHOST_PROC_CLK_POLICY_CTL__GO			0
#define IHOST_PROC_CLK_POLICY_CTL__GO_AC		1

#define IHOST_PROC_CLK_CORE0_CLKGATE			0x19000200
#define IHOST_PROC_CLK_CORE1_CLKGATE			0x19000204
#define IHOST_PROC_CLK_ARM_SWITCH_CLKGATE		0x19000210
#define IHOST_PROC_CLK_ARM_PERIPH_CLKGATE		0x19000300
#define IHOST_PROC_CLK_APB0_CLKGATE			0x19000400

#define IHOST_PROC_CLK_PLLARMA				0x19000c00
#define IHOST_PROC_CLK_PLLARMA__PLLARM_SOFT_POST_RESETB	1
#define IHOST_PROC_CLK_PLLARMA__PLLARM_LOCK		28

/* SCU registers */
#define IHOST_SCU_CONTROL					0x19020000
#define IHOST_SCU_INVALIDATE				0x1902000c

/* L2C registers */
#define IHOST_L2C_CACHE_ID					0x19022000

#endif	/* _armca9_core_h_ */
