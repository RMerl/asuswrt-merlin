/*
 * p6032/sbdreset.sx: low level reset code for P-6032/P-6064
 *
 * Copyright (c) 2000 Algorithmics Ltd - all rights reserved.
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

#define	P60XX_I2C
#define	P60XX_PARITY

#include "sbmips.h"		
#include "sbd.h"

#include "hd2532.h"
#include "i82371eb.h"
#include "i8254.h"
#include "pc97307.h"
#include "isapnpreg.h"
#include "pcireg.h"
#define PCI_MAP_IO 1
#include "prid.h"
#include "mipsmacros.h"

/*
 * R4000 Config Register 
 */
//#ifndef CFG_ECMASK
#define CFG_CM          0x80000000      /* Master-Checker mode */
#define CFG_ECMASK      0x70000000      /* System Clock Ratio */
#define CFG_ECSHIFT     28
#define CFG_ECBY2       0x00000000      /* divide by 2 */
#define CFG_ECBY3       0x00000000      /* divide by 3 */
#define CFG_ECBY4       0x00000000      /* divide by 4 */
#define CFG_EPMASK      0x0f000000      /* Transmit data pattern */
#define CFG_EPD         0x00000000      /* D */
#define CFG_EPDDX       0x01000000      /* DDX */
#define CFG_EPDDXX      0x02000000      /* DDXX */
#define CFG_EPDXDX      0x03000000      /* DXDX */
#define CFG_EPDDXXX     0x04000000      /* DDXXX */
#define CFG_EPDDXXXX    0x05000000      /* DDXXXX */
#define CFG_EPDXXDXX    0x06000000      /* DXXDXX */
#define CFG_EPDDXXXXX   0x07000000      /* DDXXXXX */
#define CFG_EPDXXXDXXX  0x08000000      /* DXXXDXXX */
#define CFG_SBMASK      0x00c00000      /* Secondary cache block size */
#define CFG_SBSHIFT     22
#define CFG_SB4         0x00000000      /* 4 words */
#define CFG_SB8         0x00400000      /* 8 words */
#define CFG_SB16        0x00800000      /* 16 words */
#define CFG_SB32        0x00c00000      /* 32 words */
#define CFG_EMMASK      0x00c00000      /* Vr54xx: SysAD mode */
#define CFG_EMSHIFT     22
#define CFG_EM_R4K      0x00000000      /* Vr54xx: R4x000 compatible */
#define CFG_EM_SPLITRD  0x00400000      /* Vr54xx: Multiple split reads */
#define CFG_EM_PIPEWR   0x00800000      /* Vr54xx: Pipeline writes */
#define CFG_EM_WRREISSU 0x00c00000      /* Vr54xx: Write-reissue */
#define CFG_AD          0x00800000      /* Accelerated data (R4100) */
#define CFG_SS          0x00200000      /* Split secondary cache */
#define CFG_SW          0x00100000      /* Secondary cache port width */
#define CFG_EWMASK      0x000c0000      /* System port width */
#define CFG_EWSHIFT     18
#define CFG_EW64        0x00000000      /* 64 bit */
#define CFG_EW32        0x00040000      /* 32 bit */
#define CFG_SC          0x00020000      /* Secondary cache absent */
#define CFG_SM          0x00010000      /* Dirty Shared mode disabled */
#define CFG_BE          0x00008000      /* Big Endian */
#define CFG_EM          0x00004000      /* ECC mode enable */
#define CFG_EB          0x00002000      /* Block ordering */
#define CFG_ICMASK      0x00000e00      /* Instruction cache size */
#define CFG_ICSHIFT     9
#define CFG_DCMASK      0x000001c0      /* Data cache size */
#define CFG_DCSHIFT     6
#define CFG_IB          0x00000020      /* Instruction cache block size */
#define CFG_DB          0x00000010      /* Data cache block size */
#define CFG_CU          0x00000008      /* Update on Store Conditional */
#define CFG_K0MASK      0x00000007      /* KSEG0 coherency algorithm */
//#endif

/*
 * Primary cache mode
 */
#define CFG_C_WTHRU_NOALLOC     0       /* r4600 only */
#define CFG_C_WTHRU_ALLOC       1       /* r4600 only */
#define CFG_C_UNCACHED          2
#define CFG_C_NONCOHERENT       3
#define CFG_C_WBACK             3
#define CFG_C_COHERENTXCL       4
#define CFG_C_COHERENTXCLW      5
#define CFG_C_COHERENTUPD       6       /* r4000/r4400 only */
#define CFG_C_UNCACHED_ACCEL    7       /* t5 only */


		
//#undef DBGSBD
#define DBGSBD 0
	
/* CPU-specific bootmode bitstreams */	
#ifdef __MIPSEB
#define BOOTMODE_RM7000		0x02034100
#define BOOTMODE_RM52XX		0x00230100
#define BOOTMODE_R4650		0x00001100
#define BOOTMODE_RC6447X	0x00001100
#define BOOTMODE_RC6457X	0x06001100
#else
#define BOOTMODE_RM7000		0x02034000
#define BOOTMODE_RM52XX		0x00230000
#define BOOTMODE_R4650		0x00001000
#define BOOTMODE_RC6447X	0x00001000
#define BOOTMODE_RC6457X 	0x06001000
#endif
	
#define WBFLUSH 

#define MEG	0x100000		
	
