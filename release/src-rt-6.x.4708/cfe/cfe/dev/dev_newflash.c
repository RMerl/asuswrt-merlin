/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  "New" Flash device driver		File: dev_newflash.c
    *  
    *  This driver supports various types of flash
    *  parts. 
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


#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "addrspace.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"

#include "bsp_config.h"
#include "dev_newflash.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

/*
 * Get the address of the flash sector buffer from the
 * config file.  Addresses are PHYSICAL.
 */

#ifndef CFG_FLASH_SECTOR_BUFFER_ADDR
#define CFG_FLASH_SECTOR_BUFFER_ADDR	(100*1024*1024-128*1024)
#endif

#ifndef CFG_FLASH_SECTOR_BUFFER_SIZE
#define CFG_FLASH_SECTOR_BUFFER_SIZE	(128*1024)
#endif


/* #define _NEWFLASH_DEBUG_ */

/*  *********************************************************************
    *  Forward declarations
    ********************************************************************* */


static void flashdrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr);


static int flashdrv_open(cfe_devctx_t *ctx);
static int flashdrv_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int flashdrv_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat);
static int flashdrv_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int flashdrv_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer);
static int flashdrv_close(cfe_devctx_t *ctx);

/*  *********************************************************************
    *  Device dispatch
    ********************************************************************* */

const static cfe_devdisp_t flashdrv_dispatch = {
    flashdrv_open,
    flashdrv_read,
    flashdrv_inpstat,
    flashdrv_write,
    flashdrv_ioctl,
    flashdrv_close,	
    NULL,
    NULL
};

const cfe_driver_t newflashdrv = {
    "New CFI flash",
    "flash",
    CFE_DEV_FLASH,
    &flashdrv_dispatch,
    flashdrv_probe
};



/*  *********************************************************************
    *  Externs
    ********************************************************************* */

extern void *flashop_engine_ptr;
extern int flashop_engine_len;

extern void _cfe_flushcache(int);

static int flash_sector_query(flashdev_t *softc,flash_sector_t *sector);

/*  *********************************************************************
    *  Globals
    ********************************************************************* */

/*
 * This is a pointer to a DRAM version of our flash subroutines.
 * We make a global here so that it doesn't get copied multiple
 * times for each flash we instantiate.
 */

static int (*flashop_engine_ram)(flashinstr_t *prog) = NULL;
static uint8_t *flash_sector_buffer = NULL;

/*  *********************************************************************
    *  FLASH_OP_BEGIN(softc)
    *  
    *  Reset the pointer to the flash operations so that we can
    *  begin filling in new instructions to execute
    *  
    *  Input parameters: 
    *  	   softc - our softc.
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#define flash_op_begin(softc)  softc->fd_iptr = 0;

/*  *********************************************************************
    *  FLASH_OP_ADD(softc,op,dest,src,cnt)
    *  
    *  Add an instruction to the flashop table
    *  
    *  Input parameters: 
    *  	   softc - our flash
    *  	   op,dest,src,cnt - data for the opcode
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flash_op_add(flashdev_t *softc,long base,long op,long dest,long src,long cnt)
{
    flashinstr_t *fi = &(softc->fd_inst[softc->fd_iptr]);

    fi->fi_op = op;
    fi->fi_base = base;
    fi->fi_dest = dest;
    fi->fi_src = src;
    fi->fi_cnt = cnt;

    softc->fd_iptr++;
}


/*  *********************************************************************
    *  FLASH_OP_EXECUTE(softc)
    *  
    *  Execute the stored "flash operations"
    *  
    *  Input parameters: 
    *  	   softc - our flash
    *  	   
    *  Return value:
    *  	   0 if ok, else # of failures (less than zero)
    ********************************************************************* */

static int flash_op_execute(flashdev_t *softc) 
{
    flash_op_add(softc,softc->fd_probe.flash_phys,FEOP_RETURN,0,0,0);

#ifdef _NEWFLASH_DEBUG_
    if (1) {
	int idx;
	printf("---------------\nCalling engine @ %08X\n",flashop_engine_ram);
	for (idx = 0; idx < softc->fd_iptr; idx++) {
	    printf("%2d %08X %08X %08X %08X\n",
		   softc->fd_inst[idx].fi_op,
		   softc->fd_inst[idx].fi_base,
		   softc->fd_inst[idx].fi_dest,
		   softc->fd_inst[idx].fi_src,
		   softc->fd_inst[idx].fi_cnt);
	    }
	}
#endif

    /*
     * If someone hooked the flashop engine, call the hook.
     */
    if (softc->fd_probe.flash_engine_hook) {
	return (*(softc->fd_probe.flash_engine_hook))(&(softc->fd_inst[0]));
	}

    /*
     * Otherwise, call the standard one.
     */
    if (!flashop_engine_ram) return CFE_ERR_UNSUPPORTED;
    return (*flashop_engine_ram)(&(softc->fd_inst[0]));
}


/*  *********************************************************************
    *  FLASH_SETUP_ENGINE()
    *  
    *  Set up the "flash engine", copying the routine to DRAM
    *  and flushing the cache so we can call it.
    *  
    *  Input parameters: 
    *  	   nothing
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flash_setup_engine(void)
{
#if (CFG_RAMAPP || CFG_EMBEDDED_PIC || CFG_CFLASH)
    /* CFE is relocated, no need to copy flash engine to heap */
    flashop_engine_ram = (void *) flashop_engine_ptr;
#else
    /* Copy flash engine to heap */
    uint32_t *dst,*src;
    int idx;

    if (flashop_engine_ram) return;		/* already done */

    /*
     * Allocate space for engine
     */

    flashop_engine_ram = (void *) KMALLOC(flashop_engine_len,0);
    if (!flashop_engine_ram) return;

    /*
     * Copy engine to RAM - do it 32-bits at a time to avoid
     * a certain platform with broken byte reads (no, not the 1250)
     */

    dst = (uint32_t *) flashop_engine_ram;
    src = (uint32_t *) flashop_engine_ptr;
    for (idx = 0; idx < flashop_engine_len/sizeof(uint32_t); idx++) {
	*dst++ = *src++;
	}

    /*
     * Flush the d-cache, invalidate the I-cache.
     */

    _cfe_flushcache(1);
    _cfe_flushcache(2);
