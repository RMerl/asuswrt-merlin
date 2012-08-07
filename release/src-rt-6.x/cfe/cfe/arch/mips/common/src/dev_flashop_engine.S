/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Flash "self-write" module	File: dev_flashop_engine.S
    *  
    *  This module takes care of the case of writing to the flash
    *  memory that CFE is currently reading its code from.
    *
    *  Note: this code is written to be position-independent, even
    *  for non-PIC versions of CFE!  It will be copied (with memcpy)
    *  into the heap for execution.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include "sbmips.h"
#include "mipsmacros.h"

#include "bsp_config.h"
#include "dev_newflash.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#if defined(__MIPSEB) && defined(_MIPSEB_DATA_INVARIANT_)

#ifdef	BCMHND74K

#define FLASHCMD_8(base,offset,value) \
	li	t0,value 	; \
	ADD	base,((offset)<<1) ; \
	xori	base,7 		; \
	sb	t0,0(base) 	; \
	xori	base,7 		; \
	SUB	base,((offset)<<1)

#define FLASHCMD_16(base,offset,value) \
	li	t0,value 	; \
	ADD	base,((offset)<<1) ; \
	xori	base,6 		; \
	sh	t0,0(base) 	; \
	xori	base,6 		; \
	SUB	base,((offset)<<1)

#define FLASHCMD_16B(base,offset,value) \
	li	t0,value 	; \
	ADD	base,((offset)<<1) ; \
	xori	base,7 		; \
	sb	t0,0(base) 	; \
	xori	base,7 		; \
	SUB	base,((offset)<<1)

#else	/* Not 74K, bcm33xx */

#define FLASHCMD_8(base,offset,value) \
	li	t0,value 	; \
	ADD	base,((offset)<<1) ; \
	xori	base,3 		; \
	sb	t0,0(base) 	; \
	xori	base,3 		; \
	SUB	base,((offset)<<1)

#define FLASHCMD_16(base,offset,value) \
	li	t0,value 	; \
	ADD	base,((offset)<<1) ; \
	xori	base,2 		; \
	sh	t0,0(base) 	; \
	xori	base,2 		; \
	SUB	base,((offset)<<1)

#define FLASHCMD_16B(base,offset,value) \
	li	t0,value 	; \
	ADD	base,((offset)<<1) ; \
	xori	base,3 		; \
	sb	t0,0(base) 	; \
	xori	base,3 		; \
	SUB	base,((offset)<<1)

#endif	/* BCMHND74K */

#else
	
#define FLASHCMD_8(base,offset,value) \
	li	t0,value ;	      \
	sb	t0,offset(base)

#define FLASHCMD_16(base,offset,value) \
	li	t0,value ;	      \
	sh	t0,((offset)<<1)(base)

#define FLASHCMD_16B(base,offset,value) \
	li	t0,value ;	      \
	sb	t0,((offset)<<1)(base)

#endif

/*  *********************************************************************
    *  flashop_engine
    *  
    *  This routine is written in a PIC method to allow us to do
    *  flash operations without any help from CFE.  We need to do
    *  this when we're not relocated and want to muck with the
    *  flash we're running from.
    *  
    *  This routine follows some simple instructions in a table,
    *  so you can batch up the operations in one place.
    *  
    *  Input parameters: 
    *  	   a0 - pointer to instruction list
    *  	   
    *  Return value:
    *  	   v0 - 0 if all instructions succeeded
    *  	   else less than zero, # of failing instructions
    ********************************************************************* */

                .text

#define reg_op		  t3
#define reg_base	  t4
#define reg_dest	  t5
#define reg_src		  t6
#define reg_cnt		  t7

LEAF(flashop_engine)

instloop:	LR	reg_op,FEINST_OP(a0)	   /* Load instruction */
		LR	reg_base,FEINST_BASE(a0)
		LR	reg_dest,FEINST_DEST(a0)
		LR	reg_src,FEINST_SRC(a0)
		LR	reg_cnt,FEINST_CNT(a0)
		li	v0,0			   /* total of result values */
		li	v1,0			   /* result for this function */

