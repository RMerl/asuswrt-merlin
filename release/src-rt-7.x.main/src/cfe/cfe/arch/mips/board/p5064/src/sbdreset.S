/*
 * p5064/sbdreset.S: low level code for Algorithmics P5064 board
 * Copyright (c) 1997 Algorithmics Ltd.
 */

#include <sbmips.h>
#include "sbd.h"
#include "pio.h"
#include "z80pio.h"
#include "gpio.h"
#include "v96xpbc.h"
#include "i82371sb.h"
#include "i8254.h"
#include "rtc.h"
#include "pc97307.h"
#include "isapnpreg.h"
#include "bsp_config.h"

#include "mipsmacros.h"

#if !defined(__MIPSEB) && !defined(__MIPSEL)
#error "must define MIPSEB or MIPSEL"
#endif

#define CFG_BE		0x00008000	/* Big Endian */
#define MAXREVISION 3
#define NTLBENTRIES 64
	
#if defined(__MIPSEL)
#define V96X_SWAP_MEM	V96X_SWAP_NONE
#define V96X_SWAP_IO	V96X_SWAP_NONE
#elif defined(__MIPSEB)
#define V96X_SWAP_MEM	V96X_SWAP_8BIT
#define V96X_SWAP_IO	V96X_SWAP_AUTO
#endif
	
#if !__mips64 && __mips >= 3 && __mips != 32
	/* force 64-bit register support */
#undef __mips64	
#define __mips64 1	
	.set	gp64
#endif
	
#define DISPLAY(d0,d1,d2,d3)			\
	li	t8,PHYS_TO_K1(LED_BASE);	\
	li	t9,d0; 				\
	sw	t9,LED(0)(t8); 			\
	li	t9,d1; 				\
	sw	t9,LED(1)(t8);	 		\
	li	t9,d2;	 			\
	sw	t9,LED(2)(t8);	 		\
	li	t9,d3;	 			\
	sw	t9,LED(3)(t8)

#define MEG	0x100000		

/*
 * Data 
 */

#define R_R5K_INITCACHE		_TBLIDX(0)

boardinit_table:
	_LONG_	rm5200_l1cache_init		# [ 0] R_R5K_INITCACHE

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
#define	pio_base 	s1
#define	pio_val		s2
#define	crptr		s3
#define	crval		s4
#define msize		s5
#define	rasave		s6
#define revision	t7

LEAF(mips_kseg0_switch)

		and	ra,(K0SIZE-1)
		or	ra,K0BASE
		jr	ra

END(mips_kseg0_switch)

LEAF(mips_kseg1_switch)

		and	ra,(K0SIZE-1)
		or	ra,K1BASE
		jr	ra

END(mips_kseg1_switch)



LEAF(board_earlyinit)
	move	rasave,ra

	/*
	 * Determine board revision
	 */


	li	revision,PHYS_TO_K1(OPTION_BASE)
	lw	revision,0(revision)
	and	revision,OPTION_REV
	srl	revision,OPTION_REV_SHIFT
	bleu	revision,MAXREVISION,1f
	li	revision,MAXREVISION
1:		

	/* Initialise other low-level I/O devices */
	LOADREL(a0,reginittab)
	or	a0,K1BASE
	sll	v0,revision,2
	addu	a0,v0
	lw	a0,0(a0)
	or	a0,K1BASE

	bal	reginit


	move	ra,rasave
	j	ra

END(board_earlyinit)

LEAF(board_dram_init)

	move	rasave,ra

	/* 
	 * Now determine DRAM configuration and size by
	 * reading the I2C EEROM on the DIMMS
	 */

	bal	i2creset
	li	msize,0
	
	DISPLAY ('D','I','M','0')
	
	/* start with DIMM #0 */
	li	a0,0
	li	crval,DCR_SLOW_CAS|DCR_DIMM0
	
.nextdimm:
	/* read DIMM memory type (must be SDRAM) */
	li	a1,2
	bal	i2cread
	bne	v0,4,.nodimm
	
	/* read DIMM memory size per side */
	li	a1,31
	bal	i2cread
	li	tmpsize,4*1024*1024	# memory size unit
	mul	tmpsize,v0	
	
	/* read DIMM number of rows */
	li	a1,3
	bal	i2cread
	bne	v0,11,1f
	or	crval,DCR_ROWS_11	
	b	2f
1:	bne	v0,12,1f
	or	crval,DCR_ROWS_12	
	b	2f