#define DISPLAY(d0,d1,d2,d3,d4,d5,d6,d7) \
	li	t8,PHYS_TO_K1(LED_BASE+HD2532_CRAM);	\
	li	t9,d0;				\
	sw	t9,HD2532_CHAR(0)(t8); 		\
	li	t9,d1; 				\
	sw	t9,HD2532_CHAR(1)(t8);		\
	li	t9,d2;	 			\
	sw	t9,HD2532_CHAR(2)(t8);		\
	li	t9,d3;	 			\
	sw	t9,HD2532_CHAR(3)(t8);		\
	li	t9,d4;				\
	sw	t9,HD2532_CHAR(4)(t8); 		\
	li	t9,d5; 				\
	sw	t9,HD2532_CHAR(5)(t8);		\
	li	t9,d6;	 			\
	sw	t9,HD2532_CHAR(6)(t8);		\
	li	t9,d7;	 			\
	sw	t9,HD2532_CHAR(7)(t8)

	

#if !defined(DBGSBD)
#define DBGDISPLAY(d0,d1,d2,d3,d4,d5,d6,d7)
#define DBGLA(reg, label)
#define DBGLW(reg, label)
#define DBGSTRING(label, string)
#define DBGTSTRING(string)
#define DBGLSTR(reg, string)
#define DBGPSTR(string)
#else	
#define DBGDISPLAY(d0,d1,d2,d3,d4,d5,d6,d7) \
	DISPLAY(d0,d1,d2,d3,d4,d5,d6,d7); \
	li	t8,ROMMS(500); \
	.set	noreorder; \
99:	bnez	t8,99b; \
	subu	t8,1; \
	.set	reorder

#define DBGLA(reg, label)	 \
	LOADREL(reg,label);		 \
	or	reg,K1BASE
		
#define DBGLW(reg, label)	\
	DBGLA(reg,label);	\
	lw	reg,0(reg)

#define DBGSTRING(label, string)	\
	.rdata;			\
label:	.asciiz string;		\
	.previous
	
#define DBGTSTRING(string)		\
	.rdata;			\
	.word	9f;		\
	.section .rodata1; 	\
9:	.asciiz string

#define DBGLSTR(reg, string)	\
	DBGLA(reg,9f);		\
	DBGSTRING(9,string)

/* print a string */
#define DBGPSTR(string)	\
	DBGLSTR(a0,string);		\
	bal	rom_prs
	
#include "ns16550.h"
	
#define TXWAIT		10000
	
#ifndef NS16550_INB
#define NS16550_INB(dst,offs,base) \
	lbu	dst,offs(base)
#endif
	
#ifndef NS16550_OUTB
#define NS16550_OUTB(src,offs,base) \
	sb	src,offs(base)
#endif

	.text
reset_table:
	_LONG_	reginit	
	
/*
 * rom_outch (int c)
 * Print a character on console device
 */
SLEAF(rom_outch)
	/* blocking transmit, with timeout */
	li	t0,TXWAIT			# timeout
	li	t1,PHYS_TO_K1(UART0_BASE)
1:	NS16550_INB(v0,LSR,t1)			# get LSR
	and	v0,LSR_TXRDY			# tx ready?
	bnez	v0,1f				# yup - go and write
	subu	t0,1				# continue until timeout
	bnez	t0,1b
1:	NS16550_OUTB(a0,DATA,t1)		# write data
	j	ra
SEND(rom_outch)
	

/*
 * rom_prnl ()
 * Print <CR, LF> on console device
 */
LEAF(rom_prnl)
	move	t7,ra
	li	a0,'\r'
	bal	rom_outch
	li	a0,'\n'
	bal	rom_outch
	j	t7
END(rom_prnl)


/*
 * rom_prs (char *s)
 * Print a string on console device
 */
LEAF(rom_prs)
	move	t7,ra
	move	t6,a0

1:	lbu	a0,0(t6)
	beqz	a0,2f
	bal	rom_outch
	addu	t6,1
	b	1b

2:	j	t7
END(rom_prs)


/*
 * rom_prx (unsigned int x, unsigned int log2size)
 * Print a register on console device as hex digits
 */
LEAF(rom_prx)
	move	t7,ra
	
	li	t5,8			# t5 = bitcnt = 8 << log2size
	sll	t5,a1			
	
#if __mips64
	li	t6,64			# t6 = regsize - bitcnt
	subu	t6,t5
	dsll	t6,a0,t6		# get sig part of a0 to top of t6
1:	dsrl	a0,t6,60		# get top 4 bits
	addu	a0,'0'
	ble	a0,'9',2f
	addu	a0,'A'-'0'-10
2:	bal	rom_outch
	dsll	t6,4
	subu	t5,4
	bnez	t5,1b
#else
	li	t6,32			# t6 = regsize - bitcnt
	subu	t6,t5
	sll	t6,a0,t6		# get sig part of a0 to top of t6
1:	srl	a0,t6,28		# get top 4 bits
	addu	a0,'0'
	ble	a0,'9',2f
	addu	a0,'A'-'0'-10
2:	bal	rom_outch
	sll	t6,4
	subu	t5,4
	bnez	t5,1b
#endif	
	
	j	t7
END(rom_prx)

SLEAF(_rom_consinit)
	li	v0,PHYS_TO_K1(UART0_BASE)
	# enable 16550 fifo if it is there
	li	t0,FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_4
	NS16550_OUTB(t0,FIFO,v0)
	
	# 100us delay 
	.set	noreorder
	li	t0,CACHEUS(100)
1:	bnez	t0,1b
	subu	t0,1
	.set	reorder
	
	li	a1,9600

	/* wait for all tx data to be sent */
	li	t0,TXWAIT