#endif
}

/*  *********************************************************************
    *  FLASH_ERASE_RANGE(softc,range)
    *  
    *  Erase a range of sectors
    *  
    *  Input parameters: 
    *  	   softc - our flash
    *  	   range - range structure
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error
    ********************************************************************* */

static int flash_erase_range(flashdev_t *softc,flash_range_t *range)
{
    flash_sector_t sector;
    int res;

    if (softc->fd_probe.flash_type != FLASH_TYPE_FLASH) {
	return CFE_ERR_UNSUPPORTED;
	}

    if (range->range_base+range->range_length > softc->fd_probe.flash_size) {
	return CFE_ERR_INV_PARAM;
	}

    res = 0;

    sector.flash_sector_idx = 0;

    for (;;) {
	res = flash_sector_query(softc,&sector);
	if (res != 0) break;
	if (sector.flash_sector_status == FLASH_SECTOR_INVALID) {
	    break;
	    }

	if ((sector.flash_sector_offset >= range->range_base) &&
	    (sector.flash_sector_offset <
	     (range->range_base+range->range_length-1))) {

	    flash_op_begin(softc);
	    flash_op_add(softc,softc->fd_probe.flash_phys,
			 softc->fd_erasefunc,
			 sector.flash_sector_offset,
			 0,0);
	    res = flash_op_execute(softc);

	    if (res != 0) break;
	    }
	sector.flash_sector_idx++;
	}

    return res;

}

/*  *********************************************************************
    *  FLASH_ERASE_ALL(softc)
    *  
    *  Erase the entire flash device, except the NVRAM area, 
    *  sector-by-sector.
    *  
    *  Input parameters: 
    *  	   softc - our flash
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int flash_erase_all(flashdev_t *softc)
{
    flash_range_t range;

    range.range_base = 0;
    range.range_length = softc->fd_probe.flash_size * 
	softc->fd_probe.flash_nchips;

    return flash_erase_range(softc,&range);
}


/*  *********************************************************************
    *  flash_range_intersection(sector,inrange,outrange)
    *  
    *  Compute the intersection between a flash range and a
    *  sector.
    *  
    *  Input parameters: 
    *  	   sector - sector to examine
    *  	   range - range we are checking
    *  	   outrange - where to put resulting intersection range
    *  	   
    *  Return value:
    *  	   1 - range is an entire sector
    *  	   0 - range is a partial sector
    *  	   -1 - range has no intersection
    ********************************************************************* */

static int flash_range_intersection(flash_sector_t *sector,
				    flash_range_t *inrange,
				    flash_range_t *outrange)
{
    int start,end;

    /*
     * compute the start and end pointers 
     */

    start = (int) (max(sector->flash_sector_offset,
		       inrange->range_base));

    end = (int) (min((sector->flash_sector_offset+sector->flash_sector_size),
		     (inrange->range_base+inrange->range_length)));

    /*
     * if the end is in the right place wrt the start,
     * there is an intersection.
     */

    if (end > start) {
	outrange->range_base   = (unsigned int) start;
	outrange->range_length = (unsigned int) (end-start);

	if ((sector->flash_sector_offset == outrange->range_base) &&
	    (sector->flash_sector_size == outrange->range_length)) {
	    return 1;		/* instersection: entire sector */
	    }
	else {
	    return 0;		/* intersection: partial sector */
	    }
	}
    else {
	outrange->range_base = (unsigned int) start;
	outrange->range_length = 0;
	return -1;		/* intersection: none */
	}
}


/*  *********************************************************************
    *  FLASH_SECTOR_QUERY(softc,sector)
    *  
    *  Query the sector information about a particular sector.  You can
    *  call this iteratively to find out about all of the sectors.
    *  
    *  Input parameters: 
    *  	   softc - our flash info
    *  	   sector - structure to receive sector information
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int flash_sector_query(flashdev_t *softc,flash_sector_t *sector)
{
    int idx;
    int nblks;
    int blksiz;
    unsigned int offset;
    int whichchip;
    int secidx;
    int curblk;

    if (softc->fd_probe.flash_type != FLASH_TYPE_FLASH) {
	return CFE_ERR_UNSUPPORTED;
	}	

    if (softc->fd_probe.flash_nsectors == 0) {
	return CFE_ERR_UNSUPPORTED;
	}

    /* Figure out which chip */
    whichchip = sector->flash_sector_idx / softc->fd_ttlsect;
    if (whichchip >= softc->fd_probe.flash_nchips) {
	sector->flash_sector_status = FLASH_SECTOR_INVALID;
	return 0;
	}

    /* Within that chip, get sector info */
    offset = softc->fd_probe.flash_size * whichchip;
    secidx = sector->flash_sector_idx % softc->fd_ttlsect;
    curblk = 0;

    for (idx = 0; idx < softc->fd_probe.flash_nsectors; idx++) {
	nblks = FLASH_SECTOR_NBLKS(softc->fd_probe.flash_sectors[idx]);
	blksiz = FLASH_SECTOR_SIZE(softc->fd_probe.flash_sectors[idx]);
	if (secidx < curblk+nblks) {
	    sector->flash_sector_status = FLASH_SECTOR_OK;
	    sector->flash_sector_offset = 
		offset + (secidx-curblk)*blksiz;
	    sector->flash_sector_size = blksiz;
	    break;
	    }

	offset += (nblks)*blksiz;
	curblk += nblks;
	}
    

    if (idx == softc->fd_probe.flash_nsectors) {
	sector->flash_sector_status = FLASH_SECTOR_INVALID;
	}

    return 0;
}


