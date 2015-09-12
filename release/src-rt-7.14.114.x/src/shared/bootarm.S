/*
 * HNDRTE ARM boot code for standalone apps.
 *
 * Code should be position-independent until it copies itself to SRAM.
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
 * $Id: bootarm.S,v 1.18 2008-04-03 18:30:21 $
 */

#include <arminc.h>
#include <sbhndarm.h>
#include <sbconfig.h>

#include "startarm.S"

	.text

#if defined(__ARM_ARCH_7R__)
FUNC(inflrom)

#ifndef FLOPS_SUPPORT

/* In the cr4 we don't have the luxury of multiple windows into ram/rom,
 * when the rom is remapped to address 0, the ram shadowed by the rom is
 * simply not available
 *
 * So the boot process is going to be:
 *	1) Jump to the rom's real address
 *	2) Change remap
 *	3) Copy image to ram
 *	4) Jump to ram
 * Input:
 *	r9 - arm wrapper
 * Changed:
	r0-r7
 */

	/* 1) Jump to the rom's real address */
	ldr	r0,=atrom
#ifndef CONFIG_XIP
	ldr	r1,=SI_ARM_ROM
	add	r0,r0,r1
#endif
	mov	r15, r0

	/* 2) Change remap */
atrom:	
	mov	r2,r9			/* Get arm wrapper */
	ldr	r5,=AI_IOCTRL
	ldr	r3,[r2,r5]
	ldr	r4,=SICF_REMAP_MSK
	bic	r3,r3,r4
	ldr	r4,=SICF_REMAP_NONE
	orr	r3,r3,r4
	str	r3,[r2,r5]
	ldr	r3,[r2,r5]		/* read back to wait for the posted write */

	/* 3) Copy image to ram */
#ifdef	CONFIG_XIP
	/* install the exception handler in RAM */
	ldr	r0,=text_start
	eor	r1,r1,r1
	ldr	r2,=disable_arm_irq
1:	ldmia	r0!,{r4-r7}
	stmia	r1!,{r4-r7}
	cmp	r2,r0
	bhi	1b

	/* Just copy the data segment */
	ldr	r0,=rodata_end
	ldr	r1,=data_start

#else
	/* Copy entire image */
	ldr	r0,=SI_ARM_ROM
	ldr	r1,=text_start

#endif	/* CONFIG_XIP */

#else /* FLOPS_SUPPORT */
	/* for case where flops is supported, there is no need to remap
	 * or copy the image/vectors to ram. Just copying the data 
	 * segment would be enough.
	 */
	ldr	r0,=rodata_end
	ldr	r1,=data_start

#endif /* FLOPS_SUPPORT */

	ldr	r2,=data_end
2:	ldmia	r0!,{r4-r7}
	stmia	r1!,{r4-r7}
	cmp	r2,r1
	bhi	2b

	/* 4) Jump to ram */
	bx	lr
#elif defined(__ARM_ARCH_7A__)
FUNC(inflrom)

#else	/* !__ARM_ARCH_7R__ && !__ARM_ARCH_7A__ */

/*
 * Copy data and code into RAM depending on where it's booted.
 * Input:
 *	r3 - remap.
 *	r7 - (chip type << CID_TYPE_SHIFT)
 *	r11 - socram wrapper
 * Changed:
 *	r0, r1, r2, r3, r4, r5
 */
FUNC(inflrom)
	/* Don't reset SOCRAM if running from ROM */
	ldr	r0,=SICF_REMAP_ROM
	cmp	r0,r3
	beq	copytoram

/* Initialize SOCRAM */
initsocram:
	mov	r0,r11
	tst	r7,r7
	bne	ai_initsocram

	/* Do a core reset in SB chips */
	ldr	r1,=(((SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT) | SBTML_RESET)
	ldr	r2,=SBTMSTATELOW
	str	r1,[r0,r2]
	/* Read back and delay */
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	/* Clear reset */
	ldr	r1,=((SICF_FGC | SICF_CLOCK_EN) << SBTML_SICF_SHIFT)
	str	r1,[r0,r2]
	/* Read back and delay */
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	/* Leave clock enabled */
	ldr	r1,=(SICF_CLOCK_EN << SBTML_SICF_SHIFT)
	str	r1,[r0,r2]
	/* Read back and delay */
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]

	b	copytoram

	/* Do a core reset in AI chips */
ai_initsocram:
	/* Set reset while enabling the clock */
	ldr	r1,=(SICF_FGC | SICF_CLOCK_EN)
	ldr	r2,=AI_IOCTRLSET
	str	r1,[r0,r2]
	ldr	r1,=AIRC_RESET
	ldr	r2,=AI_RESETCTRL
	str	r1,[r0,r2]
	/* Read back and delay */
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]

	/* Clear reset */
	ldr	r1,=0
	str	r1,[r0,r2]
	/* Read back and delay */
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]

	/* Clear Force Gated Clock */
	ldr	r1,=SICF_FGC
	ldr	r2,=AI_IOCTRLCLEAR
	str	r1,[r0,r2]
	/* Read back and delay */
	ldr	r2,=AI_IOCTRL
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]
	ldr	r1,[r0,r2]

/* Copy data or self to SRAM */
copytoram:
#ifdef	CONFIG_XIP
	/* Just copy the data segment */
	ldr	r0,=rodata_end
	ldr	r1,=data_start
	ldr	r2,=data_end
1:	ldmia	r0!,{r3-r4}
	stmia	r1!,{r3-r4}
	cmp	r2,r1
	bhi	1b
	mov	pc,lr
#else
	/* Copy entire image */
	ldr	r0,=SI_ARM_ROM
	ldr	r2,=SICF_REMAP_ROM
	cmp	r3,r2
	beq	1f
	ldr	r0,=SI_FLASH2
1:	ldr	r1,=text_start

	/* Copy upto donecopy and then run docopy from SRAM */
	ldr	r2,=donecopy
	ldr     r3,=SI_ARM_SRAM2
	add	r1,r1,r3
	add	r2,r2,r3
	ldr	r3,=mapoff
	b	docopy

/* Now change RAM mapping to NONE/SRAM */
mapoff:
	mov	r2,r9			/* Get arm wrapper */
	tst	r7,r7
	bne	1f

	/* For SB chips, get sbtmstatelow and shift it */
	ldr	r5,=SBTMSTATELOW
	ldr	r3,[r2,r5]
	ldr	r6,=SBTML_SICF_SHIFT
	_LSR_	r3,r3,r6
	b	2f

	/* For AI chips, just get ioctrl register */
1:	ldr	r5,=AI_IOCTRL
	ldr	r3,[r2,r5]
	eor	r6,r6,r6

2:	ldr	r4,=SICF_REMAP_MSK
	bic	r3,r3,r4
	ldr	r4,=SICF_REMAP_NONE
	orr	r3,r3,r4
	_LSL_	r3,r3,r6
3:	str	r3,[r2,r5]
	/* read back to wait for the posted write */
	ldr	r3,[r2,r5]
	nop
	nop

	/* Copy after donecopy using docopy in SRAM */
	ldr	r2,=data_end
	ldr     r3,=SI_ARM_SRAM2
	add	r2,r2,r3
	mov	r3,lr

docopy:
	ldmia	r0!,{r4}
	stmia	r1!,{r4}
	cmp	r2,r1
	bhi	docopy
	mov	pc,r3

	/* Make sure any literals needed by the code above are "close" and
	 * have been copied to ram before we switch:
	 */
	.ltorg
donecopy:
#endif	/* CONFIG_XIP */
#endif	/* __ARM_ARCH_7R__ */	
END(inflrom)