1:	NS16550_INB(t1,LSR,v0)
	and	t1,LSR_TSRE
	bnez	t1,2f
	subu	t0,1
	bnez	t0,1b
	
	/* convert baud rate in a1 into register value */
2:	beqz	a1,.fail			# don't want divide error
	li	t2,NS16550_HZ/16		# brtc = CLK/16/speed
	divu	t2,a1
	blez	t2,.fail			# brtc must be > 0
	
	li	t0,CFCR_DLAB			# select brtc divisor
	NS16550_OUTB(t0,CFCR,v0)			
	NS16550_OUTB(t2,DLL,v0)			# store divisor lsb
	srl	t2,8	
	NS16550_OUTB(t2,DLM,v0)			# store divisor msb
	li	t0,CFCR_8BITS			# set 8N1 mode
	NS16550_OUTB(t0,CFCR,v0)
	
	li	t0,MCR_DTR|MCR_RTS|MCR_IENABLE	# enable DTR & RTS
	NS16550_OUTB(t0,MCR,v0)
	
	li	t0,IER_ERXRDY			# enable receive interrupt(!)
	NS16550_OUTB(t0,IER,v0)
	
	move	v0,zero				# indicate success
	j	ra
	
.fail:	li	v0,1
	j	ra
	
SEND(_rom_consinit)			
#endif
	
/*
 * Basic board initialisation, called straight from RESET.
 * It is only called if this program is built for ROM.  
 * It can use any registers except s8, k0 and k1.
 *
 * Note that s8 holds a "relocation factor" (see ../share/romlow.sx)
 * which must be added to any address before it is used.  This
 * is to support relocatable roms.
 */
	
#define tmpsize		s0
#define msize		s1
#define sdShape		s2
#define bonito		s3
#define dbg		s4
#define sdCfg		s5
#define	rasave		s7

LEAF(board_earlyinit)
	move	rasave,ra
	
#ifdef DBGSBD
	/* Initialise the LED so we can write to it */
	li	a1, PHYS_TO_K1(LED_BASE+HD2532_CW)
	li	a2,HD2532_CW_C
	sw	a2,(a1)

	.set	noreorder
	li	t0,CACHEUS(110)
1:	bnez	t0,1b
	subu	t0,1
	.set	reorder

	DBGDISPLAY('P','6','0','6','4',' ',' ',' ');
#endif
	
	mfc0	t2,C0_PRID		# get PrID
	mtc0	zero,$18		# C0_IWATCH/C0_WATCHLO
	mtc0	zero,$19		# C0_DWATCH/C0_WATCHHI

	/* get global pointer to Bonito registers */
	li	bonito,PHYS_TO_K1(BONITO_BASE)
	
	srl	t2,8
	and	t2,0xff

	/*
	 * some CPUs need a non-zero bootmode stream
	 * we achieve this by loading the bitstream into the intpol
	 * register and issuing a self-reset
	 * but first we must avoid getting into an infinite loop...
	 */

	lw	t3,BONITO_BONGENCFG(bonito)
	and	t3,BONITO_BONGENCFG_CPUSELFRESET
	bnez	t3,.noreset

	b	.noreset
	
	
//	.rdata
	.align	2
#define QEDCDIV(mb20,mb7_5) \
	.word	0+((mb20<<20)|(mb7_5<<5))
		
cdiv_qed:
	QEDCDIV(0,0)	# x2
	QEDCDIV(1,3)	# x2.5
	QEDCDIV(0,1)	# x3
	QEDCDIV(1,5)	# x3.5
	QEDCDIV(0,2)	# x4
	QEDCDIV(1,7)	# x4.5
	QEDCDIV(0,3)	# x5
	QEDCDIV(0,3)	# x5
	
	
	.struct 0
#define CPU(prid,bootmode,cdiv,xtra) \
	.word	prid,bootmode,cdiv,xtra
CPU_PRID:	.word	0
CPU_BOOTMODE:	.word	0
CPU_CDIV:	.word	0
CPU_XTRA:	.word	0
CPU_SIZE:	
	.previous
cpu_data:
	CPU(PRID_RM7000,BOOTMODE_RM7000,cdiv_qed,xtra_rm7000)
	CPU(PRID_RM52XX,BOOTMODE_RM52XX,cdiv_qed,0)
	CPU(PRID_RC6447X,0,0,BOOTMODE_RC6447X)
	CPU(PRID_RC6457X,0,BOOTMODE_RC6457X,0)
	CPU(PRID_R4650,0,BOOTMODE_R4650,0)
	.word	0
	.text

	DISPLAY('R','e','s','e','t','C','h','k')

	LOADREL(t1,cpu_data)	
	or	t1,K1BASE
	
1:	lw	t3,CPU_PRID(t1)
	beqz	t3,.noreset		# off end of table
	beq	t3,t2,.reset
	addu	t1,CPU_SIZE
	b	1b
		
.reset:	
	DISPLAY('R','e','s','e','t','R','e','q')
	# use the switch settings to modify the base settings 

	# to access the switches we need to enable CS0 buffbit
	li	t4,BONITO_IODEVCFG_BUFFBIT_CS0
	sw	t4,BONITO_IODEVCFG(bonito)
	lw	zero,BONITO_IODEVCFG(bonito)

	li	t4,PHYS_TO_K1(CPLD_BASE)
	lw	t4,CPLD_SWITCHES(t4)

	lw	t3,CPU_BOOTMODE(t1)	# base setting
	lw	t5,CPU_CDIV(t1)
	beqz	t5,1f			# skip CDIV settings if we don't know how

	# get clock multiplier switches
	or	t5,K1BASE
	
	and	t6,t4,CPLD_CLKMULT
	srl	t7,t6,CPLD_CLKMULT_SHIFT
	sll	t7,2
	addu	t5,t7
	lw	t6,0(t5)
	
	or	t3,t6			# set multiplier bits

