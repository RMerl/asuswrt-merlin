/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Flash memory definitions			File: dev_flash.h
    *  
    *  Stuff we use when manipulating flash memory devices.
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

/*
 * AMD Flash commands and magic offsets
 */

#define	AMD_FLASH_MAGIC_ADDR_1	0x555	/* AAA for 16-bit devices in 8-bit mode */
#define	AMD_FLASH_MAGIC_ADDR_2	0x2AA	/* 554 for 16-bit devices in 8-bit mode */

#define	AMD_FLASH_RESET		0xF0
#define	AMD_FLASH_MAGIC_1	0xAA
#define	AMD_FLASH_MAGIC_2	0x55
#define	AMD_FLASH_AUTOSEL	0x90
#define	AMD_FLASH_PROGRAM	0xA0
#define	AMD_FLASH_UNLOCK_BYPASS	0x20
#define	AMD_FLASH_ERASE_3	0x80
#define	AMD_FLASH_ERASE_4	0xAA
#define	AMD_FLASH_ERASE_5	0x55
#define	AMD_FLASH_ERASE_ALL_6	0x10
#define	AMD_FLASH_ERASE_SEC_6	0x30

/*
 * INTEL Flash commands and magic offsets
 */

#define INTEL_FLASH_READ_MODE 0xFF
#define INTEL_FLASH_ERASE_BLOCK	0x20
#define INTEL_FLASH_ERASE_CONFIRM 0xD0
#define INTEL_FLASH_PROGRAM	0x40

/* INTEL Flash commands for 16-bit mode */
#define INTEL_FLASH_16BIT_READ_MODE 		0xFF00
#define INTEL_FLASH_16BIT_ERASE_BLOCK 		0x2000
#define INTEL_FLASH_16BIT_ERASE_CONFIRM 	0xD000
#define INTEL_FLASH_16BIT_PROGRAM 		0x4000
#define INTEL_FLASH_8BIT			0
#define INTEL_FLASH_16BIT			1
  

/*
 * Common Flash Interface (CFI) commands and offsets
 */

#define FLASH_CFI_QUERY_ADDR	0x55
#define FLASH_CFI_QUERY_MODE	0x98
#define FLASH_CFI_QUERY_EXIT	0xFF

#define FLASH_CFI_MANUFACTURER	0x00
#define FLASH_CFI_DEVICE	0x01
#define FLASH_CFI_SIGNATURE	0x10
#define FLASH_CFI_QUERY_STRING	0x10
#define FLASH_CFI_COMMAND_SET	0x13
#define FLASH_CFI_DEVICE_SIZE	0x27
#define FLASH_CFI_DEVICE_INTERFACE 0x28
#define FLASH_CFI_REGION_COUNT	0x2C
#define FLASH_CFI_REGION_TABLE	0x2D

#define FLASH_CFI_CMDSET_INTEL_ECS	0x0001	/* Intel extended */
#define FLASH_CFI_CMDSET_AMD_STD	0x0002	/* AMD Standard */
#define FLASH_CFI_CMDSET_INTEL_STD	0x0003	/* Intel Standard */
#define FLASH_CFI_CMDSET_AMD_ECS	0x0004	/* AMD Extended */

#define FLASH_CFI_DEVIF_X8	0x0000	/* 8-bit asynchronous */
#define FLASH_CFI_DEVIF_X16	0x0001	/* 16-bit asynchronous */
#define FLASH_CFI_DEVIF_X8X16	0x0002	/* 8 or 16-bit with BYTE# pin */
#define FLASH_CFI_DEVIF_X32	0x0003	/* 32-bit asynchronous */

/*
 * JEDEC offsets
 */

#define FLASH_JEDEC_OFFSET_MFR	0
#define FLASH_JEDEC_OFFSET_DEV	1

/* Vendor-specific flash identifiers */

#define FLASH_MFR_HYUNDAI       0xAD

/*  *********************************************************************
    *  Macros for defining custom sector tables
    ********************************************************************* */

#define FLASH_SECTOR_RANGE(nblks,size) (((nblks)-1) << 16) + ((size)/256)
#define FLASH_SECTOR_NBLKS(x) (((x) >> 16)+1)
#define FLASH_SECTOR_SIZE(x) (((x) & 0xFFFF)*256)
#define FLASH_MAXSECTORS  8


/*  *********************************************************************
    *  Structures
    ********************************************************************* */

/*
 * This structure is passed in the "probe_ptr" field of the 
 * flash's probe routines and can be used for advanced 
 * configuration.  If you pass this structure, probe_a and 
 * probe_b will be ignored by the driver
 * 
 * flash_prog_phys is the base address you use for flash commands - 
 * you can put 0 here if it's the same as flash_phys.  some boards,
 * like the Algor P5064, have a different PA region used for doing
 * byte accesses to the flash.  In this case the special
 * "flash_prog_phys" field is used for that.
 */


#define FLASH_FLG_NVRAM	0x00000001	/* Reserve space for NVRAM */
#define FLASH_FLG_AUTOSIZE 0x00000002   /* resize to actual device size */
#define FLASH_FLG_16BIT	0x00000004	/* it's a 16-bit ROM in 16-bit mode */
#define FLASH_FLG_MANUAL 0x00000008	/* Not CFI, manual sectoring */
#define FLASH_FLG_WIDE 0x00000010	/* must shift control addresses left one bit */

#ifndef __ASSEMBLER__
typedef struct flash_probe_t {
    long flash_phys;
    long flash_prog_phys;	/* base address for programming, if different */
    int flash_size;		/* total flash size */
    int flash_flags;
    /* The following are used when manually sectoring */
    int flash_cmdset;
    int flash_nsectors;		/* number of ranges */
    int flash_sectors[FLASH_MAXSECTORS];
} flash_probe_t;
#endif
