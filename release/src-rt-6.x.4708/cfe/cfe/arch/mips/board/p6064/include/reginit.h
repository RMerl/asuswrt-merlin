/*
 * share/reginit.h: generic hardware initialisation engine
 *
 * Copyright (c) 2001, Algorithmics Ltd.  All rights reserved.
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

#define MOD_MASK	0x00000003
#define MOD_B		0x00000000 /* byte "modifier" */
#define MOD_H		0x00000001 /* halfword "modifier" */
#define MOD_W		0x00000002 /* word "modifier" */
#if __mips64
#define MOD_D		0x00000003 /* doubleword "modifier" */
#endif

#define OP_MASK		0x000000fc
#define	OP_EXIT		0x00000000 /* exit (status) */
#define OP_DELAY	0x00000008 /* delay (cycles) */
#define OP_RD		0x00000010 /* read (addr) */
#define OP_WR		0x00000014 /* write (addr, val) */
#define OP_RMW		0x00000018 /* read-modify-write (addr, and, or) */
#define OP_WAIT		0x00000020 /* wait (addr, mask, value) */
#define OP_JUMP		0x00000024 /* jump (addr) */

#ifdef __ASSEMBLER__

#if __mips64
#define rword	.dword
#define lr	ld
#else
#define rword	.word
#define lr	lw
#endif

	.struct	0
Init_Op:	.word	0
Init_A0:	.word	0
Init_A1:	rword	0
Init_A2:	rword	0
Init_Size:
	.previous
	
#define WR_INIT(mod,addr,val) \
	.word	OP_WR|MOD_##mod,PHYS_TO_K1(addr);\
	rword	(val),0
	
#define RD_INIT(mod,addr) \
	.word	OP_RD|MOD_##mod,PHYS_TO_K1(addr);\
	rword	0,0
	
#define RMW_INIT(mod,addr,and,or) \
	.word	OP_RMW|MOD_##mod,PHYS_TO_K1(addr);\
	rword	(and),(or)
	
#define WAIT_INIT(mod,addr,and,or) \
	.word	OP_WAIT|MOD_##mod,PHYS_TO_K1(addr);\
	rword	(mask),(val)

#define JUMP_INIT(addr) \
88:	.word	OP_JUMP,addr;\
	rword	0,0
	
#define DELAY_INIT(cycles) \
	.word	OP_DELAY,(cycles);\
	rword	0,0
	
#define EXIT_INIT(status) \
	.word	OP_EXIT,(status);\
	rword	0,0

#else

struct reginit {
    unsigned long	op;
    unsigned long	a0;
#if __mips64
    unsigned long long	a1;
    unsigned long long	a2;
#else
    unsigned long	a1;
    unsigned long	a2;
#endif
};

#define WR_INIT(mod,addr,val) \
	{OP_WR | MOD_ ## mod, PHYS_TO_K1(addr), (val)}
	
#define RD_INIT(mod,addr) \
	{OP_RD | MOD_ ## mod, PHYS_TO_K1(addr)}
	
#define RMW_INIT(mod,addr,and,or) \
	{OP_RMW | MOD_ ## mod, PHYS_TO_K1(addr), (and), (or)}
	
#define WAIT_INIT(mod,addr,and,or) \
	{OP_WAIT | MOD_ ## mod, PHYS_TO_K1(addr), (mask), (val)}
	
#define JUMP_INIT(addr) \
	{OP_JUMP, (unsigned long)(addr)}
	
#define DELAY_INIT(cycles) \
	{OP_DELAY, (cycles)}
	
#define EXIT_INIT(status) \
	{OP_EXIT, (status)}
#endif