1:	/* others... */		
	lw	t5,CPU_XTRA(t1)
	beqz	t5,.doreset
	or	t5,K1BASE
	j	t5

xtra_rm7000:
	# set scache 
	b	.doreset
	
		
.doreset:
	DISPLAY('R','e','s','e','t','G','o',' ')
	sw	t3,BONITO_INTPOL(bonito)
	lw	t3,BONITO_BONGENCFG(bonito)
	or	t3,BONITO_BONGENCFG_CPUSELFRESET
	sw	t3,BONITO_BONGENCFG(bonito)
	lw	t3,BONITO_BONGENCFG(bonito)
1:	b	1b		# loop forever
	
.noreset:
	DISPLAY('N','o',' ','R','e','s','e','t')
	
	/* if we get here, the cpu has been correctly initialised */
	bne	t2,PRID_R4650,2f
	
	/* r4640/50 initialisation */
	mtc0	zero,$0			# C0_IBASE
	mtc0	zero,$1			# C0_IBOUND
	mtc0	zero,$2			# C0_DBASE
	mtc0	zero,$3			# C0_DBOUND
#ifndef BOOTPKG
	/* Making this dependent on BOOTPKG is wrong, but it is done this
	 * way to stop PMON reenabling the caches if ITROM has detected an
	 * error
	 */
	li	t3,0x22233333
	mtc0	t3,$17			# C0_CALG
#endif	
	b	3f
	
2:	/* r4xxx/r5xxx initialisation */
	/* set config register for 32b/32b cachelines, kseg0 cacheable */
	and	t1,~0x3f		# set bits 5..0 only
	or	t1,CFG_IB | CFG_DB | CFG_C_WBACK
	/* Vr4300/5432: software controls the endianness (r/o on other CPUs) */
#ifdef __MIPSEB
//	or	t1,CFG_BE	
#else
//	and	t1,~CFG_BE	
#endif
	/* set DDDD rate for CPUs that aren't hardware configured */
	and	t1,~CFG_EPMASK
	or	t1,CFG_EPD
	bne	t2,PRID_R5400,1f
	/* Vr5400: set R4x00 compatible bus mode */
	and	t1,~CFG_EMMASK
	or	t1,CFG_EM_R4K
	b	2f
	/* Vr4100: switch off accelerated data (undefined on other CPUs)*/
1:	and	t1,~CFG_AD
2:	mtc0	t1,C0_CONFIG
	
3:	mfc0	t1,C0_STATUS		# get Status
	mtc0	zero,C0_CAUSE
	and	t1,M_SR_SR		# leave the SoftReset bit
	or	t1,M_SR_BEV		# force Boot Exception Vec
	mtc0	t1,C0_STATUS
	
	/* Initialise other low-level I/O devices */

	LOADREL(a0,reginittab)
	or	a0,K1BASE
	LOADREL(t1,reset_table)
	or	t1,K1BASE
	LW	t1,0(t1)
	or	t1,K1BASE

	DISPLAY('R','e','g','i','n','i','t',' ')

	jal	t1

#if DBGSBD > 0
	
	li	a0,PHYS_TO_K1(PCI_IO_SPACE)
	li	a1,0x55
	li	a2,0xaa
	sb	a1,0x4d0(a0)
	sb	a2,0x4d1(a0)
	sb	a1,0x4d2(a0)
	sb	a2,0x4d3(a0)
	lbu	zero,0x4d0(a0)
	lbu	zero,0x4d1(a0)
	lbu	zero,0x4d2(a0)
	lbu	zero,0x4d3(a0)

	sb	a2,0x4d0(a0)
	sb	a1,0x4d1(a0)
	sb	a2,0x4d2(a0)
	sb	a1,0x4d3(a0)
	lbu	zero,0x4d0(a0)
	lbu	zero,0x4d1(a0)
	lbu	zero,0x4d2(a0)
	lbu	zero,0x4d3(a0)

	bal	_rom_consinit

	DBGPSTR("In _sbd_reset\r\n")
#endif

	move	ra,rasave

	j	ra

END(board_earlyinit)

LEAF(board_dram_init)

	move	rasave,ra

	li	bonito,PHYS_TO_K1(BONITO_BASE)

#ifdef P60XX_I2C
	/* 
	 * Now determine DRAM configuration and size by
	 * reading the I2C EEROM on the DIMMS
	 */

	bal	i2creset
	li	msize,0
	
	DISPLAY ('D','I','M','M','0',' ',' ',' ')
	
	/* start with DIMM #0 */
	li	a0,0
	/* Do a RMW on SDCFG to preserve power up values */
	lw	sdCfg,BONITO_SDCFG(bonito)
	/* use parity if DIMMS support it */
	or	sdCfg,BONITO_SDCFG_DRAMPARITY
	
.nextdimm:
	li	sdShape,0
	
	/* read DIMM memory type (must be SDRAM) */
	li	a1,2
	bal	i2cread
	bne	v0,4,.nodimm
	
	/* read DIMM memory size per side */
	li	a1,31
	bal	i2cread
	beqz	v0,.nodimm
	sll	tmpsize,v0,22		# multiply by 4M
	
	/* read DIMM number of rows */
	li	a1,3
	bal	i2cread
	subu	v0,11
	bgtu	v0,14-11,.nodimm
	sll	v0,BONITO_SDCFG_AROWBITS_SHIFT
	and	v0,BONITO_SDCFG_AROWBITS
	or	sdShape,v0
	