/*  *********************************************************************
    *  FLASH_SET_CMDSET(softc,cmdset,bus16,dev16)
    *  
    *  Set the command-set that we'll honor for this flash.
    *  
    *  Input parameters: 
    *  	   softc - our flash
    *  	   cmdset - FLASH_CFI_CMDSET_xxx
    *      bus16 - true if bus is 16 bits wide
    *      dev16 - true if device supports 16-bit operation
    *
    *  So: bus16 && dev16 -> 16-bit commands
    *      !bus16 && dev16 -> 8-bit commands to 16-bit flash with BYTE#
    *      !bus16 && !dev16 -> 8-bit commands
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flash_set_cmdset(flashdev_t *softc,int cmdset,int bus16,int dev16)
{
    switch (cmdset) {
#if (FLASH_DRIVERS & FLASH_DRIVER_INTEL)
	case FLASH_CFI_CMDSET_INTEL_ECS:
	    if (bus16) {
		softc->fd_erasefunc = FEOP_INTELEXT_ERASE16;
		softc->fd_pgmfunc   = FEOP_INTELEXT_PGM16;
		softc->fd_readfunc  = FEOP_READ16;
		}
	    else {
		softc->fd_erasefunc = FEOP_INTELEXT_ERASE8;
		softc->fd_pgmfunc   = FEOP_INTELEXT_PGM8;
		softc->fd_readfunc  = FEOP_READ8;
		}
	    break;
	case FLASH_CFI_CMDSET_INTEL_STD:
	    if (bus16) {
		softc->fd_erasefunc = FEOP_INTEL_ERASE16;
		softc->fd_pgmfunc   = FEOP_INTEL_PGM16;
		softc->fd_readfunc  = FEOP_READ16;
		}
	    else {
		softc->fd_erasefunc = FEOP_INTEL_ERASE8;
		softc->fd_pgmfunc   = FEOP_INTEL_PGM8;
		softc->fd_readfunc  = FEOP_READ8;
		}
	    break;
#endif
#if (FLASH_DRIVERS & FLASH_DRIVER_AMD)
	case FLASH_CFI_CMDSET_AMD_STD:
	case FLASH_CFI_CMDSET_AMD_ECS:
	    if (!bus16 && !dev16) {	/* 8-bit bus, 8-bit flash */
		softc->fd_erasefunc = FEOP_AMD_ERASE8;
		softc->fd_pgmfunc   = FEOP_AMD_PGM8;
		softc->fd_readfunc  = FEOP_READ8;
		}
	    else if (bus16 && dev16) {	/* 16-bit bus, 16-bit flash */
		softc->fd_erasefunc = FEOP_AMD_ERASE16;
		softc->fd_pgmfunc   = FEOP_AMD_PGM16;
		softc->fd_readfunc  = FEOP_READ16;
		}
	    else {			/* 8-bit bus, 16-bit flash w/BYTE# */
		softc->fd_erasefunc = FEOP_AMD_ERASE16B;
		softc->fd_pgmfunc   = FEOP_AMD_PGM16B;
		softc->fd_readfunc  = FEOP_READ8;
		}
	    break;
#endif
#if (FLASH_DRIVERS & FLASH_DRIVER_SST)
	case FLASH_CFI_CMDSET_SST:
		if (bus16 && dev16) {	/* 16-bit bus, 16-bit flash */
		softc->fd_erasefunc = FEOP_SST_ERASE16;
		softc->fd_pgmfunc   = FEOP_SST_PGM16;
		softc->fd_readfunc  = FEOP_READ16;
		}
	    else {			
		/* we don't understand the command set - treat it like ROM */
		softc->fd_erasefunc = FEOP_RETURN;
		softc->fd_pgmfunc   = FEOP_RETURN;
		softc->fd_readfunc  = bus16 ? FEOP_READ16 : FEOP_READ8;
		}
	    break;
#endif
	default:
	    /* we don't understand the command set - treat it like ROM */
	    softc->fd_erasefunc = FEOP_RETURN;
	    softc->fd_pgmfunc   = FEOP_RETURN;
	    softc->fd_readfunc  = bus16 ? FEOP_READ16 : FEOP_READ8;
	    break;
	}
}

#if (FLASH_DRIVERS & FLASH_DRIVER_CFI)
/*  *********************************************************************
    *  FLASH_CFI_PROBE(softc)
    *  
    *  Try to do a CFI query on this device.  If we find the m
    *  magic signature, extract some useful information from the
    *  query structure.
    *  
    *  Input parameters: 
    *  	   softc - out flash
    *  	   
    *  Return value:
    *  	   0 if successful, <0 if error
    ********************************************************************* */