#ifdef __long64
		dli	t0,0x9000000000000000	   /* uncached - XKPHYS */
		or	reg_base,t0		   /* so we can access flash beyond KSEG */
#else
		or	reg_base,K1BASE		   /* 32-bit, regular KSEG */
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		bne	reg_op,FEOP_RETURN,99f	   /* Return to main prog */

		j	ra
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_REBOOT,99f	   /* restart system  */

		li	t0,0xBFC00000		   /* jump to boot vector */
		j	t0
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_READ8,99f	   /* Read, 8-bit mode */

		ADD	reg_src,reg_src,reg_base

1:		lbu	t0,0(reg_src)		   /* Copy user data */
		sb	t0,0(reg_dest)
		ADD	reg_src,1
		add	reg_dest,1
		sub	reg_cnt,1
		bgt	reg_cnt,zero,1b

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_READ16,99f	   /* Read, 16-bit mode */

		ADD	reg_src,reg_src,reg_base

		li	t0,1			   /* test bottom bit */
		and	t0,t0,reg_src		   /* t0 == 1 if odd */
		beq	t0,zero,1f		   /* no odd byte to worry about */

		SUB	reg_src,reg_src,t0	   /* make even value */
		lh	t0,0(reg_src)		   /* interesting byte is odd */
#ifdef __MIPSEB
		sb	t0,0(reg_dest)		   /* interesting byte in low 8 bits */
#else
		srl	t0,t0,8			   /* little endian */
		sb	t0,0(reg_dest)		   /* interesting byte is high 8 bits */
#endif

		ADD	reg_src,2		   /* advance one word (we made addr even above) */
		add	reg_dest,1		   /* dest always written by bytes */
		sub	reg_cnt,1

1:		beq	reg_cnt,zero,nextinst

		lh	t0,0(reg_src)		   /* Copy user data */

#ifdef __MIPSEB
		sb	t0,1(reg_dest)		   /* Big endian to memory */
		srl	t0,t0,8			   /* t0 = 0x1234 -> 0x12 0x34 */
		sb	t0,0(reg_dest)
#else
		sb	t0,0(reg_dest)		   /* little endian */
		srl	t0,t0,8			   /* t0 = 0x1234 -> 0x34 0x12 */
		sb	t0,1(reg_dest)
#endif

		ADD	reg_src,2
		add	reg_dest,2
		sub	reg_cnt,2
		bgt	reg_cnt,1,1b

		beq	reg_cnt,zero,nextinst	   /* no straggler */

		lh	t0,0(reg_src)		   /* interesting byte is odd */
#ifdef __MIPSEB
		srl	t0,t0,8			   /* little endian */
		sb	t0,0(reg_dest)		   /* interesting byte in high 8 bits */
#else
		sb	t0,0(reg_dest)		   /* interesting byte is low 8 bits */
#endif

		b	nextinst
/* CFI - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if (FLASH_DRIVERS & FLASH_DRIVER_CFI)
99:		bne	reg_op,FEOP_CFIQUERY8,99f   /* CFI Query 8-bit */

		ADD	reg_src,reg_src,reg_base

		FLASHCMD_8(reg_base,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_MODE)

1:		lbu	t0,0(reg_src)		   /* Copy CFI data */
		sb	t0,0(reg_dest)
		ADD	reg_src,1
		add	reg_dest,1
		sub	reg_cnt,1
		bgt	reg_cnt,zero,1b

		FLASHCMD_8(reg_base,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_EXIT)

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_CFIQUERY16,99f   /* CFI Query 16-bit in word mode */

		ADD	reg_src,reg_src,reg_base

		FLASHCMD_16(reg_base,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_MODE)