1:	bne	v0,13,.nodimm
	or	crval,DCR_ROWS_13	
	b	2f
	
2:	/* read DIMM number of cols */
	li	a1,4
	bal	i2cread
	bne	v0,8,1f
	or	crval,DCR_COLS_8
	b	2f
1:	bne	v0,9,1f
	or	crval,DCR_COLS_9
	b	2f
1:	bne	v0,10,1f
	or	crval,DCR_COLS_10
	b	2f
1:	bne	v0,11,.nodimm
	or	crval,DCR_COLS_11
	b	2f
	
2:	/* read DIMM number of blocks-per-dram */
	li	a1,17
	bal	i2cread
	bne	v0,2,1f
	or	crval,DCR_BLOCKS_2
	b	2f
1:	bne	v0,4,.nodimm
	or	crval,DCR_BLOCKS_4
	b	2f
	
2:	/* read DIMM number of sides (banks) */
	li	a1,5
	bal	i2cread
	bne	v0,1,1f
	or	crval,DCR_SINGLE
	b	2f
1:	bne	v0,2,.nodimm
	b	2f
	
	/* msize *= sides (banks) */
2:	mul	tmpsize,v0	
	addu	msize,tmpsize
	
.nodimm:		
	/* write control register value */
/*	la	ta0,PHYS_TO_K1(DCR_BASE)*/
	li	ta0,PHYS_TO_K1(DCR_BASE)
	move	ta1,crval
	bal	crwrite
	
	DISPLAY ('D','I','M','1')
	li	crval,DCR_SLOW_CAS|DCR_DIMM1
	addu	a0,1
	bltu	a0,2,.nextdimm
	
	/* Panic if we've not got any memory! */
	bnez	msize,1f
	DISPLAY ('!','M','E','M')
2:	b	2b
	
	/* Skip the memory clear if this was a soft reset */
1:	mfc0	t1,C0_STATUS
	and	t1,M_SR_SR
	bnez	t1,noclear
	
	/* We have to clear memory to initialise parity */

	/* Clear bottom 64K running uncached */
	DISPLAY ('Z','6','4','K')
	li	a0,PHYS_TO_K1(0)
	addu	a1,a0,65536
	.set noreorder
1:	SR	zero,0(a0)
	SR	zero,REGSIZE(a0)
	SR	zero,(2*REGSIZE)(a0)
	addu	a0,REGSIZE*4
	bne	a0,a1,1b
	SR	zero,-REGSIZE(a0)
	.set reorder
	
	/* we can now initialise the caches for a fast clear_mem */
	DISPLAY ('C','A','C','H')
	CALLINIT_KSEG1(boardinit_table,R_R5K_INITCACHE)

	/* Finally clear rest of memory running cached */
	li	a0,PHYS_TO_K1(65536)
	subu	a1,msize,65536
	blez	a1,noclear
	addu	a1,a0
	
	/* clear all of memory (to set correct parity) */
	DISPLAY ('Z','M','E','M')

	/* run memory clear loop cached */
	bal	mips_kseg0_switch
	
	.set noreorder
1:	#cache	Create_Dirty_Exc_D,0(a0)
	SR	zero,0(a0)
	SR	zero,REGSIZE(a0)
	SR	zero,(REGSIZE*2)(a0)
	addu	a0,REGSIZE*4
	bne	a0,a1,1b
	SR	zero,-REGSIZE(a0)
	.set reorder

	/* revert to uncached */
	bal	mips_kseg1_switch
	
noclear:		
	/* store memory size */
	move	 k0,msize		/* K0 returns the memory size */
	srl	t0,msize,22	# in 4MB units
	sb	t0,PHYS_TO_K1(SOFTC_BASE + SOFTC_MEMSZ)

	li	t0,K1BASE
	sw	k0,0(t0)		/* store in 1st word of memory */

.noinit:

	move	v0,k0
	srl	v0,20			/* return in megabytes */

	move	ra,rasave
	j	ra
END(board_dram_init)


/* crwrite (base, val) 
 *	Write 8-bit <val> to 8 consecutive 1-bit registers, 
 *	starting at <base>.
 * Uses:	t8
 */
SLEAF(crwrite)
	li	t8,8
1:	.set	noat
	and	AT,ta1,1
	sll	AT,31
	sra	AT,31
	sw	AT,0(ta0)
	.set	at
	srl	ta1,1
	subu	t8,1
	addu	ta0,4
	bnez	t8,1b
	j	ra