static int flash_cfi_probe(flashdev_t *softc)
{
    uint8_t cfidata[FLASH_MAX_CFIDATA];
    unsigned int cmdset;
    unsigned int devif;
    int bus16 = 0;
    int dev16 = 0;
    int idx, idx_cmd[2];
    int found = 0;
    int regcnt;
    int nblks;
    int blksiz;
    int insane = 0;
    uint16_t insaner = 0;
    uint8_t mfgid = 0;

    if (softc->fd_probe.flash_flags & FLASH_FLG_BUS16) {
	bus16 = 1;
	}

    /*
     * Do a CFI query (16-bit)
     */

    idx = FEOP_CFIQUERY8;
    if (softc->fd_probe.flash_flags & FLASH_FLG_DEV16) {
	idx = (softc->fd_probe.flash_flags & FLASH_FLG_BUS16) ?
	    FEOP_CFIQUERY16 : FEOP_CFIQUERY16B;
	}

    flash_op_begin(softc);
    flash_op_add(softc,softc->fd_probe.flash_phys,
		 idx,(long)cfidata,0,FLASH_MAX_CFIDATA);
    flash_op_execute(softc);

    /* 
     * Look for signature. 
     */
       

    if ((cfidata[FLASH_CFI_SIGNATURE+0] == 'Q') && 
	(cfidata[FLASH_CFI_SIGNATURE+1] == 'R') && 
	(cfidata[FLASH_CFI_SIGNATURE+2] == 'Y')) {
	found = 1;
	}

    /* Now try the SST CFI query */
    if (!found && (idx==FEOP_CFIQUERY16)) {
	idx = FEOP_SST_CFIQUERY16;
	flash_op_begin(softc);
	flash_op_add(softc,softc->fd_probe.flash_phys,
		     idx,(long)cfidata,0,FLASH_MAX_CFIDATA);
	flash_op_execute(softc);

	/* 
	 * Look for signature. 
	 */

	if ((cfidata[FLASH_CFI_SIGNATURE+0] == 'Q') && 
	    (cfidata[FLASH_CFI_SIGNATURE+1] == 'R') && 
	    (cfidata[FLASH_CFI_SIGNATURE+2] == 'Y')) {
		found = 1;
	    }
    }

    /*
     * No CFI, bail.  First, set the command set to an invalid
     * value so that we'll use default routines to read but not do programming
     */

    if (!found) {
	flash_set_cmdset(softc,-1,bus16,dev16);
	return CFE_ERR_UNSUPPORTED;
	}

    softc->fd_probe.flash_type = FLASH_TYPE_FLASH;

    /* 
     * Gather info from flash 
     */

    cmdset = ((unsigned int) (cfidata[FLASH_CFI_COMMAND_SET])) +
	(((unsigned int) (cfidata[FLASH_CFI_COMMAND_SET+1])) << 8);

    
    devif = ((unsigned int) (cfidata[FLASH_CFI_DEVICE_INTERFACE])) +
	(((unsigned int) (cfidata[FLASH_CFI_DEVICE_INTERFACE+1])) << 8);

    
    softc->fd_probe.flash_size = (1 << (unsigned int)(cfidata[FLASH_CFI_DEVICE_SIZE]));

    /*
     * It's a 16-bit device if it is either always 16 bits or can be.
     * we'll use "bus16" to decide if the BYTE# pin was strapped
     */

    dev16 = 0;
    if ((devif == FLASH_CFI_DEVIF_X16) || (devif == FLASH_CFI_DEVIF_X8X16)) dev16 = 1;

    regcnt = cfidata[FLASH_CFI_REGION_COUNT];

    softc->fd_probe.flash_nsectors = regcnt;

    /* 
     * Hiss!  Some AMD top-boot flash parts have broken CFI tables - they are backwards! 
     * Do some extra probing to find it.
     */
	if (cmdset == FLASH_CFI_CMDSET_AMD_STD) {
		unsigned int ext;
		uint8_t sig0, sig1, sig2, major, minor, boot;

		ext = ((unsigned int)(cfidata[FLASH_CFI_EXT_TABLE])) +
		        (((unsigned int) (cfidata[FLASH_CFI_EXT_TABLE + 1])) << 8);

		sig0 = cfidata[ext + 0];
		sig1 = cfidata[ext + 1];
		sig2 = cfidata[ext + 2];
		major = cfidata[ext + 3];
		minor = cfidata[ext + 4];
		boot = cfidata[ext + 15];
	
		idx_cmd[0] = FEOP_AMD_DEVCODE8;
		idx_cmd[1] = FEOP_AMD_MANID8;
		if (softc->fd_probe.flash_flags & FLASH_FLG_DEV16) {
			idx_cmd[0] = (softc->fd_probe.flash_flags & FLASH_FLG_BUS16) ?
			        FEOP_AMD_DEVCODE16 : FEOP_AMD_DEVCODE16B;
			idx_cmd[1] = (softc->fd_probe.flash_flags & FLASH_FLG_BUS16) ?
			        FEOP_AMD_MANID16 : FEOP_AMD_MANID16B;
		}
 
		flash_op_begin(softc);
		flash_op_add(softc,softc->fd_probe.flash_phys,
		             idx_cmd[0],(long)&insaner,0,0);
		flash_op_add(softc,softc->fd_probe.flash_phys,
		             idx_cmd[1],(long)&mfgid,0,0);
		flash_op_execute(softc);
		insaner &= 0xff;


#ifdef _NEWFLASH_DEBUG_
		xprintf("CFI Mfg Info mfgid: 0x%x, devid 0x%x boot 0x%x\n\n",
		        mfgid, insaner, boot);
		xprintf("CFI Ext Info @ 0x%x, sign 0x%x%x%x, major 0x%x, minor 0x%x, boot 0x%x\n\n",
		        ext, sig0, sig1, sig2, major, minor, boot);
#endif /* _NEWFLASH_DEBUG_ */

		if ((sig0 == 'P') && (sig1 == 'R') && (sig2 == 'I')) {
			/* MX29LV320CB seems to report Top boot on CFI query,
			 * were as the device is actually Bottom boot. We check
			 * for device id incase of this flash to make out T/B boot.
			 */
			if ((mfgid == 0xc2) && (insaner == 0xa7)) insane = 1;

			if (((major << 8) | minor) < 0x3131) {
				/* CFI version 1.0, don't trust the boot indicator */
				goto oldway;
			}
			if (boot == 3)
				insane = 1;
		} else {
oldway:
#ifdef _NEWFLASH_DEBUG_
			xprintf("Insaner = 0x%x\n", insaner);
#endif /* _NEWFLASH_DEBUG_ */
			if ((insaner == 0xc4) || (insaner == 0xf6)) {
				insane = 1;
			}
		}
#ifdef _NEWFLASH_DEBUG_
		if (insane)
			xprintf("Warning: Top boot AMD part with backwards CFI table!\n");
#endif /* _NEWFLASH_DEBUG_ */
	}

	for (idx = 0; idx < regcnt; idx++) {
		nblks = ((int)cfidata[FLASH_CFI_REGION_TABLE+0+idx*4] + 
		         (int)(cfidata[FLASH_CFI_REGION_TABLE+1+idx*4]<<8)) + 1;
		blksiz = ((int)cfidata[FLASH_CFI_REGION_TABLE+2+idx*4] + 
		          (int)(cfidata[FLASH_CFI_REGION_TABLE+3+idx*4]<<8)) * 256;

		if (softc->fd_probe.max_erasesize < blksiz)
			softc->fd_probe.max_erasesize = blksiz;

		if (insane) {
			/* Insane */
			softc->fd_probe.flash_sectors[((regcnt-1)-idx)] = 
			        FLASH_SECTOR_RANGE(nblks,blksiz);
		} else {
			/* Sane */
			softc->fd_probe.flash_sectors[idx] = 
			        FLASH_SECTOR_RANGE(nblks,blksiz);
		}
#ifdef _NEWFLASH_DEBUG_
		xprintf("Sector range[%d] = 0x%08x i.e. num = 0x%x(%d) size = 0x%x(%d)\n", idx,
		        softc->fd_probe.flash_sectors[idx], nblks, nblks, blksiz, blksiz);
#endif /* _NEWFLASH_DEBUG_ */
	}

    /*
     * Set the command set we're going to use.
     */

    flash_set_cmdset(softc,cmdset,bus16,dev16);

    return 0;

}