1:		lh	t0,0(reg_src)		   /* Copy CFI data */
		sb	t0,0(reg_dest)
		ADD	reg_src,2
		add	reg_dest,2
		sub	reg_cnt,2
		bgt	reg_cnt,zero,1b

		FLASHCMD_16(reg_base,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_EXIT)

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_CFIQUERY16B,99f   /* CFI Query 16-bit in byte mode */

		ADD	reg_src,reg_src,reg_base

		FLASHCMD_16B(reg_base,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_MODE)

1:		lb	t0,0(reg_src)		   /* Copy CFI data */
		sb	t0,0(reg_dest)
		ADD	reg_src,1
		add	reg_dest,1
		sub	reg_cnt,1
		bgt	reg_cnt,zero,1b

		FLASHCMD_16B(reg_base,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_EXIT)

		b	nextinst
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_MEMCPY,99f   /* Generic memcpy */

1:		lbu	t0,0(reg_src)
		sb	t0,0(reg_dest)
		add	reg_src,1
		add	reg_dest,1
		sub	reg_cnt,1
		bgt	reg_cnt,zero,1b

		b	nextinst


/* AMD  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if (FLASH_DRIVERS & FLASH_DRIVER_AMD)
99:		bne	reg_op,FEOP_AMD_ERASE8,99f   /* AMD erase (8-bit) */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */

		FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* send the erase command (cycle 3) */

		FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_ERASE_3)

	/* Do an "unlock write" sequence (cycles 4-5) */

		FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

        /* Send the "erase sector" qualifier (cycle 6) */

		FLASHCMD_8(reg_dest,0,AMD_FLASH_ERASE_SEC_6)

	/* Wait for the erase to complete */

1:		lb	t0,0(reg_dest)		# get byte
		and	t0,0xFF			# test hi byte
		bne	t0,0xFF,1b		# go till bit is set

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_AMD_ERASE16,99f   /* AMD erase (16-bit in word mode) */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */

		FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* send the erase command (cycle 3) */

		FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_ERASE_3)

	/* Do an "unlock write" sequence (cycles 4-5) */

		FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

        /* Send the "erase sector" qualifier (cycle 6) */

		FLASHCMD_16(reg_dest,0,AMD_FLASH_ERASE_SEC_6)

	/* Wait for the erase to complete */

1:		lh	t0,0(reg_dest)		# get word
		and	t0,0xFF			# test byte
		bne	t0,0xFF,1b		# go till erased

		b	nextinst

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_AMD_ERASE16B,99f   /* AMD erase (16-bit in byte mode) */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */

		FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* send the erase command (cycle 3) */

		FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_ERASE_3)

	/* Do an "unlock write" sequence (cycles 4-5) */

		FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

        /* Send the "erase sector" qualifier (cycle 6) */

		FLASHCMD_16B(reg_dest,0,AMD_FLASH_ERASE_SEC_6)

	/* Wait for the erase to complete */

1:		lh	t0,0(reg_dest)		# get word
		and	t0,0xFF			# test byte
		bne	t0,0xFF,1b		# go till erased

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_AMD_PGM8,99f	/* AMD 8-bit program */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */
11:
		FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* Send a program command (cycle 3) */

	 	FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_PROGRAM)

	/* Write a byte (cycle 4) */

		lbu	t0,0(reg_src)
		sb	t0,0(reg_dest)	# t0 = byte written to flash
		
	/* Wait for write to complete */

1:		lbu	t2,0(reg_dest)	# t2 = byte from flash

		and	t1,t2,0x80	# done if bit7 of flash
		and	t0,t0,0x80	# is same as bit7 of data
		beq	t1,t0,2f

		and	t1,t2,0x20	# not done if bit5
		bne	t1,0x20,1b	# is still set
