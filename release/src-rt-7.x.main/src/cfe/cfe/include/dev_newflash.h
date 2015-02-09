/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Flash memory definitions			File: dev_newflash.h
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

#ifndef __ASSEMBLER__
#include "cfe_device.h"
#include "cfe_iocb.h"
#endif

/*  *********************************************************************
    *  Configuration
    ********************************************************************* */

/* Bits for compile-time removal of features */
#define FLASH_DRIVER_INTEL	1		/* support Intel cmd sets */
#define FLASH_DRIVER_AMD	2		/* support AMD cmd sets */
#define FLASH_DRIVER_CFI	4		/* support auto probing */
#define FLASH_DRIVER_SST	8		/* support for SST cmd sets */

/* Default value unless overridden in bsp_config.h */
#ifndef FLASH_DRIVERS
#define FLASH_DRIVERS (FLASH_DRIVER_SST | FLASH_DRIVER_INTEL | FLASH_DRIVER_AMD | FLASH_DRIVER_CFI)
#endif

/*  *********************************************************************
    *  Flash magic numbers
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
#define AMD_FLASH_DEVCODE8      0x2
#define AMD_FLASH_DEVCODE16     0x1
#define AMD_FLASH_DEVCODE16B    0x2
#define AMD_FLASH_MANID         0x0
#define	AMD_FLASH_PROGRAM	0xA0
#define	AMD_FLASH_UNLOCK_BYPASS	0x20
#define	AMD_FLASH_ERASE_3	0x80
#define	AMD_FLASH_ERASE_4	0xAA
#define	AMD_FLASH_ERASE_5	0x55
#define	AMD_FLASH_ERASE_ALL_6	0x10
#define	AMD_FLASH_ERASE_SEC_6	0x30

/*
 * SST Flash commands and magic offsets
 */

#define	SST_FLASH_MAGIC_ADDR_1	0x5555 
#define	SST_FLASH_MAGIC_ADDR_2	0x2AAA 
 
#define	SST_FLASH_RESET		0xF0
#define	SST_FLASH_MAGIC_1	0xAA
#define	SST_FLASH_MAGIC_2	0x55
#define	SST_FLASH_AUTOSEL	0x90
#define SST_FLASH_DEVCODE8      0x2
#define SST_FLASH_DEVCODE16     0x1
#define SST_FLASH_DEVCODE16B    0x2
#define SST_FLASH_MANID         0x0
#define	SST_FLASH_PROGRAM	0xA0
#define	SST_FLASH_UNLOCK_BYPASS	0x20
#define	SST_FLASH_ERASE_3	0x80
#define	SST_FLASH_ERASE_4	0xAA
#define	SST_FLASH_ERASE_5	0x55
#define	SST_FLASH_ERASE_ALL_6	0x10
#define	SST_FLASH_ERASE_SEC_6	0x30

/*
 * INTEL Flash commands and magic offsets
 */

#define INTEL_FLASH_READ_MODE 0xFF
#define INTEL_FLASH_ERASE_BLOCK	0x20
#define INTEL_FLASH_ERASE_CONFIRM 0xD0
#define INTEL_FLASH_PROGRAM	0x40
#define INTEL_FLASH_WRITE_BUFFER	0xE8
#define INTEL_FLASH_WRITE_BUFFER_SIZE	32
#define INTEL_FLASH_WRITE_CONFIRM 0xD0
#define INTEL_FLASH_READ_STATUS	0x70
#define INTEL_FLASH_CONFIG_SETUP 0x60
#define INTEL_FLASH_LOCK 0x01
#define INTEL_FLASH_UNLOCK 0xD0
#define INTEL_FLASH_LOCKDOWN 0x2F


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
#define FLASH_CFI_EXT_TABLE	0x15
#define FLASH_CFI_DEVICE_SIZE	0x27
#define FLASH_CFI_DEVICE_INTERFACE 0x28
#define FLASH_CFI_REGION_COUNT	0x2C
#define FLASH_CFI_REGION_TABLE	0x2D


#define FLASH_CFI_CMDSET_INTEL_ECS	0x0001	/* Intel extended */
#define FLASH_CFI_CMDSET_AMD_STD	0x0002	/* AMD Standard */
#define FLASH_CFI_CMDSET_INTEL_STD	0x0003	/* Intel Standard */
#define FLASH_CFI_CMDSET_AMD_ECS	0x0004	/* AMD Extended */
#define FLASH_CFI_CMDSET_SST		0x0701	/* SST Standard */

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
#define FLASH_MFR_AMD           0x01