SEND(crwrite)
		

/*
 * This is the code to read the I2C EEROM on the DIMMS
 */
	
#define EEABITS		8	/* 256x8 = 8 address bits */
#define PGSHIFT		7
#define PGMASK		(((0x1 << (EEABITS-8)) - 1) << 1)
#define PGNUM(o)	(((o) >> PGSHIFT) & PGMASK)
#define OMASK		0xff
#define OFFS(o)		((o) & OMASK)
#define READ		0x01
#define WRITE		0x00


#define	SET \
	bgeu	revision,2,97f; \
	sw	pio_val,ZPIO_B_DAT(pio_base); \
	b	98f; \
97:	sb	pio_val,GPIO2_DATA(pio_base); \
98:		
	
#define	BIS(x) \
	or	pio_val,x; SET

#define	BIC(x) \
	and	pio_val,~(x); SET
	
/*
 * Max chip frequency = 100 kHz, so we require at least 5 usec
 * delay between signal changes.
 */
#define WAIT \
	li	t9,ROMUS(6); \
	.set noreorder; \
10:	bnez	t9,10b; \
	subu	t9,1; \
	.set reorder
	
/* set SCL high */
#define SCL_HI \
	WAIT; BIS(PIO_I2C_SCL)
	
/* set SCL low */
#define SCL_LO \
	WAIT; BIC(PIO_I2C_SCL)
	
/* set SDA high */
#define SDA_HI \
	WAIT; BIS(PIO_I2C_SDA)
	
/* set SDA low */
#define SDA_LO \
	WAIT; BIC(PIO_I2C_SDA)
	
/* disable SDA output driver (tristate) */
#define SDI \
	WAIT; \
	and	pio_val,~PIO_I2C_DIR; \
	bgeu	revision,2,97f; \
	li	t9,ZPIO_MODE_CTRL; \
	sw	t9,ZPIO_B_CTL(pio_base); \
	li	t9,~PIO_OMASK; \
	sw	t9,ZPIO_B_CTL(pio_base); \
	sw	pio_val,ZPIO_B_DAT(pio_base); \
	b	98f; \
97:	li	t9,PIO_OMASK; \
	sb	t9,GPIO2_DIR(pio_base); \
	sb	pio_val,GPIO2_DATA(pio_base); \
98:			
	
/* enable SDA output driver (tristate) */
#define SDO \
	WAIT; \
	or	pio_val,PIO_I2C_DIR; \
	bgeu	revision,2,97f; \
	li	t9,ZPIO_MODE_CTRL; \
	sw	t9,ZPIO_B_CTL(pio_base); \
	li	t9,~(PIO_I2C_SDA | PIO_OMASK); \
	sw	t9,ZPIO_B_CTL(pio_base); \
	sw	pio_val,ZPIO_B_DAT(pio_base); \
	b	98f; \
97:	li	t9,PIO_I2C_SDA | PIO_OMASK; \
	sb	t9,GPIO2_DIR(pio_base); \
	sb	pio_val,GPIO2_DATA(pio_base); \
98:

/* read SDA */
#define SDRD \
	WAIT; \
	bgeu	revision,2,97f; \
	lw	v0,ZPIO_B_DAT(pio_base); \
	b	98f; \
97:	lb	v0,GPIO2_DATA(pio_base); \
98:	srl	v0, 7; \
	and	v0, 1
	
/* send a START: SDA high->low when SCL high */
#define START \
	SDO;	\
	SDA_HI; \
	SCL_HI; \
	SDA_LO; \
	SCL_LO

/* send a STOP: SDA low->high when SCL high */
#define STOP \
	SDO;	\
	SDA_LO; \
	SCL_HI; \
	SDA_HI; \
	SCL_LO; \
	SDA_LO; \
	SDI

/* receive an ACK: a single 0 bit */
#define GETACK \
	SCL_HI; \
	SDRD;   \
	SCL_LO; \
	xor v0,1

/* send an ACK: a single 0 bit */
#define SENDACK \
	SDO;	\
	SDA_LO; \
	SCL_HI; \
	SCL_LO; \
	SDI
	
/* send a NACK: a single 1 bit */
#define SENDNACK \
	SDO;	\
	SDA_HI; \
	SCL_HI; \
	SCL_LO; \
	SDA_LO; \
	SDI

/* send 8 bit word in ta0 (note: check for ACK externally) */
SLEAF(i2csend8)
	li	t0,8
	SDO
