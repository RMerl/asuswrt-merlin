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
#define CFG_INIT_L2		0	/* there is no L2 cache */

#define CFG_INIT_DRAM		1	/* initialize DRAM controller */
#define CFG_DRAM_SIZE		xxx	/* size of DRAM if you don't initialize */

#define CFG_NETWORK		1	/* define to include network support */
#define CFG_TCP			0	/* include TCP stack */

#define CFG_FATFS               0
#define CFG_HTTPFS		0	/* Enable HTTP filesystem */

#ifdef CFG_ROMBOOT
#define CFG_DHCP		1	/* Enable DHCP client */
#else
#define CFG_DHCP		0	/* Disable DHCP client */
#endif
#define CFG_AUTOBOOT		0	/* Enable auto command */

#define CFG_HTTP		1	/* Enable mini Web server */
#if CFG_HTTP
#define CFG_TCP			0	/* Disable CFG_TCP when the mini Web server is enabled */
#endif

#define CFG_UI			1	/* Define to enable user interface */

#define CFG_MULTI_CPUS		0	/* no multi-cpu support */

#ifndef CFG_HEAP_SIZE
#define CFG_HEAP_SIZE           400    /* heap size in kilobytes */
#endif

#define CFG_STACK_SIZE		8192	/* stack size (bytes, rounded up to K) */

#ifdef CFG_SIM
#define CFG_SERIAL_BAUD_RATE	600	/* normal console speed */
#else
#define CFG_SERIAL_BAUD_RATE	115200	/* normal console speed */
#endif

#define CFG_VENDOR_EXTENSIONS   0

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
#define CFG_FLASH_SECTOR_BUFFER_ADDR  (1*1024*1024-128*1024)
#define CFG_FLASH_SECTOR_BUFFER_SIZE  (128*1024)

/*
 * The flash staging buffer is where we store a flash image before we write
 * it to the flash.  It's too big for the heap. If zero then the code will
 * choose between the given ADDR and mem_topofmem, depending on which
 * area is bigger.
 */

#define CFG_FLASH_STAGING_BUFFER_ADDR (4096)
#define CFG_FLASH_STAGING_BUFFER_SIZE 0

/*
 * Default TFTP maximum number retries is 3.
 */

#define TFTP_MAX_RETRIES 3

/*
 * Maximum retry times limit to do ui_docommand("flash -noheader : flash1.trx")
 * when check_trx() have problem.
 */
#define CFE_ERR_TIMEOUT_LIMIT 3