/*  *********************************************************************
    *  FLASH_DO_PROBE(softc)
    *  
    *  Probe to see if we're ROM or RAM.  If ROM, see if we're flash.
    *  If flash, do CFI query.
    *  
    *  Input parameters: 
    *  	   softc - our structure
    *  	   
    *  Return value:
    *  	   FLASH_TYPE_xxx
    ********************************************************************* */
static int flash_do_probe(flashdev_t *softc)
{
    uint8_t test_byte0,test_byte1;
    uint8_t save0,save1;
    volatile uint8_t *ptr;

    /*
     * flash_do_probe is called before we open the device, so we
     * need to allocate space for instructions so the flashop
     * engine will work.
     */

    softc->fd_inst = KMALLOC(FLASH_MAX_INST*sizeof(flashinstr_t),0);
    if (!softc->fd_inst) return FLASH_TYPE_ROM;

    /*
     * Attempt to read/write byte zero.  If it is changable,
     * this is SRAM (or maybe a ROM emulator with the write line hooked up)
     */

    ptr = (volatile uint8_t *) UNCADDR(softc->fd_probe.flash_phys);
    save0 = *ptr;			/* save old value */
    save1 = *(ptr+1);			/* save old value */

    test_byte0 = (save0 != 0x88) ? 0x88 : 0x89;
    test_byte1 = (save1 != 0x99) ? 0x99 : 0x91;

    *(ptr) = test_byte0;
    *(ptr+1) = test_byte1;

    if ((*ptr == test_byte0) && (*(ptr+1) == test_byte1)) {
	softc->fd_probe.flash_type = FLASH_TYPE_SRAM;

	/*Only write back saved values if it's RAM*/
	*(ptr) = save0;
	*(ptr+1) = save1;

#ifdef _NEWFLASH_DEBUG_
	xprintf("Flash type SRAM\n");
#endif

	}
    else {
	softc->fd_probe.flash_type = FLASH_TYPE_ROM;

#ifdef _NEWFLASH_DEBUG_
	xprintf("Flash type ROM\n");
#endif

	}

    /* 
     * If we thought it was ROM, try doing a CFI query
     * to see if it was flash.  This check is kind of kludgey
     * but should work.
     */

    if (softc->fd_probe.flash_type == FLASH_TYPE_ROM) {
	flash_cfi_probe(softc);
	}


    KFREE(softc->fd_inst);
    softc->fd_inst = NULL;

    return softc->fd_probe.flash_type;
}

#endif  /* (FLASH_DRIVERS & FLASH_DRIVER_CFI) */


/*  *********************************************************************
    *  flash_do_parts(probe,parts)
    *  
    *  Partition the flash into the sizes specified.  We use
    *  the sizes in the table to generate a table of {offset,size}
    *  pairs that eventually become partitions.
    *  
    *  The only thing magical about this is that size "0" means
    *  "fill to max" and that partitions beyond the "0" are aligned
    *  to the top of the flash.  Therefore, if you had a 4MB
    *  flash and listed four partitions, 512K, 0, 512K, 512K,
    *  then there would be a 2.5M partition in the middle and two
    *  512K partitions at the top.
    *  
    *  Input parameters: 
    *  	   probe - flash probe data (user-supplied table)
    *  	   parts - our partition table (output)
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flash_do_parts(flashdev_t *softc)
{
    int idx;
    int middlepart = -1;
    int lobound = 0;
    newflash_probe_t *probe = &(softc->fd_probe);
    flashpart_t *parts = &(softc->fd_parts[0]);
    int hibound = probe->flash_size*probe->flash_nchips;

    for (idx = 0; idx < probe->flash_nparts; idx++) {
	if (probe->flash_parts[idx].fp_size == 0) {
	    middlepart = idx;
	    break;
	    }
	parts[idx].fp_offset = lobound;
	parts[idx].fp_size = probe->flash_parts[idx].fp_size;
	lobound += probe->flash_parts[idx].fp_size;
	}

    if (idx != probe->flash_nparts) {
	for (idx = probe->flash_nparts - 1; idx > middlepart;
	     idx--) {
	    parts[idx].fp_size = probe->flash_parts[idx].fp_size;
	    hibound -= probe->flash_parts[idx].fp_size;
	    parts[idx].fp_offset = hibound;
	    }
	 /* Truncating highbound to adjust to max_erasesize
	  * to keep the middlepart size in CFE and linux in
	  * sync
	  */
	 hibound = (parts[++idx].fp_offset / probe->max_erasesize) * \
		    probe->max_erasesize;
	}

    if (middlepart != -1) {
	parts[middlepart].fp_offset = lobound;
	parts[middlepart].fp_size = hibound - lobound;
	}