2:	/* read DIMM number of cols */
	li	a1,4
	bal	i2cread
	subu	v0,8
	bgtu	v0,11-8,.nodimm
	sll	v0,BONITO_SDCFG_ACOLBITS_SHIFT
	and	v0,BONITO_SDCFG_ACOLBITS
	or	sdShape,v0
	
2:	/* read DIMM number of blocks-per-dram */
	li	a1,17
	bal	i2cread
	beq	v0,2,2f
	bne	v0,4,.nodimm
	or	sdShape,BONITO_SDCFG_ABANKBIT
	
2:	/* read DIMM number of sides (banks) */
	li	a1,5
	bal	i2cread
	beq	v0,1,2f
	bne	v0,2,.nodimm
	or	sdShape,BONITO_SDCFG_ASIDES
	sll	tmpsize,1	# msize *= 2	
	
2:	/* read DIMM width */
	li	a1,6
	bal	i2cread
	bleu	v0,36,2f
	bgtu	v0,72,.nodimm
	or	sdShape,BONITO_SDCFG_AWIDTH64

2:	/* check width for parity operation */
	beq	v0,36,2f
	beq	v0,72,2f

	and	sdCfg,~BONITO_SDCFG_DRAMPARITY
	
2:	
	addu	msize,tmpsize
	b	2f
		
.nodimm:
	or	sdShape,BONITO_SDCFG_AABSENT
	li	v0,0xff<<BONITO_SDCFG_AROWBITS_SHIFT
2:	beqz	a0,1f
	sll	sdShape,BONITO_SDCFG_BROWBITS_SHIFT
	li	v0,0xff<<BONITO_SDCFG_BROWBITS_SHIFT
1:	not	v0
	and	sdCfg,v0
	or	sdCfg,sdShape
	
	DISPLAY ('D','I','M','M','1',' ',' ',' ')
	addu	a0,1
	bltu	a0,2,.nextdimm

	/* If we are running in SDRAM, chop 4MB off the memory size,
	   and don't modify sdCfg register (assume someone in 
	   PCI-world has already set it up). */
	lw	t0,BONITO_BONPONCFG(bonito)
	and	t0,BONITO_BONPONCFG_ROMBOOT
	bne	t0,BONITO_BONPONCFG_ROMBOOT_SDRAM,1f
	
	beqz	msize,2f	# already zero!
	subu	msize,4*1024*1024
	b	2f
	
1:	sw	sdCfg,BONITO_SDCFG(bonito)
	
2:		
#else	
	li	msize,4*0x100000
#endif
	
	
	
	li	t1,0		# accumulate pcimembasecfg settings
		
	/* set bar0 mask and translation to point to all memory */
	sub	t0,msize,1
	not	t0
	srl	t0,BONITO_PCIMEMBASECFG_ASHIFT-BONITO_PCIMEMBASECFG_MEMBASE0_MASK_SHIFT
	and	t0,BONITO_PCIMEMBASECFG_MEMBASE0_MASK
	or	t1,t0
	
	li	t0,0x00000000
	srl	t0,BONITO_PCIMEMBASECFG_ASHIFT-BONITO_PCIMEMBASECFG_MEMBASE0_TRANS_SHIFT
	and	t0,BONITO_PCIMEMBASECFG_MEMBASE0_TRANS
	or	t1,t0
	or	t1,BONITO_PCIMEMBASECFG_MEMBASE0_CACHED

	/* set bar1 to minimum size to conserve PCI space */
	li	t0, ~0
	srl	t0,BONITO_PCIMEMBASECFG_ASHIFT-BONITO_PCIMEMBASECFG_MEMBASE1_MASK_SHIFT
	and	t0,BONITO_PCIMEMBASECFG_MEMBASE1_MASK
	or	t1,t0
	
	li	t0,0x00000000
	srl	t0,BONITO_PCIMEMBASECFG_ASHIFT-BONITO_PCIMEMBASECFG_MEMBASE1_TRANS_SHIFT
	and	t0,BONITO_PCIMEMBASECFG_MEMBASE1_TRANS
	or	t1,t0
	or	t1,BONITO_PCIMEMBASECFG_MEMBASE1_CACHED

	sw	t1,BONITO_PCIMEMBASECFG(bonito)

	/* enable configuration cycles now */
	lw	t0,BONITO_BONPONCFG(bonito)
	and	t0,~BONITO_BONPONCFG_CONFIG_DIS
	sw	t0,BONITO_BONPONCFG(bonito)
			
#ifdef P60XX_PARITY	
	/* We have to clear memory to initialise parity */

	/* Skip memory clear if no memory */
	beqz	msize,.noclear
	
	/* Skip memory clear if non-parity memory */
	lw	t1,BONITO_SDCFG(bonito)
	and	t1,BONITO_SDCFG_DRAMPARITY
	beqz	t1,.noclear
	
	/* Skip memory clear if a soft reset */
	mfc0	t1,C0_STATUS
	and	t1,M_SR_SR
	bnez	t1,.noclear

	/* Clear bottom 256K running uncached */
	DISPLAY ('Z','2','5','6','K',' ',' ',' ')
	li	a0,PHYS_TO_K1(0)
	addu	a1,a0,256*1024
	.set noreorder
1:	sd	zero,0(a0)
	sd	zero,8(a0)
	sd	zero,16(a0)
	addu	a0,32
	bne	a0,a1,1b
	sd	zero,-8(a0)
	.set reorder
	
	/* We can now initialise the caches for a fast clear_mem */