/*  *********************************************************************
    *  Macros for defining custom sector tables
    ********************************************************************* */

#define FLASH_SECTOR_RANGE(nblks,size) (((nblks)-1) << 16) + ((size)/256)
#define FLASH_SECTOR_NBLKS(x) (((x) >> 16)+1)
#define FLASH_SECTOR_SIZE(x) (((x) & 0xFFFF)*256)
#define FLASH_MAXSECTORS  8

/*  *********************************************************************
    *  Flashop engine constants and structures
    *  The flashop engine interprets a little table of commands
    *  to manipulate flash parts - we do this so we can operate
    *  on the flash that we're currently running from.
    ********************************************************************* */

/*
 * Structure of the "instruction" table is six words,
 * size dependant on the machine word size (32 or 64 bits).
 *
 *    opcode
 *    flash base
 *    destination
 *    source
 *    length
 *    result
 */

#define FEINST_OP	_TBLIDX(0)
#define FEINST_BASE	_TBLIDX(1)
#define FEINST_DEST	_TBLIDX(2)
#define FEINST_SRC	_TBLIDX(3)
#define FEINST_CNT	_TBLIDX(4)
#define FEINST_RESULT	_TBLIDX(5)
#define FEINST_SIZE	_TBLIDX(6)	/* size of an instruction */

#ifndef __ASSEMBLER__
typedef struct flashinstr_s {		/* must match offsets above */
    long fi_op;
    long fi_base;
    long fi_dest;
    long fi_src;
    long fi_cnt;
    long fi_result;
} flashinstr_t;
#endif

/*
 * Flash opcodes
 */

#define FEOP_RETURN       0		/* return to CFE */
#define FEOP_REBOOT	  1		/* Reboot system */
#define FEOP_READ8	  2		/* read, 8 bits at a time */
#define FEOP_READ16	  3		/* read, 16 bits at a time */
#define FEOP_CFIQUERY8	  4		/* CFI Query 8-bit */
#define FEOP_CFIQUERY16	  5		/* CFI Query 16-bit */
#define FEOP_CFIQUERY16B  6		/* CFI Query 16-bit */
#define FEOP_MEMCPY	  7		/* generic memcpy */
#define FEOP_AMD_ERASE8	  8		/* AMD-style 8-bit erase-sector */
#define FEOP_AMD_ERASE16  9		/* AMD-style 16-bit erase-sector */
#define FEOP_AMD_ERASE16B 10		/* AMD-style 16-bit erase-sector */
#define FEOP_AMD_PGM8     11            /* AMD-style 8-bit program */
#define FEOP_AMD_PGM16    12            /* AMD-style 16-bit program */
#define FEOP_AMD_PGM16B   13            /* AMD-style 16-bit program */
#define FEOP_AMD_DEVCODE8 14            /* AMD-style 8-bit Boot Block Info */
#define FEOP_AMD_DEVCODE16 15           /* AMD-style 8-bit Boot Block Info */
#define FEOP_AMD_DEVCODE16B 16          /* AMD-style 8-bit Boot Block Info */
#define FEOP_AMD_MANID8   17            /* AMD-style 8-bit Boot Block Info */
#define FEOP_AMD_MANID16  18            /* AMD-style 8-bit Boot Block Info */
#define FEOP_AMD_MANID16B 19            /* AMD-style 8-bit Boot Block Info */
#define FEOP_INTEL_ERASE8 20            /* Intel-style 8-bit erase-sector */
#define FEOP_INTEL_ERASE16 21           /* Intel-style 16-bit erase-sector */
#define FEOP_INTEL_PGM8    22 	        /* Intel-style 8-bit program */
#define FEOP_INTEL_PGM16  23            /* Intel-style 16-bit program */
#define FEOP_SST_CFIQUERY16	24	/* CFI Query that includes the SST unlock */
#define FEOP_SST_ERASE16	25 	/* SST-style 16-bit erase-sector */
#define FEOP_SST_PGM16	26		/* SST-style 16-bit program */
#define FEOP_INTELEXT_ERASE8 27         /* Intel-extended 8-bit erase-sector */
#define FEOP_INTELEXT_ERASE16 28        /* Intel-extended 16-bit erase-sector */
#define FEOP_INTELEXT_PGM8    29 	/* Intel-extended 8-bit program */
#define FEOP_INTELEXT_PGM16  30         /* Intel-extended 16-bit program */