#ifdef _NEWFLASH_DEBUG_
    printf("Partition information:\n");
    for (idx = 0; idx < probe->flash_nparts;idx++) {
	printf("#%02d   %08X -> %08X  (%d)\n",idx,
	       parts[idx].fp_offset,parts[idx].fp_offset+parts[idx].fp_size-1,
	       parts[idx].fp_size);
	}
#endif
}


/*  *********************************************************************
    *  flashdrv_allocbuf(dev)
    *  
    *  Allocate sector buffer for flash programming.  Use a global
    *  buffer for all devices.
    *  
    *  Input parameters: 
    *  	   dev - our device
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void flashdrv_allocbuf(flashdev_t *softc)
{
    if (!flash_sector_buffer) {
#if CFG_FLASH_ALLOC_SECTOR_BUFFER
	flash_sector_buffer = KMALLOC(CFG_FLASH_SECTOR_BUFFER_SIZE,0);
	if (!flash_sector_buffer) {
	    printf("FLASH: Could not allocate sector buffer, using default\n");
	    flash_sector_buffer = (uint8_t *) KERNADDR(CFG_FLASH_SECTOR_BUFFER_SIZE);
	    }
#else
	flash_sector_buffer = (uint8_t *) KERNADDR(CFG_FLASH_SECTOR_BUFFER_ADDR);
#endif
	}

    softc->fd_sectorbuffer = flash_sector_buffer;
}

/*  *********************************************************************
    *  flashdrv_probe(drv,probe_a,probe_b,probe_ptr)
    *  
    *  Device probe routine.  Attach the flash device to
    *  CFE's device table.
    *  
    *  Input parameters: 
    *  	   drv - driver descriptor
    *  	   probe_a - physical address of flash
    *  	   probe_b - size of flash (bytes)
    *  	   probe_ptr - unused
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flashdrv_probe(cfe_driver_t *drv,
			      unsigned long probe_a, unsigned long probe_b, 
			      void *probe_ptr)
{
    flashdev_t *softc;
    newflash_probe_t *probe;
    int idx;
    char descr[80];
    static int flashidx = 0;
    char *x;

    /*
     * First thing to do is configure the flashop engine
     * if not already done.  Basically we copy a bit of
     * position-independent code into the DRAM.
     */

    flash_setup_engine();

    /*
     * Now, on with the probing.
     */

    probe = (newflash_probe_t *) probe_ptr;

    /* 
     * probe_a is the flash base address
     * probe_b is the size of the flash
     * probe_ptr is unused.
     */

    softc = (flashdev_t *) KMALLOC(sizeof(flashdev_t),0);
    if (softc) {
	memset(softc,0,sizeof(flashdev_t));

	flashdrv_allocbuf(softc);

	if (probe) {
	    /* 
	     * Passed probe structure, do fancy stuff 
	     */
	    memcpy(&(softc->fd_probe),probe,sizeof(newflash_probe_t));
	    if (softc->fd_probe.flash_nchips == 0) {
		softc->fd_probe.flash_nchips = 1;
		}
	    }
	else {
	    /* Didn't pass probe structure, do the compatible thing */
	    softc->fd_probe.flash_phys = probe_a;
	    softc->fd_probe.flash_size = (probe_b & FLASH_SIZE_MASK);
	    softc->fd_probe.flash_flags = (probe_b & FLASH_FLG_MASK);
	    softc->fd_probe.flash_nchips = 1;
	    }

	if (softc->fd_probe.flash_flags & FLASH_FLG_MANUAL) {
	    /* Manual probing, just set the command set. */
	    flash_set_cmdset(softc,softc->fd_probe.flash_cmdset,
			     ((softc->fd_probe.flash_flags & FLASH_FLG_BUS16) ? 1 : 0),
			     ((softc->fd_probe.flash_flags & FLASH_FLG_DEV16) ? 1 : 0));
	    }
	else {
	    /* Do automatic probing */
#if (FLASH_DRIVERS & FLASH_DRIVER_CFI)
	    flash_do_probe(softc);
#else
	    return;		/* No automatic probing, bail! */
#endif
	    }

	/* Remember total size of all devices */
	softc->fd_ttlsize = softc->fd_probe.flash_nchips * softc->fd_probe.flash_size;

	/* Get the flash total size */
	probe->flash_size = softc->fd_ttlsize;

	/* Set description */
	x = descr;
	x += xsprintf(x,"%s at %08X size %uKB",drv->drv_description,
		 softc->fd_probe.flash_phys,
		 softc->fd_ttlsize/1024);
	if (softc->fd_probe.flash_nchips > 1) {
	    xsprintf(x," (%d chips)",softc->fd_probe.flash_nchips);
	    }

	/*
	 * If flash is not partitioned, just instantiate one
	 * device.   Otherwise, instantiate multiple flashes
	 * to cover the entire device.
	 */

	if (softc->fd_probe.flash_nparts == 0) {
	    softc->fd_parts[0].fp_dev = softc;
	    softc->fd_parts[0].fp_offset = 0;
	    softc->fd_parts[0].fp_size = softc->fd_probe.flash_size;
	    cfe_attach(drv,&(softc->fd_parts[0]),NULL,descr);
	    }
	else {
	    /*
	     * Partition flash into chunks 
	     */
	    flash_do_parts(softc);

	    /*
	     * Instantiate devices for each piece
	     */

	    for (idx = 0; idx < softc->fd_probe.flash_nparts; idx++) {
		char name[32];
		char *nptr;

		xsprintf(descr,"%s at %08X offset %08X size %uKB",
			 drv->drv_description,
			 softc->fd_probe.flash_phys,
			 softc->fd_parts[idx].fp_offset,
			 (softc->fd_parts[idx].fp_size+1023)/1024);

		softc->fd_parts[idx].fp_dev = softc;
		if (softc->fd_probe.flash_parts[idx].fp_name == NULL) {
		    sprintf(name,"%d",idx);
		    nptr = name;
		    }
		else {
		    nptr = softc->fd_probe.flash_parts[idx].fp_name;
		    }
		cfe_attach_idx(drv,
			       flashidx,
			       &(softc->fd_parts[idx]),
			       nptr,
			       descr);
		}
		flashidx++;
	    }


	/* Count total sectors on the device */

	softc->fd_ttlsect = 0;
	for (idx = 0; idx < softc->fd_probe.flash_nsectors; idx++) {
	    softc->fd_ttlsect += FLASH_SECTOR_NBLKS(softc->fd_probe.flash_sectors[idx]);
	    }

	}

}


