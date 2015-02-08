
	.global __main
	.global __rom_start

        .global _rambase
        .global _ramstart
	
	.global splash_bits
	.global _start
	.global _stext
	.global _edata

#define DEBUG
#define ROM_OFFSET 0x10C00000
#define STACK_GAURD 0x10

	.text
	
_start:
_stext:
	movew	#0x2700, %sr            /* Exceptions off! */


	movew	#16384, %d0  /* PLL settle wait loop */
L0:
	subw	#1, %d0
	bne	L0
#ifdef DEBUG
	moveq	#70, %d7		/* 'F' */
	moveb	%d7,0xfffff907          /* No absolute addresses */
pclp1:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	pclp1
#endif /* DEBUG */

#ifdef CONFIG_RELOCATE
	/* Copy me to RAM */
	moveal	#__rom_start, %a0
	moveal	#_stext, %a1
	moveal	#_edata, %a2

	/* Copy %a0 to %a1 until %a1 == %a2 */
LD1:
	movel	%a0@+, %d0
	movel	%d0, %a1@+
	cmpal	%a1, %a2
	bhi	LD1
	
#ifdef DEBUG
	moveq	#74, %d7		/* 'J' */
	moveb	%d7,0xfffff907          /* No absolute addresses */
pclp2:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	pclp2
#endif /* DEBUG */
	/* jump into the RAM copy */
	jmp     ram_jump
ram_jump:

#endif /* CONFIG_RELOCATE */

#ifdef DEBUG
	moveq	#82, %d7		/* 'R' */
	moveb	%d7,0xfffff907          /* No absolute addresses */
pclp3:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	pclp3
#endif /* DEBUG */
	moveal	#0x007ffff0, %ssp
	moveal	#_sbss, %a0
	moveal	#_ebss, %a1

	/* Copy 0 to %a0 until %a0 >= %a1 */
L1:
	movel	#0, %a0@+
	cmpal	%a0, %a1
	bhi	L1

#ifdef DEBUG
	moveq	#67, %d7                /* 'C' */
	jsr	putc
#endif /* DEBUG */

	pea	0
	pea	env
	pea	%sp@(4)
	pea	0

#ifdef DEBUG
	moveq	#70, %d7		/* 'F' */
	jsr	putc
#endif /* DEBUG */

lp:
	jsr	start_kernel
        jmp lp
_exit:

	jmp	_exit

__main:
	/* nothing */
	rts

#ifdef DEBUG
putc:
	moveb	%d7,0xfffff907
pclp:
	movew	0xfffff906, %d7
	andw	#0x2000, %d7
	beq	pclp
	rts
#endif /* DEBUG */

	.data

/*
 *      Set up the usable of RAM stuff. Size of RAM is determined then
 *      an initial stack set up at the end.
 */
.align 4
_ramvec:
.long   0
_rambase:
.long   0
_ramstart:
.long   0
_ramend:
.long   0

env:
	.long	0