/*
 * Flashop result values.
 */

#define FERES_OK	  0
#define FERES_ERROR	  -1



/*  *********************************************************************
    *  Structures
    ********************************************************************* */

/*
 * Flags.  If you instantiate the driver with probe_a = physical
 * address and probe_b = size, you should also OR in the
 * bus and device width below.
 */

#define FLASH_FLG_AUTOSIZE 0x00000002   /* resize to actual device size */
#define FLASH_FLG_BUS8	   0
#define FLASH_FLG_BUS16	   0x00000004	/* ROM is connected to 16-bit bus */
#define FLASH_FLG_DEV8	   0
#define FLASH_FLG_DEV16	   0x00000010	/* ROM has 16-bit data width */
#define FLASH_FLG_MANUAL   0x00000008	/* Not CFI, manual sectoring */
#define FLASH_FLG_MASK	   0x000000FF	/* mask of probe bits for flags */
#define FLASH_SIZE_MASK    0xFFFFFF00	/* mask of probe bits for size */
/* you don't have to shift the size, we assume it's in multiples of 256bytes */

#ifndef __ASSEMBLER__

/*
 * Partition structure - use this to define a flash "partition."
 * The partitions are assigned in order from the beginning of the flash.
 * The special size '0' means 'fill to end of flash', and you can
 * have more partitions after that which are aligned with the top
 * of the flash.
 * Therefore if you have a 1MB flash and set up
 * partitions for 256KB, 0, 128KB, the 128KB part will be aligned
 * to the top of the flash and the middle block will be 768KB.
 * Partitions can be on byte boundaries.  
 */

typedef struct newflash_part_t {
    fl_size_t fp_size;
    char *fp_name;
} newflash_part_t;
#define FLASH_MAX_PARTITIONS	8

/*
 * Probe structure - this is used when we want to describe to the flash
 * driver the layout of our flash, particularly when you want to
 * manually describe the sectors.
 */

typedef struct newflash_probe_t {
    long flash_phys;		/* physical address of flash */
    int flash_size;		/* total flash size */
    int max_erasesize;          /* maximum erase size among all erase blocks */
    int flash_flags;		/* flags (FLASH_FLG_xxx) */
    int flash_type;		/* FLASH_TYPE_xxx */
    /* The following are used when manually sectoring */
    int flash_cmdset;		/* FLASH_CMDSET_xxx */
    int flash_nsectors;		/* number of ranges */
    int flash_sectors[FLASH_MAXSECTORS];
    /* This says how many contiguous flash chips are in this region */
    int flash_nchips;		/* "flash_size" is just for one chip */
    /* The following are used for partitioned flashes */
    int flash_nparts;		/* zero means not partitioned */
    newflash_part_t flash_parts[FLASH_MAX_PARTITIONS];
    /* The following are used for whacky, weird flashes */
    int (*flash_ioctl_hook)(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
    /* You can replace the flash engine with your own */
    int (*flash_engine_hook)(flashinstr_t *prog);
} newflash_probe_t;


/*  *********************************************************************
    *  PRIVATE STRUCTURES
    *  
    *  These structures are actually the "property" of the 
    *  flash driver.  The only reason a board package might
    *  want to dig around in here is if it implements a hook
    *  or overrides functions to support special, weird flash parts.
    ********************************************************************* */

typedef struct flashdev_s flashdev_t;

typedef struct flashpart_s {
    flashdev_t *fp_dev;
    fl_offset_t fp_offset;
    fl_size_t fp_size;
} flashpart_t;

#define FLASH_MAX_CFIDATA	256	/* total size of CFI Data */
#define FLASH_MAX_INST		8	/* instructions we use during probing */

struct flashdev_s {
    newflash_probe_t fd_probe;	/* probe information */

    uint8_t fd_erasefunc;	/* Erase routine to use */
    uint8_t fd_pgmfunc;		/* program routine to use */
    uint8_t fd_readfunc;	/* Read routine to use */
    flashpart_t fd_parts[FLASH_MAX_PARTITIONS];

    uint8_t *fd_sectorbuffer;	/* sector copy buffer */
    int fd_ttlsect;		/* total sectors on one device */
    int fd_ttlsize;		/* total size of all devices (flash size * nchips) */

    int fd_iptr;		/* flashop engine instructions */
    flashinstr_t *fd_inst;
};



#endif