1:	and	t1,ta0,0x80
	beqz	t1,2f
	SDA_HI
	b	3f
2:	SDA_LO
3:	SCL_HI
	SCL_LO
	sll	ta0,1
	subu	t0,1
	bnez	t0,1b
	SDA_LO
	SDI
	j	ra
SEND(i2csend8)


/* receive 8 bit word into v0 (note: send ACK/NACK externally) */
SLEAF(i2cget8)
	li	t1,0
	li	t0,7
1:	SCL_HI
	SDRD
	SCL_LO
	sll	v0,t0
	or	t1,v0
	subu	t0,1
	bgez	t0,1b
	move	v0,t1
	j	ra
SEND(i2cget8)
	

/* DADDR(rw, a0=dev, a1=addr)
	send first command byte: device address & page number */
	
#define DADDR(rw)						\
	START;							\
	sll t0,a0,1; 		/* t0 = dev << 1 */		\
	srl ta0,a1,PGSHIFT;	/* ta0 = (addr >> PGSHIFT) */	\
	and ta0,PGMASK;		/* ta0 &= PGMASK */		\
	or ta0,t0; 		/* ta0 |= (dev << 1) */		\
	or ta0,(0xa0 | rw);	/* ta0 |= (0xa0 | rw) */	\
	bal i2csend8;						\
	GETACK

/* ADDR(rw, a0=dev, a1=addr)
	send two command bytes */
	
#define ADDR(rw)						\
	/* timeout for previous write to complete */		\
	li t3,1000; 						\
								\
	/* send first byte: device address */ 			\
1:	DADDR(rw); 						\
	bnez v0, 2f; 						\
	/* no acknowledge - count down */ 			\
	subu t3,1; 						\
	beqz t3,99f; 						\
	b 1b; 							\
								\
2:	/* send second byte: word offset */ 			\
	and ta0,a1,OMASK;	/* offset in page */ 		\
	bal i2csend8;						\
	GETACK;							\
99:

	
SLEAF(i2creset)
	/* initialise global registers */

	bgeu	revision,2,97f
	
	li	pio_base,PHYS_TO_K1(ZPIO_BASE)
	b	98f

97:	li	pio_base,PHYS_TO_K1(ISAPORT_BASE(GPIO_PORT))
	
98:	li	pio_val,0
	
        /* send 9 STOPS */
	li	t0,9
1:	STOP
	subu	t0,1
	bnez	t0,1b
	
	j	ra
SEND(i2creset)
	

/* i2cread (unsigned dev, unsigned offs) */
SLEAF(i2cread)	
	move	t8,ra
	
	ADDR(WRITE)		# write address
	beqz	v0,9f		# ack?
	
	DADDR(READ)		# start read
	beqz	v0,9f		# ack?
	
	bal	i2cget8		# get data byte
	
	SENDNACK		# terminate read
	STOP
	
	j	t8
	
9:	li	v0,-1
	j	t8
SEND(i2cread)	
	


	.lcomm	wbfltmp,4

LEAF(wbflush)
	sync
	la	t0,wbfltmp
	or	t0,K1BASE
	lw	zero,0(t0)
	j	ra
END(wbflush)


LEAF(sbddelay)
	mfc0	t0,C0_CONFIG
	li	t1,CACHEMISS+ROMCYCLE
	and	t0,M_CFG_K0COH
	beq	t0,V_CFG_K0COH(K_CFG_K0COH_UNCACHED),1f
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



#define MOD_B		0x00000000 /* byte "modifier" */
#define MOD_H		0x00000001 /* halfword "modifier" */
#define MOD_W		0x00000002 /* word "modifier" */
#define MOD_D		0x00000003 /* doubleword "modifier" */
#define MOD_MASK	0x00000003

#define OP_MASK		0x000000fc
#define	OP_EXIT		0x00000000 /* exit(status) */
#define OP_DELAY	0x00000008 /* delay(cycles) */
#define OP_RD		0x00000010 /* read (addr)) */
#define OP_WR		0x00000014 /* write (addr, val) */
#define OP_RMW		0x00000018 /* read-modify-write (addr, and, or) */
#define OP_WAIT		0x00000020 /* poll (addr, mask, value) */

#define Init_Op	0
#define Init_A0 4
#define Init_A1 8
#define Init_A2 12
#define Init_Size 16
	


