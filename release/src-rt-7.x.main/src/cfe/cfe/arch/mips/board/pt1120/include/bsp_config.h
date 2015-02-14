/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BSP Configuration file			File: bsp_config.h
    *  
    *  This module contains global parameters and conditional
    *  compilation settings for building CFE.
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *
    *  modification history
    *  --------------------
    *  01a,27jun02,gtb  derived from ptswarm.  
    *
    *********************************************************************  
    *
    *  Copyright 2000,2001
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
    *     and retain this copyright notice and list of conditions as 
    *     they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation. Neither the "Broadcom 
    *     Corporation" name nor any trademark or logo of Broadcom 
    *     Corporation may be used to endorse or promote products 
    *     derived from this software without the prior written 
    *     permission of Broadcom Corporation.
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



#define CFG_INIT_L1		1	/* initialize the L1 cache */
#define CFG_INIT_L2		1	/* initialize the L2 cache */

#define CFG_INIT_DRAM		1	/* initialize DRAM controller */
#define CFG_DRAM_SIZE		xxx	/* size of DRAM if you don't initialize */

#define CFG_NETWORK		1	/* define to include network support */

#define CFG_UI			1	/* Define to enable user interface */

#define CFG_MULTI_CPUS		0	/* Define to include multiple CPU support */

#define CFG_HEAP_SIZE		1024	/* heap size in kilobytes */

#define CFG_STACK_SIZE		8192	/* stack size (bytes, rounded up to K) */

/*
 * These parameters control the flash driver's sector buffer.  
 * If you write environment variables or make small changes to
 * flash sectors from user applications, you
 * need to have the heap big enough to store a temporary sector
 * for merging in small changes to flash sectors, so you
 * should set CFG_FLASH_ALLOC_SECTOR_BUFFER in that case.
 * Otherwise, you can provide an address in unallocated memory
 * of where to place the sector buffer.
 */

#define CFG_FLASH_ALLOC_SECTOR_BUFFER 0	/* '1' to allocate sector buffer from the heap */
#define CFG_FLASH_SECTOR_BUFFER_ADDR (100*1024*1024-128*1024)	/* 100MB - 128K */
#define CFG_FLASH_SECTOR_BUFFER_SIZE (128*1024)

/*
 * The flash staging buffer is where we store a flash image before we write
 * it to the flash.  It's too big for the heap.
 */

#define CFG_FLASH_STAGING_BUFFER_ADDR (100*1024*1024)
#define CFG_FLASH_STAGING_BUFFER_SIZE (4*1024*1024)

/*
 * These parameters control the default DRAM init table
 * inside of sb1250_draminit.c.
 */

#ifndef CFG_DRAM_ECC
#define CFG_DRAM_ECC		1	/* Turn on to enable ECC */
#endif

#define CFG_DRAM_SMBUS_CHANNEL	0	/* SMBus channel for memory SPDs */
#define CFG_DRAM_SMBUS_BASE     0x54	/* starting SMBus device base */
#define CFG_DRAM_BLOCK_SIZE	32	/* don't interleave columns */
#define CFG_DRAM_CSINTERLEAVE   0       /* Use 0,1, or 2. Max number of address
					   bits allowed for chip select
					   interleaving. Only matching dimms
					   will be interleaved.  3 outcomes:
					   no interleaving, interleave CS 0 &
					   1, and interleave CS 0,1,2 & 3. */
#define CFG_DRAM_INTERLEAVE	0	/* interleave channels if possible */

/* N.B.:  Some P.C.s can't talk this fast ! */

#define CFG_SERIAL_BAUD_RATE	115200	/* normal console speed */

/* 
* The ptswarm board has an EXAR st16550 UART on the I/O bus at chip select 2.
* It is compatible with an NS16550, which is supported by CFE.
* Our clock crytal is twice as fast as the default, so here's the override.
*/
#define NS16550_HZ      3686400		/* serial clock for the external UART */


#define CFG_VENDOR_EXTENSIONS	0

#include "pt1120.h"