/*  *********************************************************************
    *  flashdrv_open(ctx)
    *  
    *  Called when the flash device is opened.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0 if ok else error code
    ********************************************************************* */

static int flashdrv_open(cfe_devctx_t *ctx)
{
    flashpart_t *part = ctx->dev_softc;
    flashdev_t *softc = part->fp_dev;
    int ttlsect = softc->fd_ttlsect;

    /*
     * Calculate number of flashop instructions we'll need at most. 
     * This will be two for each sector plus two more for the first 
     * and last sectors, plus two extra
     */

    ttlsect = (ttlsect * 2 * softc->fd_probe.flash_nchips) + 6;

    /*
     * Allocate memory for instructions.
     */

#ifdef _NEWFLASH_DEBUG_
    printf("%s: allocating %d instructions\n",cfe_device_name(ctx),ttlsect);
#endif

    softc->fd_inst = KMALLOC(ttlsect*sizeof(flashinstr_t),0);
    if (!softc->fd_inst) return CFE_ERR_NOMEM;

    return 0;
}


/*  *********************************************************************
    *  flashdrv_read(ctx,buffer)
    *  
    *  Read data from the flash device.    The flash device is 
    *  considered to be like a disk (you need to specify the offset).
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - buffer descriptor
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int flashdrv_read(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    flashpart_t *part = ctx->dev_softc;
    flashdev_t *softc = part->fp_dev;
    int blen;
    int offset;

    blen = buffer->buf_length;
    offset = (long)buffer->buf_offset;

    if ((offset + blen) > part->fp_size) {
	blen = part->fp_size - offset;
	}

    offset += part->fp_offset;

    if (blen > 0) {
	flash_op_begin(softc);
	flash_op_add(softc,softc->fd_probe.flash_phys,
		     softc->fd_readfunc,(long)buffer->buf_ptr,offset,blen);
	flash_op_execute(softc);
	}

    buffer->buf_retlen = blen;

    return 0;
}

/*  *********************************************************************
    *  flashdrv_inpstat(ctx,inpstat)
    *  
    *  Return "input status".  For flash devices, we always return true.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   inpstat - input status structure
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int flashdrv_inpstat(cfe_devctx_t *ctx,iocb_inpstat_t *inpstat)
{
    inpstat->inp_status = 1;
    return 0;
}


/*  *********************************************************************
    *  flashdrv_write(ctx,buffer)
    *  
    *  Write data to the flash device.    The flash device is 
    *  considered to be like a disk (you need to specify the offset).
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - buffer descriptor
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int flashdrv_write2(cfe_devctx_t *ctx,iocb_buffer_t *buffer,int reboot)
{
    flashpart_t *part = ctx->dev_softc;
    flashdev_t *softc = part->fp_dev;
    int blen;
    int res;
    int offset;
    int whichchip;
    long chipbase;
    flash_range_t outrange;
    flash_range_t inrange;
    flash_sector_t sector;

    blen = buffer->buf_length;
    offset = (long)buffer->buf_offset;

    /* Compute range within physical flash */

    if ((offset + blen) > part->fp_size) {
	blen = part->fp_size - offset;
	}

    offset += part->fp_offset;

    /* Handle case of writing nothing */

    if (blen == 0) {
	buffer->buf_retlen = blen;
	return (buffer->buf_length == blen) ? 0 : CFE_ERR_IOERR;
	}

    /* now, offset/blen forms the range we want to write to. */

    inrange.range_base = offset;
    inrange.range_length = blen;

    sector.flash_sector_idx = 0;

    flash_op_begin(softc);

    for (;;) {
	res = flash_sector_query(softc,&sector);
	if (res != 0) break;
	if (sector.flash_sector_status == FLASH_SECTOR_INVALID) {
	    break;
	    }

	whichchip = sector.flash_sector_idx / softc->fd_ttlsect;
	chipbase  = softc->fd_probe.flash_phys + 
	    (long) (whichchip * softc->fd_probe.flash_size);

	res = flash_range_intersection(&sector,&inrange,&outrange);

	switch (res) {
	    case 1:		/* Erease/program entire sector */
#ifdef _NEWFLASH_DEBUG_
		    xprintf("Erease/program entire (0x%x) sector (0x%x @ 0x%x)\n",
		            outrange.range_length, sector.flash_sector_size,
		            sector.flash_sector_offset);
#endif /* _NEWFLASH_DEBUG_ */
		flash_op_add(softc,chipbase,
			     softc->fd_erasefunc,
			     sector.flash_sector_offset,
			     0,0);
		flash_op_add(softc,chipbase,
			     softc->fd_pgmfunc,
			     outrange.range_base,
			     ((long)buffer->buf_ptr)+(outrange.range_base-inrange.range_base),
			     outrange.range_length);
		break;

	    case 0:		/* Erase/reprogram partial sector */
#ifdef _NEWFLASH_DEBUG_
		    xprintf("Read/erease/program partial (0x%x) sector (0x%x @ 0x%x)\n",
		            outrange.range_length, sector.flash_sector_size,
		            sector.flash_sector_offset);
#endif /* _NEWFLASH_DEBUG_ */
		/* Save old sector */
		flash_op_add(softc,chipbase,
			     softc->fd_readfunc,
			     (long)(softc->fd_sectorbuffer),
			     sector.flash_sector_offset,
			     sector.flash_sector_size);
		/* Copy in new stuff */
		flash_op_add(softc,chipbase,
			     FEOP_MEMCPY,
			     ((long)(softc->fd_sectorbuffer))+(outrange.range_base-sector.flash_sector_offset),
			     ((long)buffer->buf_ptr)+(outrange.range_base-inrange.range_base),
			     outrange.range_length);
		/* Erase sector */
		flash_op_add(softc,chipbase,
			     softc->fd_erasefunc,
			     sector.flash_sector_offset,
			     0,0);
		/* Program sector */
		flash_op_add(softc,chipbase,
			     softc->fd_pgmfunc,
			     sector.flash_sector_offset,
			     (long)(softc->fd_sectorbuffer),
			     sector.flash_sector_size);
		break;

	    case -1:	   	/* No intersection */
		break;
	    }

	sector.flash_sector_idx++;

	}

    if (reboot) {
	flash_op_add(softc,softc->fd_probe.flash_phys,FEOP_REBOOT,0,0,0);
	}

    res = flash_op_execute(softc);

    buffer->buf_retlen = blen;

    return (res == 0) ? 0 : CFE_ERR_IOERR;
}