SLEAF(reginit)
	lw	t3, Init_Op(a0)
	lw	t0, Init_A0(a0)
	and	t4, t3,OP_MASK
	
	bne	t4, OP_EXIT, 8f
	/* exit(status) */
	move	v0,t0
	b	9f

8:	bne	t4, OP_DELAY, 8f
	/* delay(cycles) */
	.set noreorder
1:	bnez	t0,1b
	subu	t0,1
	.set reorder
	addu	a0,Init_Size
	b	reginit
	
8:	bne	t4,OP_RD,8f
	/* read(addr) */
	and	t4,t3,MOD_MASK
	bnez	t4,1f
	lbu	zero,0(t0)
	b	2f
1:	subu	t4,1
	bnez	t4,1f
	lhu	zero,0(t0)
	b	2f
1:	subu	t4,1
	bnez	t4,1f
	lw	zero,0(t0)
#if __mips64
	b	2f
1:	ld	zero,0(t0)
#endif
2:	addu	a0,Init_Size
	b	reginit
	
8:	bne	t4,OP_WR,8f
	/* write(addr,val) */
	lw	t1,Init_A1(a0)
	and	t4,t3,MOD_MASK
	bnez	t4,1f
	sb	t1,0(t0)
	b	2f
1:	subu	t4,1
	bnez	t4,1f
	sh	t1,0(t0)
	b	2f
1:	subu	t4,1
	bnez	t4,1f
	sw	t1,0(t0)
#if __mips64
	b	2f
1:	sd	t1,0(t0)
#else
1:			
#endif
2:	addu	a0,Init_Size
	b	reginit
	
8:	bne	t4,OP_RMW,8f
	/* write(addr,val) */
	lw	t1,Init_A1(a0)
	lw	t2,Init_A2(a0)
	and	t4,t3,MOD_MASK
	bnez	t4,1f
	lbu	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sb	t4,0(t0)
	b	2f
1:	subu	t4,1
	bnez	t4,1f
	lhu	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sh	t4,0(t0)
	b	2f
1:	subu	t4,1
	bnez	t4,1f
	lw	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sw	t4,0(t0)
#if __mips64
	b	2f
1:	ld	t4,0(t0)
	and	t4,t1
	or	t4,t2
	sd	t4,0(t0)
#else
1:			
#endif
2:	addu	a0,Init_Size
	b	reginit
	
8:	li	v0,-1		
9:	j	ra	
	
SEND(reginit)

#define WR_INIT(mod,addr,val) \
	.word	OP_WR|MOD_##mod,PHYS_TO_K1(addr),(val),0
#define DELAY_INIT(cycles) \
	.word	OP_DELAY,(cycles),0,0
#define EXIT_INIT(status) \
	.word	OP_EXIT,(status),0,0
	
#define V96XWR_INIT(mod,v96xreg,val) \
	WR_INIT(mod,V96XPBC_BASE+(v96xreg),val)

#define ISABWR_INIT(mod,isabreg,val) \
	WR_INIT(mod,PCI_CONF_SPACE+(isabreg),val)
	
#define ISAWR_INIT(mod,isareg,val) \
	WR_INIT(mod,PCI_IO_SPACE+(isareg),val)
	
#define DISPLAY_INIT(a,b,c,d) \
	WR_INIT(W, LED_BASE+LED(0), 0+a); \
	WR_INIT(W, LED_BASE+LED(1), 0+b); \
	WR_INIT(W, LED_BASE+LED(2), 0+c); \
	WR_INIT(W, LED_BASE+LED(3), 0+d)

/*	.rdata*/		    /* this RO data can go in text section */
	.text
	.globl reginittab
	
reginittab:
	.word	reginittabRevA	
	.word	reginittabRevB
	.word	reginittabRevC
	.word	reginittabRevD

/* 15us ISA bus refresh clock */
#define ISAREFRESH (PT_CRYSTAL/(1000000/15))
	
reginittabRevA:	
reginittabRevB:	
	/* Set i/o system endianness first with a 64-bit write */
#if __mips64	
#ifdef __MIPSEB
	WR_INIT(D, BCR0_BASE + BCR0_ENDIAN, BCR0_ENDIAN_BE)
#else
	WR_INIT(D, BCR0_BASE + BCR0_ENDIAN, BCR0_ENDIAN_LE)
#endif	
#else
#ifdef __MIPSEB
	WR_INIT(W, BCR0_BASE + BCR0_ENDIAN + 4, BCR0_ENDIAN_BE)