2:

	/* next byte...	 */

		add	reg_src,1	# next source byte
		ADD	reg_dest,1	# next dest byte
		sub	reg_cnt,1	# one less count
		bgt	reg_cnt,0,11b


		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_AMD_PGM16,99f	/* AMD 16-bit program */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */
11:
		FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* Send a program command (cycle 3) */

	 	FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_PROGRAM)

	/* Write a byte (cycle 4) */

		lh	t0,0(reg_src)
		sh	t0,0(reg_dest)	# t0 = byte written to flash
		
	/* Wait for write to complete */

1:		lh	t2,0(reg_dest)	# t2 = byte from flash

		and	t1,t2,0x80	# done if bit7 of flash
		and	t0,t0,0x80	# is same as bit7 of data
		beq	t1,t0,2f

		and	t1,t2,0x20	# not done if bit5
		bne	t1,0x20,1b	# is still set
2:

	/* next byte...	 */

		add	reg_src,2	# next source word
		ADD	reg_dest,2	# next dest word
		sub	reg_cnt,2	# one less count
		bgt	reg_cnt,0,11b

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_AMD_PGM16B,99f	/* AMD 16-bit pgm in 8-bit mode */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */
11:
		FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* Send a program command (cycle 3) */

	 	FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_PROGRAM)

	/* Write a byte (cycle 4) */

		lb	t0,0(reg_src)
		sb	t0,0(reg_dest)	# t0 = byte written to flash
		
	/* Wait for write to complete */

1:		lb	t2,0(reg_dest)	# t2 = byte from flash

		and	t1,t2,0x80	# done if bit7 of flash
		and	t0,t0,0x80	# is same as bit7 of data
		beq	t1,t0,2f

		and	t1,t2,0x20	# not done if bit5
		bne	t1,0x20,1b	# is still set
2:

	/* next byte...	 */

		add	reg_src,1	# next source word
		ADD	reg_dest,1	# next dest word
		sub	reg_cnt,1	# one less count
		bgt	reg_cnt,0,11b

		b	nextinst

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:           bne     reg_op,FEOP_AMD_DEVCODE8,99f   /* AMD 8-bit - Boot Block Location */

              ADD     reg_src,reg_src,reg_base

              FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
              FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)
              FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_AUTOSEL)
      
              lbu     t0,AMD_FLASH_DEVCODE8(reg_src)
              sb      t0,0(reg_dest)
              li      t0,AMD_FLASH_RESET
              sb      t0,0(reg_src)


              b       nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:           bne     reg_op,FEOP_AMD_DEVCODE16,99f   /* AMD 8-bit - Boot Block Location */

              ADD     reg_src,reg_src,reg_base

              FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
              FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)
              FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_AUTOSEL)
      
              lw      t0,0(reg_src)
#ifdef __MIPSEB
	      srl     t0,t0,8	/* ((3-AMD_FLASH_DEVCODE16)*8) */
#else
	      srl     t0,t0,16	/* (AMD_FLASH_DEVCODE16*8) */
#endif
              sb      t0,0(reg_dest)
              li      t0,AMD_FLASH_RESET
              sb      t0,0(reg_src)
              
              b       nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:           bne     reg_op,FEOP_AMD_DEVCODE16B,99f   /* AMD 8-bit - Boot Block Location */
      
              ADD     reg_src,reg_src,reg_base
      
              FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
              FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)
              FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_AUTOSEL)
      
              lw      t0,0(reg_src)
#ifdef __MIPSEB
#else
	      srl     t0,t0,16			/* (AMD_FLASH_DEVCODE16B*8)*/