//	DISPLAY ('C','A','C','H',' ',' ',' ',' ')
//	bal	mips_init_cache	
		
	/* Finally clear rest of memory running cached */
	li	a0,PHYS_TO_K1(256*1024)
	subu	a1,msize,256*1024
	blez	a1,.noclear
	addu	a1,a0
	
	/* clear all of memory (to set correct parity) */
	DISPLAY ('Z','M','E','M',' ',' ',' ',' ')
	
	
	.set noreorder
1:
	sd	zero,0(a0)
	sd	zero,8(a0)
	sd	zero,16(a0)
	sd	zero,24(a0)
	sd	zero,32(a0)
	sd	zero,40(a0)
	sd	zero,48(a0)
	addu	a0,64
	bne	a0,a1,1b
	sd	zero,-8(a0)
	.set reorder

.noclear:
		
#endif /* P60XX_PARITY */
		
	/* return to generic code, with available memory size */
	DISPLAY ('R','U','N',' ',' ',' ',' ',' ')
	move	ra,rasave

	move	v0,msize

	srl	v0,20			/* return in megabytes */

	j	ra
END(board_dram_init)


LEAF(_sbd_memfail)
	DISPLAY ('!','M','E','M','O','R','Y',' ')
1:	b	1b
	j	ra
END(_sbd_memfail)

#define SMBOFFS(reg) I82371_SMB_SMB##reg
			

SLEAF(i2creset)
	j	ra
SEND(i2creset)
	

/* i2cread (unsigned dev, unsigned offs) */
SLEAF(i2cread)	
	li	t0,PHYS_TO_K1(ISAPORT_BASE(SMB_PORT))

	lbu	t1,SMBOFFS(HSTSTS)(t0)
	and	t1,~(I82371_SMB_FAILED|I82371_SMB_BUS_ERR|I82371_SMB_DEV_ERR|I82371_SMB_INTER)
	sb	t1,SMBOFFS(HSTSTS)(t0)
	
	sll	t1,a0,1
	or	t1,0xa1		# DIMM base address and read bit
	sb	t1,SMBOFFS(HSTADD)(t0)
	sb	a1,SMBOFFS(HSTCMD)(t0)
	
	
	li	t1,I82371_SMB_START|I82371_SMB_BDRW
	sb	t1,SMBOFFS(HSTCNT)(t0)

	li	t3,10000	
1:	lbu	t1,SMBOFFS(HSTSTS)(t0)
	and	t2,t1,I82371_SMB_FAILED|I82371_SMB_BUS_ERR|I82371_SMB_DEV_ERR|I82371_SMB_INTER
	bnez	t2,1f
	sub	t3,1
	bnez	t3,1b
	b	9f		
1:	
	# clear pending errors/interrupts
	sb	t1,SMBOFFS(HSTSTS)(t0)
	
	and	t2,t1,I82371_SMB_FAILED|I82371_SMB_BUS_ERR|I82371_SMB_DEV_ERR
	bnez	t2,9f

	lbu	v0,SMBOFFS(HSTDAT0)(t0)
	j	ra
	
9:	li	v0,-1
	j	ra
SEND(i2cread)	
	


#include "reginit.s"		

	 .text
//	.rdata
//	.align 3
#define BONITO_INIT(r,v)	WR_INIT(W,BONITO_BASE+BONITO_##r,v)
#define BONITO_BIS(r,b)		RMW_INIT(W,BONITO_BASE+BONITO_##r,~0,b)
#define BONITO_BIC(r,b)		RMW_INIT(W,BONITO_BASE+BONITO_##r,~(b),0)
#define BONITO_RMW(r,c,s)	RMW_INIT(W,BONITO_BASE+BONITO_##r,~(c),s)
	
#define CFGADDR(idsel,function,reg) ((1<<(11+(idsel)))+((function)<<8)+(reg))
/* generic ISA Bridge (PIIX) configuration accesses */	
#define _ISABWR_INIT(mod,function,isabreg,val) \
	WR_INIT(W,BONITO_BASE+BONITO_PCIMAP_CFG,CFGADDR(PCI_DEV_I82371,function,isabreg)>>16) ; \
	RD_INIT(W,BONITO_BASE+BONITO_PCIMAP_CFG) ; \
	WR_INIT(mod,PCI_CFG_SPACE+(CFGADDR(PCI_DEV_I82371,function,isabreg)&0xffff),val)
#define _ISABRD_INIT(mod,function,isabreg) \
	WR_INIT(W,BONITO_BASE+BONITO_PCIMAP_CFG,CFGADDR(PCI_DEV_I82371,function,isabreg)>>16) ; \
	RD_INIT(W,BONITO_BASE+BONITO_PCIMAP_CFG) ; \
	RD_INIT(mod,PCI_CFG_SPACE+(CFGADDR(PCI_DEV_I82371,function,isabreg)&0xffff))

/* generic ISA I/O accesses */	
#define _ISAWR_INIT(isareg,val) \
	WR_INIT(B,PCI_IO_SPACE+(isareg),val)
#define _ISARD_INIT(isareg) \
	RD_INIT(B,PCI_IO_SPACE+(isareg))
	
/* ISA Bridge (PIIX) configuration accesses */
#ifdef __MIPSEB
/*
 * byte swapper disabled for config cycles
 * twiddle addresses but not data
 */	
#define ISABBWR_INIT(function,isabreg,val) \
	_ISABWR_INIT(B,function,(isabreg)^3,val)