#else
	WR_INIT(W, BCR0_BASE + BCR0_ENDIAN + 0, BCR0_ENDIAN_LE)
#endif	
#endif		
	/* switch on the LED */
	WR_INIT(W, BCR0_BASE + BCR0_LED, BCR0_LED_ON)
	
	/* first ever led message */
	DISPLAY_INIT('P','5','6','4')
	
	/* toggle the V3 reset */
	WR_INIT(W, BCR1_BASE + BCR1_V96X, BCR1_ENABLE)
	WR_INIT(W, BCR1_BASE + BCR1_V96X, BCR1_RESET)
	WR_INIT(W, BCR1_BASE + BCR1_V96X, BCR1_ENABLE)
	
	/* initialise the z80pio chip, B channel */
	WR_INIT(W, ZPIO_BASE + ZPIO_B_CTL, ZPIO_MODE_CTRL)	# bidir control mode
	WR_INIT(W, ZPIO_BASE + ZPIO_B_CTL, ~PIO_OMASK)		# ip msk = ~op mask
	WR_INIT(W, ZPIO_BASE + ZPIO_B_DAT, 0)			# initial op value
	
	/* enable the ISA bridge */
	WR_INIT(W, BCR1_BASE + BCR1_ISA, BCR1_ENABLE)
	
	/* enable the PCMCIA controller */
	WR_INIT(W, BCR1_BASE + BCR1_PCMCIA, BCR1_ENABLE)
		
	/* > 18.5ms delay while V3 attempts to read EEROM... */
	DELAY_INIT(ROMMS(20))
	
	/*
	 * Setup a bare minimum to allow us to get at the i/o
	 * registers on the Intel PCI->ISA bridge, ISA bus
	 * and XBUS.
	 */
	DISPLAY_INIT('V','9','6','X')
	
	/* initial magic cycle for PCI bridge */
	V96XWR_INIT(W, V96X_LB_IO_BASE, V96XPBC_BASE+V96X_LB_IO_BASE)
	
	/* Local bus to PCI aptr 2 - LOCAL:PCI_IO_SPACE -> PCI:00000000 */
	V96XWR_INIT(H, V96X_LB_BASE2, \
		(PCI_IO_SPACE >> 16)|(V96X_SWAP_IO >> 2)|V96X_LB_BASEx_ENABLE)
	V96XWR_INIT(H, V96X_LB_MAP2, 0)
	
        /* Local to PCI aptr 0 - LOCAL:PCI_CONF_SPACE -> PCI:config (1MB) */
	V96XWR_INIT(W, V96X_LB_BASE0, \
		PCI_CONF_SPACE | V96X_SWAP_IO | V96X_ADR_SIZE_1MB | V96X_LB_BASEx_ENABLE)

	/* high 12 bits of conf space address go in map reg */
	V96XWR_INIT(H, V96X_LB_MAP0, \
		((1 << (8+PCI_IDSEL_I82371)) & V96X_LB_MAPx_MAP_ADR) | V96X_LB_TYPE_CONF)

        /* Enable PCI bus master access */
	V96XWR_INIT(H, V96X_PCI_CMD, V96X_PCI_CMD_MASTER_EN)

	/* Unreset the PCI bus */
	V96XWR_INIT(H, V96X_SYSTEM, V96X_SYSTEM_RST_OUT);

	/* 2ms reset delay */
	DELAY_INIT(ROMMS(2))

	/* 
	 * Initialise the ISA bridge via its CONF space
	 */
	
	DISPLAY_INIT ('I','S','A','B')
	
	/* Enable PCI 2.1 timing support */
	ISABWR_INIT(B, I82371_DLC, \
			I82371_DLC_DT | I82371_DLC_PR | I82371_DLC_USBPR | I82371_DLC_DTTE)

	/* Enable USB function, and force SYSCLK=PCICLK/4 */
	ISABWR_INIT(H, I82371_MSTAT, I82371_MSTAT_USBE|I82371_MSTAT_ISADIV_4);
	
	/* Programmable decode for Centronics input latch */
	ISABWR_INIT(H, I82371_PCSC, CEN_LATCH_PORT|I82371_PCSC_SIZE_4)

	/* Set top of memory to 16MB, so all ISA bus master & DMA
	   accesses are forwarded to PCI mem space, except for the
	   hole from 640K (A0000) to 1MB, which is confined to ISA */
	ISAWR_INIT (B, I82371_TOM, I82371_TOM_TOM(16) | I82371_TOM_FWD_89)
	
	/* Initialise ISA bus low-level I/O devices */
	DISPLAY_INIT('I','S','A','R')

	/* program i8254 ISA refresh counter */
	ISAWR_INIT(B,CTC_PORT+PT_CONTROL, \
		PTCW_SC(PT_REFRESH)|PTCW_16B|PTCW_MODE(MODE_RG))
	ISAWR_INIT(B,CTC_PORT+PT_REFRESH, ISAREFRESH & 0xff)
	ISAWR_INIT(B,CTC_PORT+PT_REFRESH, ISAREFRESH >> 8)
	
	/* setup 32kHz refresh for our DRAM */
	ISAWR_INIT(B,RTC_ADDR_PORT, RTC_STATUSA)
	 ISAWR_INIT(B,RTC_DATA_PORT, RTC_OSC_32KHz)
	ISAWR_INIT(B,RTC_ADDR_PORT, RTC_STATUSB)
	 ISAWR_INIT(B,RTC_DATA_PORT, RTCSB_SQWE)
	ISAWR_INIT(B,RTC_ADDR_PORT, RTC_INTR)			
	 ISAWR_INIT(B,RTC_DATA_PORT, RTCIR_32KE)

	/* Completed */
	DISPLAY_INIT('d','o','n','e')
		
	EXIT_INIT(0)
	

