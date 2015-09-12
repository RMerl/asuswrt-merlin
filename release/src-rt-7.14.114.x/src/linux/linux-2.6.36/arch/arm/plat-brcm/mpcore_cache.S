/*****************************************************************************
* Copyright 2003 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#include <linux/linkage.h>
#include <linux/init.h>

	__INIT

/*
 * v7_l1_cache_invalidate
 *
 * Invalidate contents of L1 cache without flushing its contents
 * into outer cache and memory. This is needed when the contents
 * of the cache are unpredictable after power-up.
 *
 * corrupts r0-r6
 */

ENTRY(v7_l1_cache_invalidate)
        mov     r0, #0
        mcr     p15, 2, r0, c0, c0, 0	@ set cache level to 1
        mrc     p15, 1, r0, c0, c0, 0	@ read CLIDR

        ldr     r1, =0x7fff
        and     r2, r1, r0, lsr #13	@ get max # of index size

        ldr     r1, =0x3ff
        and     r3, r1, r0, lsr #3	@ NumWays - 1
        add     r2, r2, #1		@ NumSets

        and     r0, r0, #0x7
        add     r0, r0, #4		@ SetShift

        clz     r1, r3			@ WayShift
        add     r4, r3, #1		@ NumWays
1:      sub     r2, r2, #1		@ NumSets--
        mov     r3, r4			@ Temp = NumWays
2:      subs    r3, r3, #1		@ Temp--
        mov     r5, r3, lsl r1
        mov     r6, r2, lsl r0
        orr     r5, r5, r6		@ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
        mcr     p15, 0, r5, c7, c6, 2	@ Invalidate line
        bgt     2b
        cmp     r2, #0
        bgt     1b
        dsb
        mov     r0,#0
        mcr     p15,0,r0,c7,c5,0                /* Invalidate icache */
        isb
        mov     pc, lr
ENDPROC(v7_l1_cache_invalidate)

/*
 * v7_all_dcache_invalidate
 *
 * Invalidate without flushing the contents of all cache levels
 * accesible by the current processor core.
 * This is useful when the contents of cache memory are undetermined
 * at power-up.
 *	Corrupted registers: r0-r7, r9-r11 
 *
 * Based on cache-v7.S: v7_flush_dcache_all()
 */

ENTRY(v7_all_dcache_invalidate)
	mrc	p15, 1, r0, c0, c0, 1	@ read clidr
	ands	r3, r0, #0x7000000	@ extract loc from clidr
	mov	r3, r3, lsr #23		@ left align loc bit field
	beq	finished		@ if loc is 0, then no need to clean
	mov	r10, #0			@ start clean at cache level 0
loop1:
	add	r2, r10, r10, lsr #1	@ work out 3x current cache level
	mov	r1, r0, lsr r2		@ extract cache type bits from clidr
	and	r1, r1, #7		@ mask of bits for current cache only
	cmp	r1, #2			@ see what cache we have at this level
	blt	skip			@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
	isb				@ isb to sych the new cssr&csidr
	mrc	p15, 1, r1, c0, c0, 0	@ read the new csidr
	and	r2, r1, #7		@ extract the length of the cache lines
	add	r2, r2, #4		@ add 4 (line length offset)
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3	@ find maximum number on the way size
	clz	r5, r4			@ find bit pos of way size increment
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13	@ extract max number of the index size
loop2:
	mov	r9, r4			@ create working copy of max way size
loop3:
 	orr	r11, r10, r9, lsl r5	@ factor way and cache number into r11
 	orr	r11, r11, r7, lsl r2	@ factor index number into r11
        mcr     p15, 0, r11, c7, c6, 2	@ Invalidate line
	subs	r9, r9, #1		@ decrement the way
	bge	loop3
	subs	r7, r7, #1		@ decrement the index
	bge	loop2
skip:
	add	r10, r10, #2		@ increment cache number
	cmp	r3, r10
	bgt	loop1
finished:
	mov	r10, #0			@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
	dsb
	isb
	mov	pc, lr
ENDPROC(v7_all_dcache_invalidate)