#define ISABHWR_INIT(function,isabreg,val) \
	_ISABWR_INIT(H,function,(isabreg)^2,val)
#define ISABWWR_INIT(function,isabreg,val) \
	_ISABWR_INIT(W,function,isabreg,val)
#else
#define ISABBWR_INIT(function,isabreg,val) \
	_ISABWR_INIT(B,function,(isabreg),val)
#define ISABHWR_INIT(function,isabreg,val) \
	_ISABWR_INIT(H,function,(isabreg),val)
#define ISABWWR_INIT(function,isabreg,val) \
	_ISABWR_INIT(W,function,isabreg,val)
#endif
	
/* ISA I/O accesses */	
#define ISAWR_INIT(isareg,val) \
	_ISAWR_INIT(isareg,val)
#define ISARD_INIT(isareg) \
	_ISARD_INIT(isareg)
				
#define DISPLAY_INIT(d0,d1,d2,d3,d4,d5,d6,d7) \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(0), 0+d0); \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(1), 0+d1); \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(2), 0+d2); \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(3), 0+d3); \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(4), 0+d4); \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(5), 0+d5); \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(6), 0+d6); \
	WR_INIT(W, LED_BASE+HD2532_CRAM+HD2532_CHAR(7), 0+d7)

#define ISAREFRESH (PT_CRYSTAL/(1000000/15))

reginittab:
	/* bonito endianess */
#ifdef __MIPSEB
	BONITO_BIS(BONPONCFG,BONITO_BONPONCFG_CPUBIGEND)
	BONITO_BIS(BONGENCFG,BONITO_BONGENCFG_BYTESWAP|BONITO_BONGENCFG_MSTRBYTESWAP)
#else
	BONITO_BIC(BONPONCFG,BONITO_BONPONCFG_CPUBIGEND)
	BONITO_BIC(BONGENCFG,BONITO_BONGENCFG_BYTESWAP|BONITO_BONGENCFG_MSTRBYTESWAP)
#endif
	BONITO_BIS(BONPONCFG, BONITO_BONPONCFG_IS_ARBITER|BONITO_BONPONCFG_PCIRESET_OUT)
	
	/* Bonito PIO initialisation */

        BONITO_INIT(GPIODATA,PIO_PIIXRESET)	# initial value
	BONITO_INIT(GPIOIE,PIO_IE)
	
	/* Clear PCI reset and enable PIIX */
	DELAY_INIT(ROMMS(1))
	BONITO_BIC(BONPONCFG,BONITO_BONPONCFG_PCIRESET_OUT)
	DELAY_INIT(ROMMS(2))
	BONITO_BIC(GPIODATA,PIO_PIIXRESET)
	DELAY_INIT(ROMMS(50))
	
	/* PCI bus and PIIX should now be usable */
	
	BONITO_BIS(PCICMD, BONITO_PCICMD_PERRRESPEN)
	
	BONITO_BIS(PCICMD, PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE|PCI_COMMAND_MASTER_ENABLE)
	
	/* enable i/o buffer cache and other go faster bits */
	BONITO_BIS(BONGENCFG, \
			BONITO_BONGENCFG_BUSERREN| \
			BONITO_BONGENCFG_PREFETCHEN| \
			BONITO_BONGENCFG_WBEHINDEN| \
			BONITO_BONGENCFG_PCIQUEUE| \
			BONITO_BONGENCFG_SNOOPEN)

	/* Set debug mode */
	BONITO_BIS(BONGENCFG, BONITO_BONGENCFG_DEBUGMODE)

	BONITO_BIS(IODEVCFG, BONITO_IODEVCFG_BUFFBIT_CS0|\
			     BONITO_IODEVCFG_BUFFBIT_CS1|\
			     BONITO_IODEVCFG_SPEEDBIT_CS2|BONITO_IODEVCFG_BUFFBIT_CS2|\
			     BONITO_IODEVCFG_SPEEDBIT_CS3|BONITO_IODEVCFG_BUFFBIT_CS3|\
			     BONITO_IODEVCFG_BUFFBIT_IDE)
		
	/* switch on the LED */
	WR_INIT(W, LED_BASE+HD2532_CW,HD2532_CW_C)
	DELAY_INIT(ROMUS(110))
	
	/* 
	 * Initialise the ISA bridge via its CONF space
	 */
	
	DISPLAY_INIT ('I','s','a','B','r','i','d','g')
	
	/* Turn most special purpose pins into GPIO; set ISA mode */
	ISABWWR_INIT(0, I82371_GENCFG, I82371_GENCFG_CFG)
	
	/* disable RTC & KBD chip selects */
	ISABHWR_INIT(0, I82371_XBCS, 0)
	
	/* Enable PCI 2.1 timing support */
	ISABBWR_INIT(0, I82371_DLC, I82371_DLC_DT /* | I82371_DLC_PR */ | I82371_DLC_USBPR | I82371_DLC_DTTE)

	/* Set top of memory to 16MB, so all ISA bus master & DMA
	   accesses are forwarded to PCI mem space
	 */
	ISABBWR_INIT(0, I82371_TOM, I82371_TOM_TOM(16) | I82371_TOM_FWD_LBIOS | I82371_TOM_FWD_AB | I82371_TOM_FWD_89)
	
	/* disable the internal RTC */
	ISABBWR_INIT(0, I82371_RTCCFG, 0);	
	
	/* Set the SMB base address */
	ISABWWR_INIT(3, I82371_PCI3_SMBBA, SMB_PORT|PCI_MAP_IO)
	/* enable the host controller */
	ISABBWR_INIT(3, I82371_PCI3_SMBHSTCFG, I82371_PCI3_SMB_HST_EN)
	/* enable the SMB IO ports */
	ISABBWR_INIT(3, PCI_COMMAND_STATUS_REG, PCI_COMMAND_IO_ENABLE)

	/* Set the PIIX power management base address */
	ISABWWR_INIT(3, I82371_PCI3_PMBA, PM_PORT|PCI_MAP_IO)
	/* enable the power management ports */
	ISABBWR_INIT(3, I82371_PCI3_PMREGMISC, I82371_PCI3_PMIOSE)

	/* Initialise ISA bus low-level I/O devices */
	DISPLAY_INIT('I','s','a','D','e','v',' ',' ')