reginittabRevC:	
reginittabRevD:	
	/* Set i/o system endianness first with a 64-bit write */
#ifdef __MIPSEB
	WR_INIT(D, BCR0_BASE + BCR0_ENDIAN, BCR0_ENDIAN_BE)
#else
	WR_INIT(D, BCR0_BASE + BCR0_ENDIAN, BCR0_ENDIAN_LE)
#endif	
	
	/* switch on the LED */
	WR_INIT(W, BCR0_BASE + BCR0_LED, BCR0_LED_ON)
	
	/* first ever led message */
	DISPLAY_INIT('P','5','6','4')
	
	/* toggle the V3 reset */
	WR_INIT(W, BCR1_BASE + BCR1_V96X, BCR1_ENABLE)
	WR_INIT(W, BCR1_BASE + BCR1_V96X, BCR1_RESET)
	WR_INIT(W, BCR1_BASE + BCR1_V96X, BCR1_ENABLE)
	
	/* enable the ISA bridge */
	WR_INIT(W, BCR1_BASE + BCR1_ISA, BCR1_ENABLE)
	
	/* enable the PCMCIA controller */
	WR_INIT(W, BCR1_BASE + BCR1_PCMCIA, BCR1_ENABLE)
		
	/* > 18.5ms delay while V3 attempts to read EEROM... */
	DELAY_INIT(ROMMS(20))
	
	/*
	 * Setup a bare minimum to allow us to get at the i/o
	 * registers on the Intel PCI->ISA bridge, ISA bus
	 * and XBUS.
	 */
	DISPLAY_INIT('V','9','6','X')
	
	/* initial magic cycle for PCI bridge */
	V96XWR_INIT(W, V96X_LB_IO_BASE, V96XPBC_BASE+V96X_LB_IO_BASE)
	
	/* Local bus to PCI aptr 2 - LOCAL:PCI_IO_SPACE -> PCI:00000000 */
	V96XWR_INIT(H, V96X_LB_BASE2, \
		(PCI_IO_SPACE >> 16)|(V96X_SWAP_IO >> 2)|V96X_LB_BASEx_ENABLE)
	V96XWR_INIT(H, V96X_LB_MAP2, 0)
	
        /* Local to PCI aptr 0 - LOCAL:PCI_CONF_SPACE -> PCI:config (1MB) */
	V96XWR_INIT(W, V96X_LB_BASE0, \
		PCI_CONF_SPACE | V96X_SWAP_IO | V96X_ADR_SIZE_1MB | V96X_LB_BASEx_ENABLE)

	/* high 12 bits of conf space address go in map reg */
	V96XWR_INIT(H, V96X_LB_MAP0, \
		((1 << (8+PCI_IDSEL_I82371)) & V96X_LB_MAPx_MAP_ADR) | V96X_LB_TYPE_CONF)

        /* Enable PCI bus master access */
	V96XWR_INIT(H, V96X_PCI_CMD, V96X_PCI_CMD_MASTER_EN)

	/* Unreset the PCI bus */
	V96XWR_INIT(H, V96X_SYSTEM, V96X_SYSTEM_RST_OUT);

	/* 2ms reset delay */
	DELAY_INIT(ROMMS(2))

	/* 
	 * Initialise the ISA bridge via its CONF space
	 */
	
	DISPLAY_INIT ('I','S','A','B')
	
	/* Enable PCI 2.1 timing support */
	ISABWR_INIT(B, I82371_DLC, \
			I82371_DLC_DT | I82371_DLC_PR | I82371_DLC_USBPR | I82371_DLC_DTTE)

	/* Enable USB function, and force SYSCLK=PCICLK/4 */
	ISABWR_INIT(H, I82371_MSTAT, I82371_MSTAT_USBE|I82371_MSTAT_ISADIV_4);
	
	/* Programmable decode for Centronics input latch */
	ISABWR_INIT(H, I82371_PCSC, CEN_LATCH_PORT|I82371_PCSC_SIZE_4)

	/* Set top of memory to 16MB, so all ISA bus master & DMA
	   accesses are forwarded to PCI mem space, except for the
	   hole from 640K (A0000) to 1MB, which is confined to ISA */
	ISAWR_INIT (B, I82371_TOM, I82371_TOM_TOM(16) | I82371_TOM_FWD_89 | I82371_TOM_FWD_AB)

	/* Initialise ISA bus low-level I/O devices */
	DISPLAY_INIT('I','S','A','R')

	/* program i8254 ISA refresh counter */
	ISAWR_INIT(B,CTC_PORT+PT_CONTROL, \
		PTCW_SC(PT_REFRESH)|PTCW_16B|PTCW_MODE(MODE_RG))
	ISAWR_INIT(B,CTC_PORT+PT_REFRESH, ISAREFRESH & 0xff)
	ISAWR_INIT(B,CTC_PORT+PT_REFRESH, ISAREFRESH >> 8)
	
	/* set up ISA devices */

	/* select logical device 0 (keyboard) */	
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(B,ISAPNP_MBDATA,0)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(B,ISAPNP_MBDATA,1)

	/* select logical device 1 (mouse) */	
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(B,ISAPNP_MBDATA,1)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(B,ISAPNP_MBDATA,1)
	
	/* select logical device 4 (parallel) */	
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(B,ISAPNP_MBDATA,4)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_IO_DESC0+ISAPNP_IO_BASE_15_8)
	ISAWR_INIT(B,ISAPNP_MBDATA,(ECP_PORT>>8) & 0xff)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_IO_DESC0+ISAPNP_IO_BASE_7_0)
	ISAWR_INIT(B,ISAPNP_MBDATA,ECP_PORT & 0xff)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_IRQ_DESC0+ISAPNP_IRQ_CONTROL)
	ISAWR_INIT(B,ISAPNP_MBDATA,ISAPNP_IRQ_HIGH)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(B,ISAPNP_MBDATA,1)
	
	/* select logical device 5 (COM2) */	
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(B,ISAPNP_MBDATA,5)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(B,ISAPNP_MBDATA,1)
	
	/* select logical device 6 (COM1) */	
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(B,ISAPNP_MBDATA,6)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(B,ISAPNP_MBDATA,1)
	
	/* select logical device 7 (PIO) */	
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_LOGICAL_DEV_NUM)
	ISAWR_INIT(B,ISAPNP_MBDATA,7)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_IO_DESC0+ISAPNP_IO_BASE_15_8)
	ISAWR_INIT(B,ISAPNP_MBDATA,(GPIO_PORT>>8) & 0xff)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_IO_DESC0+ISAPNP_IO_BASE_7_0)
	ISAWR_INIT(B,ISAPNP_MBDATA,GPIO_PORT & 0xff)
	ISAWR_INIT(B,ISAPNP_MBADDR,ISAPNP_ACTIVATE)
	ISAWR_INIT(B,ISAPNP_MBDATA,1)
	
	/* set default configuration for GPIO2 */
	ISAWR_INIT(B,GPIO_PORT+GPIO2_OTYPE, 0xff)
	ISAWR_INIT(B,GPIO_PORT+GPIO2_PULLUP, 0)
	ISAWR_INIT(B,GPIO_PORT+GPIO2_DIR, GPIO2_OMASK)
	ISAWR_INIT(B,GPIO_PORT+GPIO2_DATA, 0)
	
	/* Completed */
	DISPLAY_INIT('d','o','n','e')
		
	EXIT_INIT(0)
	
	