#endif

              sb      t0,0(reg_dest)
              li      t0,AMD_FLASH_RESET
              sb      t0,0(reg_src)
      
              b       nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:           bne     reg_op,FEOP_AMD_MANID8,99f   /* AMD 8-bit - Boot Block Location */

              ADD     reg_src,reg_src,reg_base

              FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
              FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)
              FLASHCMD_8(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_AUTOSEL)
      
              lbu     t0,AMD_FLASH_MANID(reg_src)
              sb      t0,0(reg_dest)
              li      t0,AMD_FLASH_RESET
              sb      t0,0(reg_src)


              b       nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:           bne     reg_op,FEOP_AMD_MANID16,99f   /* AMD 8-bit - Boot Block Location */

              ADD     reg_src,reg_src,reg_base

              FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
              FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)
              FLASHCMD_16(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_AUTOSEL)
      
              lw      t0,0(reg_src)
#ifdef __MIPSEB
	      srl     t0,t0,((3-AMD_FLASH_MANID)*8)
#else
	      srl     t0,t0,(AMD_FLASH_MANID*8)
#endif
              sb      t0,0(reg_dest)
              li      t0,AMD_FLASH_RESET
              sb      t0,0(reg_src)
              
              b       nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:           bne     reg_op,FEOP_AMD_MANID16B,99f   /* AMD 8-bit - Boot Block Location */
      
              ADD     reg_src,reg_src,reg_base
      
              FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
              FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)
              FLASHCMD_16B(reg_base,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_AUTOSEL)
      
              lbu     t0,AMD_FLASH_MANID(reg_src)

              sb      t0,0(reg_dest)
              li      t0,AMD_FLASH_RESET
              sb      t0,0(reg_src)
      
              b       nextinst        

#endif

/* INTEL  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if (FLASH_DRIVERS & FLASH_DRIVER_INTEL)
99:		bne	reg_op,FEOP_INTEL_ERASE8,99f	/* Intel erase 8-bit */

		ADD	reg_dest,reg_dest,reg_base

		FLASHCMD_8(reg_dest,0,INTEL_FLASH_ERASE_BLOCK)
		FLASHCMD_8(reg_dest,0,INTEL_FLASH_ERASE_CONFIRM)

1:		lbu	t0,0(reg_dest)	/* loop till bit 7 is set */
		andi	t0,0x80
		beq	t0,zero,1b

		FLASHCMD_8(reg_dest,0,INTEL_FLASH_READ_MODE)

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_INTEL_ERASE16,99f	/* Intel erase 16-bit */

		ADD	reg_dest,reg_dest,reg_base


		FLASHCMD_8(reg_dest,0,INTEL_FLASH_ERASE_BLOCK)
		FLASHCMD_8(reg_dest,0,INTEL_FLASH_ERASE_CONFIRM)

1:		lbu	t0,0(reg_dest)	/* loop till bit 7 is set */
		andi	t0,0x80
		beq	t0,zero,1b

		FLASHCMD_8(reg_dest,0,INTEL_FLASH_READ_MODE)

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_INTEL_PGM8,99f     /* Intel 8-bit program */

		ADD	reg_dest,reg_dest,reg_base

11:		FLASHCMD_8(reg_dest,0,INTEL_FLASH_PROGRAM)

		lbu	t0,0(reg_src)
		sb	t0,0(reg_dest)

1:		lbu	t0,0(reg_dest)	/* loop till bit 7 is set */
		andi	t0,0x80
		beq	t0,zero,1b

		lbu	t0,0(reg_dest)	/* contains final result */
		/* If good, bits 1, 3, 4 will not be set */

		add     reg_src,1
		ADD     reg_dest,1
		sub     reg_cnt,1
		bgt	reg_cnt,zero,11b

		FLASHCMD_8(reg_dest,0,INTEL_FLASH_READ_MODE)

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_INTEL_PGM16,99f    /* Intel 16-bit prog */

		ADD	reg_dest,reg_dest,reg_base

11:		FLASHCMD_16(reg_dest,0,INTEL_FLASH_PROGRAM)

		lh	t0,0(reg_src)
		sh	t0,0(reg_dest)

