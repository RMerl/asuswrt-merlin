/*
 * MIPS 74k definitions
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: mips74k_core.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef	_mips74k_core_h_
#define	_mips74k_core_h_

#include <mipsinc.h>

#ifndef _LANGUAGE_ASSEMBLY

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

typedef volatile struct {
	uint32	corecontrol;
	uint32	exceptionbase;
	uint32	PAD[1];
	uint32	biststatus;
	uint32	intstatus;
	uint32	intmask[6];
	uint32	nmimask;
	uint32	PAD[4];
	uint32	gpioselect;
	uint32	gpiooutput;
	uint32	gpioenable;
	uint32	PAD[101];
	uint32	clkcontrolstatus;
} mips74kregs_t;

/* Core specific status flags */
#define SISF_CHG_CLK_OTF_PRESENT	0x0001

#endif	/* _LANGUAGE_ASSEMBLY */

#endif	/* _mips74k_core_h_ */