static int flashdrv_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    return flashdrv_write2(ctx,buffer,0);
}


/*  *********************************************************************
    *  flashdrv_ioctl(ctx,buffer)
    *  
    *  Handle special IOCTL functions for the flash.  Flash devices
    *  support NVRAM information, sector and chip erase, and a
    *  special IOCTL for updating the running copy of CFE.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   buffer - descriptor for IOCTL parameters
    *  	   
    *  Return value:
    *  	   0 if ok else error
    ********************************************************************* */
static int flashdrv_ioctl(cfe_devctx_t *ctx,iocb_buffer_t *buffer) 
{
    flashpart_t *part = ctx->dev_softc;
    flashdev_t *softc = part->fp_dev;
    nvram_info_t *nvinfo;
    flash_info_t *info;
    flash_range_t range;
    int offset;

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_NVRAM_GETINFO:
	    /* 
	     * We only support NVRAM on flashes that have been partitioned
	     * into at least two partitions.  Every partition supports
	     * being an NVRAM in that case, but we'll only attach one
	     * of them to the environment subsystem.
	     */
	    if (softc->fd_probe.flash_nparts <= 1) {
		return CFE_ERR_UNSUPPORTED;
		}
	    nvinfo = (nvram_info_t *) buffer->buf_ptr;
	    if (buffer->buf_length != sizeof(nvram_info_t)) return CFE_ERR_INV_PARAM;

	    nvinfo->nvram_offset = 0;
	    nvinfo->nvram_size = part->fp_size;
	    nvinfo->nvram_eraseflg = 1;
	    buffer->buf_retlen = sizeof(nvram_info_t);
	    return 0;
	    break;

	case IOCTL_FLASH_ERASE_SECTOR:
	    offset = (int) buffer->buf_offset;
	    offset += part->fp_offset;
	    if (offset >= softc->fd_probe.flash_size) return -1;

	    flash_op_begin(softc);
	    flash_op_add(softc,
			 softc->fd_probe.flash_phys,
			 softc->fd_erasefunc,
			 offset,
			 0,0);
	    flash_op_execute(softc);
	    return 0;

	case IOCTL_FLASH_ERASE_ALL:
	    offset = (int) buffer->buf_offset;
	    if (offset != 0) return -1;
	    flash_erase_all(softc);
	    return 0;

	case IOCTL_FLASH_WRITE_ALL:
	    /* Write file and reboot */
	    flashdrv_write2(ctx,buffer,1);
	    return -1;		/* should not return */

	case IOCTL_FLASH_GETINFO:
	    info = (flash_info_t *) buffer->buf_ptr;
	    info->flash_base = part->fp_offset;
	    info->flash_size = part->fp_size;
	    info->flash_type = softc->fd_probe.flash_type;
	    info->flash_flags = FLASH_FLAG_NOERASE;
	    if (flashop_engine_ram != (void *) flashop_engine_ptr)
		info->flash_flags |= FLASH_FLAG_INUSE;
	    return 0;

	case IOCTL_FLASH_GETSECTORS:
	    return flash_sector_query(softc,(flash_sector_t *) buffer->buf_ptr);

	case IOCTL_FLASH_ERASE_RANGE:
	    memcpy(&range,buffer->buf_ptr,sizeof(flash_range_t));
	    range.range_base += part->fp_offset;
	    if (range.range_length > part->fp_size) {
		range.range_length = part->fp_size;
		}
	    return flash_erase_range(softc,&range);

	default:
	    /* Call hook if present. */
	    if (softc->fd_probe.flash_ioctl_hook) {
		return (*(softc->fd_probe.flash_ioctl_hook))(ctx,buffer);
		}
	    return -1;
	}

    return -1;
}


/*  *********************************************************************
    *  flashdrv_close(ctx)
    *  
    *  Close the flash device.
    *  
    *  Input parameters: 
    *  	   ctx - device context
    *  	   
    *  Return value:
    *  	   0
    ********************************************************************* */
static int flashdrv_close(cfe_devctx_t *ctx)
{
    flashpart_t *part = ctx->dev_softc;
    flashdev_t *softc = part->fp_dev;

    if (softc->fd_inst) {
	KFREE(softc->fd_inst);
	}

    softc->fd_inst = NULL;


    return 0;
}