/* 15us ISA bus refresh clock */
#define ISAREFRESH (PT_CRYSTAL/(1000000/15))
	
	ISARD_INIT(CTC_PORT+PT_CONTROL)
	
	/* program i8254 ISA refresh counter */
	ISAWR_INIT(CTC_PORT+PT_CONTROL,PTCW_SC(PT_REFRESH)|PTCW_16B|PTCW_MODE(MODE_RG))
	ISAWR_INIT(CTC_PORT+PT_REFRESH, ISAREFRESH & 0xff)
	ISAWR_INIT(CTC_PORT+PT_REFRESH, ISAREFRESH >> 8)

	/* program ISA ICU */
	ISAWR_INIT(ICU1_PORT,  0x11) /* ICW1 */
	ISAWR_INIT(ICU1_PORT+1,0x00) /* ICW2: vector */
	ISAWR_INIT(ICU1_PORT+1,0x04) /* ICW3: cascade on IRQ2 */
	ISAWR_INIT(ICU1_PORT+1,0x01) /* ICW4: 8086 mode */
	ISAWR_INIT(ICU1_PORT+1,0xff) /* OCW1: mask all */

	ISAWR_INIT(ICU2_PORT,  0x11) /* ICW1 */
	ISAWR_INIT(ICU2_PORT+1,0x08) /* ICW2: vector */
	ISAWR_INIT(ICU2_PORT+1,0x02) /* ICW3:  */
	ISAWR_INIT(ICU2_PORT+1,0x01) /* ICW4: 8086 mode */
	ISAWR_INIT(ICU2_PORT+1,0xff) /* OCW1: mask all */
		
	ISAWR_INIT(ICU1_PORT+1,~(1<<2)) /* enable IRQ2 */

	/* set up ISA devices */

	/* select logical device 1 (mouse) */	
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(ISAPNP_MBDATA,1)
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(ISAPNP_MBDATA,1)
	
	/* select logical device 4 (parallel) */	
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(ISAPNP_MBDATA,4)
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_IO_DESC0+ISAPNP_IO_BASE_15_8)
	ISAWR_INIT(ISAPNP_MBDATA,(ECP_PORT>>8) & 0xff)
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_IO_DESC0+ISAPNP_IO_BASE_7_0)
	ISAWR_INIT(ISAPNP_MBDATA,ECP_PORT & 0xff)
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_IRQ_DESC0+ISAPNP_IRQ_CONTROL)
	ISAWR_INIT(ISAPNP_MBDATA,ISAPNP_IRQ_HIGH)
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(ISAPNP_MBDATA,1)
	
	/* select logical device 5 (COM2) */	
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(ISAPNP_MBDATA,5)
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(ISAPNP_MBDATA,1)
	
	/* select logical device 6 (COM1) */	
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(ISAPNP_MBDATA,6)
	ISAWR_INIT(ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(ISAPNP_MBDATA,1)
	
		
	/* Completed */
	DISPLAY_INIT('I','n','i','t','d','o','n','e')
		
	EXIT_INIT(0)
	

	
#define M_SR_DE _MM_MAKEMASK1(16)
	
LEAF(sbdberrenb)
	mfc0	v0,C0_SR
	li	t0,M_SR_DE	
	bnez	a0,1f
	or	t1,v0,t0	# disable cache/parity errors (SR_DE = 1)
	b	2f
1:	not	t1,t0		# enable cache/parity errors (SR_DE = 0)
	and	t1,v0
2:	mtc0	t1,C0_SR
	and	v0,t0		# get old SR_DE bit
	xor	v0,t0		# and invert to make it an enable bit
	j	ra
END(sbdberrenb)


LEAF(sbdberrcnt)
	move	v0,zero
	j	ra
END(sbdberrcnt)
	

//	.lcomm	wbfltmp,4

LEAF(wbflush)
	la	t0,K1BASE
	sync
	lw	zero,0(t0)
	j	ra
END(wbflush)

	
LEAF(sbddelay)
	mfc0	t0,C0_CONFIG
	li	t1,CACHEMISS+ROMCYCLE
	and	t0,CFG_K0MASK
	beq	t0,CFG_C_UNCACHED,1f
	and	t0,ra,0x20000000
	bnez	t0,1f
	li	t1,CACHECYCLE
1:	mul	a0,1000
	addu	a0,t1
	sll	t1,1
	divu	a0,t1
	subu	a0,48		# approx number of loops so far
	.set	noreorder	
	.set	nomacro
	nop
2:	bgtz	a0,2b
	subu	a0,1
	.set	macro
	.set	reorder
	j	ra
END(sbddelay)

LEAF(mips_cycle)
	.set	noreorder	
	.set	nomacro
1:	bgtz	a0,1b
	subu	a0,1
	.set	macro
	.set	reorder
	j	ra
END(mips_cycle)
	