1:		lh	t0,0(reg_dest)	/* loop till bit 7 is set */
		andi	t0,0x80
		beq	t0,zero,1b

		lh	t0,0(reg_dest)	/* contains final result */
		/* If good, bits 1, 3, 4 will not be set */

		FLASHCMD_16(reg_dest,0,INTEL_FLASH_READ_MODE)

		add     reg_src,2
		ADD     reg_dest,2
		sub     reg_cnt,2
		bgt	reg_cnt,zero,11b

		b	nextinst
#endif
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if (FLASH_DRIVERS & FLASH_DRIVER_SST)
99:		bne	reg_op,FEOP_SST_CFIQUERY16,99f   /* SST CFI Query 16-bit in word mode */

		ADD	reg_src,reg_src,reg_base

		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_1,SST_FLASH_MAGIC_1)
		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_2,SST_FLASH_MAGIC_2)
		
                FLASHCMD_16(reg_base,0x5555,FLASH_CFI_QUERY_MODE)

1:		lh	t0,0(reg_src)		   /* Copy CFI data */
		sb	t0,0(reg_dest)
		ADD	reg_src,2
		add	reg_dest,2
		sub	reg_cnt,2
		bgt	reg_cnt,zero,1b

		FLASHCMD_16(reg_base,FLASH_CFI_QUERY_ADDR,SST_FLASH_RESET)

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_SST_ERASE16,99f   /* SST erase (16-bit in word mode) */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */

		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_1,SST_FLASH_MAGIC_1)
		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_2,SST_FLASH_MAGIC_2)

	/* send the erase command (cycle 3) */

		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_1,SST_FLASH_ERASE_3)

	/* Do an "unlock write" sequence (cycles 4-5) */

		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_1,SST_FLASH_MAGIC_1)
		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_2,SST_FLASH_MAGIC_2)

        /* Send the "erase sector" qualifier (cycle 6) */

		FLASHCMD_16(reg_dest,0,SST_FLASH_ERASE_SEC_6)

	/* Wait for the erase to complete */

1:		lh	t0,0(reg_dest)		# get word
		and	t0,0xFF			# test byte
		bne	t0,0xFF,1b		# go till erased

		b	nextinst
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		bne	reg_op,FEOP_SST_PGM16,99f	/* SST 16-bit program */

		ADD	reg_dest,reg_dest,reg_base

	/* Do an "unlock write" sequence  (cycles 1-2) */
11:
		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_1,SST_FLASH_MAGIC_1)
		FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_2,SST_FLASH_MAGIC_2)

	/* Send a program command (cycle 3) */

	 	FLASHCMD_16(reg_base,SST_FLASH_MAGIC_ADDR_1,SST_FLASH_PROGRAM)

	/* Write a short (cycle 4) */

		lh	t0,0(reg_src)
		sh	t0,0(reg_dest)	# t0 = byte written to flash
		
	/* Wait for write to complete */

1:		lh	t2,0(reg_dest)	# t2 = byte from flash

		and	t1,t2,0x80	# done if bit7 of flash
		and	t0,t0,0x80	# is same as bit7 of data
		bne	t1,t0,1b


	/* next short...	 */

		add	reg_src,2	# next source word
		ADD	reg_dest,2	# next dest word
		sub	reg_cnt,2	# one less count
		bgt	reg_cnt,0,11b

		b	nextinst
#endif
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
99:		li      v1,-1			  /* invalid command */

nextinst:	SR	v1,FEINST_RESULT(a0)	  /* store result of instruction */
		ADD	v0,v0,v1		  /* add to total */
		ADD	a0,FEINST_SIZE		  /* advance to next instr. */
		b	instloop

flashop_engine_end:
		nop

END(flashop_engine)

		.sdata

		.globl	flashop_engine_ptr
		.globl	flashop_engine_len

flashop_engine_ptr:
		_VECT_	flashop_engine
flashop_engine_len:
		.word	flashop_engine_end-flashop_engine



/*  *********************************************************************
    *  end
    ********************************************************************* */
