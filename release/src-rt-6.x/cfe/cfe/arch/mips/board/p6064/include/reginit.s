/*
 * share/reginit.s: generic hardware initialisation engine
 *
 * Copyright (c) 1998-1999, Algorithmics Ltd.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the "Free MIPS" License Agreement, a copy of 
 * which is available at:
 *
 *  http://www.algor.co.uk/ftp/pub/doc/freemips-license.txt
 *
 * You may not, however, modify or remove any part of this copyright 
 * message if this program is redistributed or reused in whole or in
 * part.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * "Free MIPS" License for more details.  
 */
	
#include "reginit.h"	
	
LEAF(reginit)			/* local name */


looptop:
	lw	t3, Init_Op(a0)
	lw	t0, Init_A0(a0)
	and	t4,t3,OP_MASK
	
	
	/* 
	 * EXIT(STATUS) 
	 */
	bne	t4, OP_EXIT, 8f
	move	v0,t0
	b	.done

	
	/* 
	 * DELAY(CYCLES) 
	 */
8:	bne	t4, OP_DELAY, 8f
	.set noreorder
1:	bnez	t0,1b
	subu	t0,1
	.set reorder
	b	.next
	
	/* 
	 * READ(ADDR) 
	 */
8:	bne	t4,OP_RD,8f
	and	t4,t3,MOD_MASK
	
	bne	t4,MOD_B,1f
	lbu	t5,0(t0)
	b	.next
	
1:	bne	t4,MOD_H,1f
	lhu	t5,0(t0)
	b	.next
	
1:	bne	t4,MOD_W,1f
#if __mips64
	lwu	t5,0(t0)
#else
	lw	t5,0(t0)
#endif		
	b	.next
	
1:	
#if __mips64
	ld	t5,0(t0)
	b	.next
#else
	b	.fatal
#endif
	
	/* 
	 * WRITE(ADDR,VAL) 
	 */
8:	bne	t4,OP_WR,8f
	lr	t1,Init_A1(a0)
	and	t4,t3,MOD_MASK
	
	bne	t4,MOD_B,1f
	sb	t1,0(t0)
	b	.next
	
1:	bne	t4,MOD_H,1f
	sh	t1,0(t0)
	b	.next
	
1:	bne	t4,MOD_W,1f
	sw	t1,0(t0)
	b	.next
	
1:	
#if __mips64
	sd	t1,0(t0)
	b	.next
#else
	b	.fatal
#endif
		
	
	/* 
	 * RMW(ADDR,AND,OR) 
	 */
8:	bne	t4,OP_RMW,8f
	lr	t1,Init_A1(a0)
	lr	t2,Init_A2(a0)
	and	t4,t3,MOD_MASK
	
	bne	t4,MOD_B,1f
	lbu	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sb	t4,0(t0)
	b	.next
	
1:	bne	t4,MOD_H,1f
	lhu	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sh	t4,0(t0)
	b	.next
	
1:	bne	t4,MOD_W,1f
	lw	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sw	t4,0(t0)
	b	.next
	
1:		
#if __mips64
	ld	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sd	t4,0(t0)
	b	.next
#else	
	b	.fatal
#endif
		
	
	/* 
	 * WAIT(ADDR,MASK,VAL) 
	 */
8:	bne	t4,OP_WAIT,8f
	lr	t1,Init_A1(a0)
	lr	t2,Init_A2(a0)
	and	t4,t3,MOD_MASK
	
	bne	t4,MOD_B,1f
3:	lbu	t4,0(t0)
	and	t4,t1
	bne	t4,t2,3b
	b	.next
	
1:	bne	t4,MOD_H,1f
3:	lhu	t4,0(t0)
	and	t4,t1
	bne	t4,t2,3b
	b	.next
	
1:	bne	t4,MOD_W,1f
3:	lw	t4,0(t0)
	and	t4,t1
	bne	t4,t2,3b
	b	.next
	
1:		
#if __mips64
3:	ld	t4,0(t0)
	and	t4,t1
	bne	t4,t2,3b
	b	.next
#else	
	b	.fatal
#endif
	
	/* 
	 * JUMP(ADDR) 
	 */
8:	bne	t4, OP_JUMP, 8f
	move	a0,t0
	b	looptop
	
	
.next:	addu	a0,Init_Size
	b	looptop
	
8:
.fatal:	li	v0,-1		
	
.done:	j	ra	

END(reginit)
