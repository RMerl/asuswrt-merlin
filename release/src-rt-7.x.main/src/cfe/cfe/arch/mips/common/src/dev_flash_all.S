/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Flash "self-write" module		File: flash_write_all.S
    *  
    *  This module takes care of the case of writing to the flash
    *  memory that CFE is currently reading its code from.  It is
    *  assumed that you'll be doing a complete flash  update,
    *  so this code erases the affected sectors, reprograms them,
    *  and jumps to the boot sector.
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
#include "dev_flash.h"
#include "mipsmacros.h"

#define WRITEFLASH(base,offset,value) \
	li	t0,value ;	      \
	sb	t0,offset(base)


/*  *********************************************************************
    *  flash_write_all(data,flashbase,size,secsize)
    *  
    *  Write bytes to flash, erasing affected sectors first.
    *  
    *  Input parameters: 
    *  	   a0 - data - pointer to data to write
    *  	   a1 - flashbase - base (phys addr) of flash area
    *  	   a2 - size - number of bytes to write
    *  	   a3 - secsize - flash sector size
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

#define data      a0
#define flashbase a1
#define datasize  a2
#define secsize   a3

#define secidx	  t4
#define secptr    t5
 
LEAF(flash_write_all)

	/*
	 * Mask all interrupts.  An exception with BEV set would be very bad.
	 */

		mfc0	v0,C0_SR		# Get current interrupt flag
		li	v1,M_SR_IE		# master interrupt control
		not	v1			# disable interrupts
		and	v0,v1			# SR now has IE=0
		mtc0	v0,C0_SR		# put back into CP0

	/*
	 * Get KSEG1 addr of flash
	 */

		or	flashbase,K1BASE


	/* 
	 * Do an "unlock write" sequence  (cycles 1-2) 
	 */

		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* 
	 * send the erase command (cycle 3)
	 */

		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_ERASE_3)

	/* 
	 * Do an "unlock write" sequence (cycles 4-5) 
	 */

		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

        /* 
	 * Send the "erase all" qualifier (cycle 6)
	 */

		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_ERASE_ALL_6)

	/*
	 * Wait for the erase to complete
	 */

1:		lb	t0,0(secptr)		# get byte
		and	t0,0xFF			# test hi byte
		bne	t0,0xFF,1b		# go till bit is set


	/*
	 * Okay, now loop through the bytes and write them to the
	 * flash.
	 */

		move	secptr,flashbase
		move	secidx,datasize

proglp:		

	/* 
	 * Do an "unlock write" sequence 
	 */

		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1)
		WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2)

	/* 
	 * Send a program command 
	 */
	 	WRITEFLASH(flashbase,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_PROGRAM)

	/* 
	 * Write a byte 
	 */

		lbu	t0,0(data)
		sb	t0,0(secptr)	# t0 = byte written to flash
		

	/*
	 * Wait for write to complete
	 */

1:		lbu	t2,0(secptr)	# t2 = byte from flash

		and	t1,t2,0x80	# done if bit7 of flash
		and	t0,t0,0x80	# is same as bit7 of data
		beq	t1,t0,2f

		and	t1,t2,0x20	# not done if bit5
		bne	t1,0x20,1b	# is still set
2:

	/*
	 * next byte...
	 */

		add	a0,1		# next source byte
		add	secptr,1	# next dest byte
		sub	datasize,1	# one less count
		bgt	datasize,0,proglp

	/*
	 * All done, reboot system
	 */

		li	v0,0xBFC00000
		j	v0

flash_write_all_end:
		nop

END(flash_write_all)

/*  *********************************************************************
    *  Data
    ********************************************************************* */

		.sdata

		.globl	flash_write_all_ptr
		.globl	flash_write_all_len

flash_write_all_ptr:
		_VECT_	flash_write_all
flash_write_all_len:
		.word	flash_write_all_end-flash_write_all
