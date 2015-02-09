/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Flash device driver			File: dev_flash.c
    *  (special version for production test board)
    *  
    *  This driver supports various types of flash
    *  parts.  You can also put the environment storage in
    *  the flash - the top sector is reserved for that purpose.
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
#include "sb1250_defs.h"
#include "lib_types.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "addrspace.h"
#include "cfe_iocb.h"
#include "cfe_console.h"
#include "cfe_device.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"

#include "dev_ptflash.h"
#include "cfe_timer.h"

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#ifdef __long64

/* When using processor in 64-bit mode, use XKSEG instead of KSEG1 */

#define FLASHCMD(sc,x,y) *((volatile unsigned char *) PHYS_TO_XKSEG_UNCACHED(\
                          ( (uintptr_t) ((sc)->flashdrv_cmdaddr+ \
                          (sc)->chip_select * (sc)->flashdrv_probe.chipsize + \
                          ((x)<<(sc)->flashdrv_widemode))))) = (y)

#define FLASHSTATUS(sc,x) *((volatile unsigned char *) PHYS_TO_XKSEG_UNCACHED(\
                          ((uintptr_t)((sc)->flashdrv_cmdaddr+ \
                          (sc)->chip_select * (sc)->flashdrv_probe.chipsize + \
                          ((x)<<(sc)->flashdrv_widemode)))))

#define WRITEFLASH_K1(sc,x,y) \
                          *((volatile unsigned char *) PHYS_TO_XKSEG_UNCACHED(\
                          ((uintptr_t) ((sc)->flashdrv_cmdaddr+(x))))) = (y)

#define READFLASH_K1(sc,x) *((volatile unsigned char *) PHYS_TO_XKSEG_UNCACHED(\
                          ((uintptr_t) ((sc)->flashdrv_cmdaddr+(x)))))

#define WRITEFLASH_K1W(sc,x,y) *((volatile unsigned short *) \
                          PHYS_TO_XKSEG_UNCACHED((uintptr_t) \
                          (sc->flashdrv_cmdaddr+(x)))) = (y)

#define READFLASH_K1W(sc,x) *((volatile unsigned short *) \
                          PHYS_TO_XKSEG_UNCACHED((uintptr_t) \
                          (sc->flashdrv_cmdaddr+(x))))

#else  /* 32-bit addressing */

#define FLASHCMD(sc,x,y) *((volatile unsigned char *)(sc->flashdrv_cmdaddr+ \
                          (sc)->chip_select * (sc)->flashdrv_probe.chipsize + \
                          ((x)<<(sc)->flashdrv_widemode))) = (y)
#define FLASHSTATUS(sc,x) *((volatile unsigned char *)(sc->flashdrv_cmdaddr+ \
                          (sc)->chip_select * (sc)->flashdrv_probe.chipsize + \
                          ((x)<<(sc)->flashdrv_widemode)))
#define WRITEFLASH_K1(sc,x,y) *((volatile unsigned char *)(sc->flashdrv_cmdaddr+(x))) = (y)
#define READFLASH_K1(sc,x) *((volatile unsigned char *)(sc->flashdrv_cmdaddr+(x)))

#define WRITEFLASH_K1W(sc,x,y) *((volatile unsigned short *)(sc->flashdrv_cmdaddr+(x))) = (y)
#define READFLASH_K1W(sc,x) *((volatile unsigned short *)(sc->flashdrv_cmdaddr+(x)))

#endif  /* __long64 */

#define GETCFIBYTE(softc,offset) READFLASH_K1(softc,((offset) << (softc->flashdrv_widemode)))

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

const static cfe_devdisp_t flashdrv_ro_dispatch = {
    flashdrv_open,
    flashdrv_read,
    NULL,
    NULL,
    NULL,
    flashdrv_close,	
    NULL,
    NULL
};

const cfe_driver_t ptflashdrv = {
    "CFI flash",
    "flash",
    CFE_DEV_FLASH,
    &flashdrv_dispatch,
    flashdrv_probe
};

const cfe_driver_t ptflashdrv_ro = {
    "Read only flash",
    "flashr",
    CFE_DEV_FLASH,
    &flashdrv_ro_dispatch,
    flashdrv_probe
};

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct flash_cfidata_s {
    unsigned int cfidata_cmdset;	/* ID of primary command set */
    unsigned int cfidata_devif;		/* device interface byte */
    unsigned int cfidata_size;		/* probed device size */
} flash_cfidata_t;

typedef struct flashops_s flashops_t;

typedef struct flashdrv_s {
    flash_probe_t flashdrv_probe;	/* data from probe */
    int flashdrv_devsize;		/* size reported by driver */
    unsigned char *flashdrv_cmdaddr;	/* virtual address (K1) */
    int flashdrv_widemode;		/* 1=wide flash in byte mode, 0=narrow flash */
    int flashdrv_initialized;		/* true if we've probed already */
    flash_info_t flashdrv_info;
    int flashdrv_nvram_ok;		/* true if we can use as NVRAM */
    int flashdrv_unlocked;		/* true if we can r/w past devsize */
    int chip_select;		        /* for multi-chip use */
    nvram_info_t flashdrv_nvraminfo;
    flashops_t *flashdrv_ops;
    flash_cfidata_t flashdrv_cfidata;
} flashdrv_t;

struct flashops_s {
    int (*erasesector)(flashdrv_t *f,int offset);
    int (*writeblk)(flashdrv_t *f,int offset,void *buf,int len);
    int (*protectsector)(flashdrv_t *f,int offset);
    int (*unprotectsector)(flashdrv_t *f,int offset);
};

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#define FLASHOP_ERASE_SECTOR(softc,sect) (*((softc)->flashdrv_ops->erasesector))((softc),(sect))
#define FLASHOP_WRITE_BLOCK(softc,off,buf,len) (*((softc)->flashdrv_ops->writeblk))((softc),(off),(buf),(len))
#define FLASHOP_PROTECT_SECTOR(softc,sect) (*((softc)->flashdrv_ops->protectsector))((softc),(sect))
#define FLASHOP_UNPROTECT_SECTOR(softc,sect) (*((softc)->flashdrv_ops->unprotectsector))((softc),(sect))

/*  *********************************************************************
    *  forward declarations
    ********************************************************************* */


static int flash_sector_query(flashdrv_t *softc,flash_sector_t *sector);

static int amd_flash_write_block(flashdrv_t *softc,int offset,void *buf,int len);
static int amd_flash_erase_sector(flashdrv_t *softc,int offset);

static int hyundai_flash_write_block(flashdrv_t *softc,int offset,void *buf,int len);
static int hyundai_flash_erase_sector(flashdrv_t *softc,int offset);

static int hyundai_flash_protect_sector(flashdrv_t *softc,int offset);
static int hyundai_flash_unprotect_sector(flashdrv_t *softc,int offset);

static int intel_flash_write_block(flashdrv_t *softc,int offset,void *buf,int len);
static int intel_flash_erase_sector(flashdrv_t *softc,int offset);

static flashops_t amd_flashops = {
    amd_flash_erase_sector,
    amd_flash_write_block,
    NULL,
    NULL,
};

static flashops_t hyundai_flashops = {
    hyundai_flash_erase_sector,
    hyundai_flash_write_block,
    hyundai_flash_protect_sector,
    hyundai_flash_unprotect_sector,
};

static flashops_t intel_flashops = {
    intel_flash_erase_sector,
    intel_flash_write_block,
    NULL,
    NULL,
};

#define FLASHOPS_DEFAULT amd_flashops



/*  *********************************************************************
    *  Externs
    ********************************************************************* */

extern void *flash_write_all_ptr;
extern int flash_write_all_len;

extern void _cfe_flushcache(int);

/*  *********************************************************************
    *  jedec_flash_maufacturer(softc)
    *  
    *  Return the manufacturer ID for this flash part.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static unsigned int jedec_flash_manufacturer(flashdrv_t *softc) 
{
     unsigned int res;

    /* Do an "unlock write" sequence */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1);
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2);
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_AUTOSEL);

    res = FLASHSTATUS(softc,FLASH_JEDEC_OFFSET_MFR) & 0xFF;

    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_RESET);

    return res;
}


/*  *********************************************************************
    *  amd_flash_write_byte(softc,offset,val)
    *  
    *  Write a single byte to the flash.  The sector that the flash
    *  byte is in should have been previously erased, or else this
    *  routine may hang.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   offset - distance in bytes into the flash
    *  	   val - byte to write
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else if flash could not be written
    ********************************************************************* */

static inline int amd_flash_write_byte(flashdrv_t *softc,int offset, unsigned char val) 
{
    unsigned int value;

    if (softc->flashdrv_probe.nchips > 1)
        softc->chip_select = offset / softc->flashdrv_probe.chipsize;
    else
        softc->chip_select = 0;

    /* Do an "unlock write" sequence */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1);
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2);

    /* Send a program command */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_PROGRAM);

    /* Restore chip select to default value */

    softc->chip_select = 0;

    /* Write a byte */
    WRITEFLASH_K1(softc,offset,val);

    for (;;) {
	value = READFLASH_K1(softc,offset) & 0xFF;
	
	if ((value & 0x80) == (val & 0x80)) {
	    return 0;
	    }
	if ((value & 0x20) != 0x20) {
	    continue; 
	    }

	if ((READFLASH_K1(softc,offset) & 0x80) == (val & 0x80)) {
	    return 0; 
	    } 
	else {
	    return -1;
	    }
	}
}
/*  *********************************************************************
    *  hyundai_flash_write_byte_flash(softc,offset,val)
    *  
    *  Write a single byte to the flash.  The sector that the flash
    *  byte is in should have been previously erased, or else this
    *  routine may hang.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   offset - distance in bytes into the flash
    *  	   val - byte to write
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else if flash could not be written
    ********************************************************************* */

static inline int hyundai_flash_write_byte_fast(flashdrv_t *softc,int offset, unsigned char val) 
{
    unsigned char toggle;
    unsigned char status;

    /* Send a fast program command */

    WRITEFLASH_K1(softc,offset,AMD_FLASH_PROGRAM);

    /* Write a byte */

    WRITEFLASH_K1(softc,offset,val);

    /* Wait for write completion (toggle bit stops toggling) */

    toggle = (READFLASH_K1(softc,offset) & 0x40);
    while (1)  {
        status = READFLASH_K1(softc,offset);

        if ((status & 0x40) == toggle)
            break;
         
        toggle ^= 0x40;
    }

    /* In fast write mode we can't do a read check, so assume data OK */

    return 0;
}


/*  *********************************************************************
    *  hyundai_flash_write_block(softc,offset,val)
    *  
    *  Write a byte stream to the flash.  The sector that the flash
    *  byte is in should have been previously erased, or else this
    *  routine may hang.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   offset - distance in bytes into the flash
    *  	   buf - buffer of bytes to write
    *	   len - number of bytes to write
    *  	   
    *  Return value:
    *  	   number of bytes written
    ********************************************************************* */
static int hyundai_flash_write_block(flashdrv_t *softc,int offset,void *buf,int len)
{
    unsigned char *ptr;
    int chipsize;
    int chip_unlocked = 1;


    /* Use unlock bypass so that byte write takes fewer ops */


    chipsize = softc->flashdrv_probe.chipsize;

    if (softc->flashdrv_probe.nchips > 1)
        softc->chip_select = offset / chipsize;
    else
        softc->chip_select = 0;

    chipsize -= offset;

    ptr = buf;

    while (len) {
        if (chip_unlocked) {
            /* First an "unlock write" sequence */
            FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1);
            FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2);

            /* Send an unlock bypass command */
            FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_UNLOCK_BYPASS);
            chip_unlocked = 0;
        }
	if (hyundai_flash_write_byte_fast(softc,offset,*ptr) < 0) break;
	len--;
	ptr++;
	offset++;
	chipsize--;
        /* See if write crosses into next flash chip */
        if (len && (chipsize == 0))
            {
            if (softc->chip_select == (softc->flashdrv_probe.nchips - 1))
                break;
            /* Bypass reset (restore current flash to read mode) */
            WRITEFLASH_K1(softc,0,0x90);
            WRITEFLASH_K1(softc,0,0x00);
            softc->chip_select++;
            chip_unlocked = 1;
            chipsize = softc->flashdrv_probe.chipsize;
            }
	}

    if (!chip_unlocked) {
        WRITEFLASH_K1(softc,0,0x90);
        WRITEFLASH_K1(softc,0,0x00);
    }

    return (ptr - (unsigned char *)buf);
}
/*  *********************************************************************
    *  amd_flash_write_block(softc,offset,val)
    *  
    *  Write a single byte to the flash.  The sector that the flash
    *  byte is in should have been previously erased, or else this
    *  routine may hang.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   offset - distance in bytes into the flash
    *  	   buf - buffer of bytes to write
    *	   len - number of bytes to write
    *  	   
    *  Return value:
    *  	   number of bytes written
    ********************************************************************* */
static int amd_flash_write_block(flashdrv_t *softc,int offset,void *buf,int len)
{
    unsigned char *ptr;

    ptr = buf;

    while (len) {
	if (amd_flash_write_byte(softc,offset,*ptr) < 0) break;
	len--;
	ptr++;
	offset++;
	}

    return (ptr - (unsigned char *)buf);
}


/*  *********************************************************************
    *  amd_flash_erase_sector(softc,offset)
    *  
    *  Erase a single sector in the flash device
    *  
    *  Input parameters: 
    *  	   softc - device context
    *  	   offset - offset in flash of sector to erase
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int amd_flash_erase_sector(flashdrv_t *softc,int offset)
{

    if (softc->flashdrv_probe.nchips > 1)
        softc->chip_select = offset / softc->flashdrv_probe.chipsize;
    else
        softc->chip_select = 0;

    /* Do an "unlock write" sequence */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1);	/* cycles 1-2 */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2);

    /* send the erase command */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_ERASE_3);	/* cycle  3 */

    /* Do an "unlock write" sequence */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1);	/* cycles 4-5 */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2);

    /* Restore chip select to default value */

    softc->chip_select = 0;
    /* 
     * Send the "erase sector" qualifier - don't use FLASHCMD
     * because it changes the offset.
     */
    WRITEFLASH_K1(softc,offset,AMD_FLASH_ERASE_SEC_6);

    while ((READFLASH_K1(softc,offset) & 0x80) != 0x80) {
	/* NULL LOOP */
	}

    return 0;
}

/*  *********************************************************************
    *  hyundai_flash_erase_sector(softc,offset)
    *  
    *  Erase a single sector in the flash device
    *  
    *  Input parameters: 
    *  	   softc - device context
    *  	   offset - offset in flash of sector to erase
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int hyundai_flash_erase_sector(flashdrv_t *softc,int offset)
{
    uint8_t toggle;
    uint8_t status;
    unsigned int count = 0;

    if (softc->flashdrv_probe.nchips > 1)
        softc->chip_select = offset / softc->flashdrv_probe.chipsize;
    else
        softc->chip_select = 0;

    /* Do an "unlock write" sequence */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1);	/* cycles 1-2 */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2);

    /* send the erase command */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_ERASE_3);	/* cycle  3 */

    /* Do an "unlock write" sequence */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_1,AMD_FLASH_MAGIC_1);	/* cycles 4-5 */
    FLASHCMD(softc,AMD_FLASH_MAGIC_ADDR_2,AMD_FLASH_MAGIC_2);

    /* Restore chip select to default value */

    softc->chip_select = 0;
    /* 
     * Send the "erase sector" qualifier - don't use FLASHCMD
     * because it changes the offset.
     */
    WRITEFLASH_K1(softc,offset,AMD_FLASH_ERASE_SEC_6);

    toggle = (READFLASH_K1(softc,offset) & 0x40);
    while (1)  {
        status = READFLASH_K1(softc,offset);
        if ((status & 0x40) == toggle)
            break;
        toggle ^= 0x40;
        if ((status & 0x20) == 0x20) {
            if ((READFLASH_K1(softc,offset) & 0x40) == toggle) {
                break;  /* no toggle, erase complete */
            }
            else {
                xprintf ("sector erase timed out\n");
                break;
            }
        }
        count++;
    }

    return 0;
}

/*  *********************************************************************
    *  hyundai_flash_protect_sector(softc,offset)
    *  
    *  Protect a sectors in the flash device
    *  
    *  Input parameters: 
    *  	   softc - device context
    *  	   offset - offset of sector to protect
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int hyundai_flash_protect_sector(flashdrv_t *softc, int offset)
{
    int chipsize = softc->flashdrv_probe.chipsize;
    int trycnt = 25;
    offset &= ~(0xff);			/* Clear least significant byte */

xprintf("protect sector at offset 0x%x\n");

    /* Address bit 1 must be set.  */
    /* Note:  in byte mode, address bit 1 is left shifted by 1 */

    if (softc->flashdrv_widemode)
        offset |= 0x4;
    else
        offset |= 0x2;

    /* Get device ready */

    WRITEFLASH_K1(softc, chipsize*(offset/chipsize), 0x60);
    
xprintf ("protect!, offset = 0x%x\n",offset);
    WRITEFLASH_K1(softc,offset,0x60);			/* Protect CMD 1 */
    cfe_usleep (150);					/* Wait 150 usec */
    WRITEFLASH_K1(softc,offset,0x40);			/* Protect CMD 2 */

    while (READFLASH_K1(softc,offset) != 0x01 && trycnt) {
        WRITEFLASH_K1(softc,offset,0x60);		/* Protect CMD 1 */
        cfe_usleep (150);				/* Wait 150 usec */
        WRITEFLASH_K1(softc,offset,0x40);		/* Protect CMD 2 */
        trycnt--;
	}

    if (trycnt == 0 && READFLASH_K1(softc,offset) != 0x01)
        {
xprintf ("protect! failed\n");
        return (1);
        }
    else
        {
xprintf ("protect! succeeded trycnt = 0x%x\n",trycnt);
        return (0);
        }
}

/*  *********************************************************************
    *  flash_protect_range(softc,range)
    *  
    *  Protect a range of sectors in the flash device
    *  
    *  Input parameters: 
    *  	   softc - device context
    *  	   ranget - range of bytes to protect
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int flash_protect_range(flashdrv_t *softc, flash_range_t *range)
{
    int status;
    flash_sector_t sector;
    int soffset;
    int res;
    int i;

    if (((softc)->flashdrv_ops->protectsector) == NULL)
        {
        return CFE_ERR_UNSUPPORTED;
        }

    if ((softc)->flashdrv_ops == &hyundai_flashops) {

        xprintf ("\n *** Apply high voltage to flash, then hit any key (or q to quit):");
        if (console_readkey() == 'q') {
            xprintf ("\n");
            return 0;
        }

        xprintf ("\n");

    }

    sector.flash_sector_idx = 0;


    for (;;) {
        res = flash_sector_query(softc,&sector);
        if (res != 0) break;
        if (sector.flash_sector_status == FLASH_SECTOR_INVALID) {
            break;
            }

        soffset = sector.flash_sector_offset;
        if ((soffset >= range->range_base) &&
            (soffset < (range->range_base+range->range_length-1))) {
            res = FLASHOP_PROTECT_SECTOR(softc,soffset);
            if (res != 0) break;
            }
        else if ((soffset < range->range_base) &&
            (soffset + sector.flash_sector_size > range->range_base)) {
            res = FLASHOP_PROTECT_SECTOR(softc,soffset);
            if (res != 0) break;
        }
        else if (soffset >= range->range_base + range->range_length) {
            res = 0;
            break;
        }
        sector.flash_sector_idx++;
    }
    status = res;

    /* Now that we've done all the protecting, send a reset to every flash */

    if ((softc)->flashdrv_ops == &hyundai_flashops) {

        xprintf ("\n *** Restore normal voltage to flash, then hit any key:");
        console_readkey();
        xprintf ("\n");

        /* 
        * Even if every flash chip didn't get put in protect mode, it doesn't
        * hurt to reset...
        */
        for (i = 0; i < softc->flashdrv_probe.nchips; i++) 
            WRITEFLASH_K1(softc,i*softc->flashdrv_probe.chipsize, AMD_FLASH_RESET);
    }

    return status;
}

/*  *********************************************************************
    *  hyundai_flash_unprotect_sector(softc,offset)
    *  
    *  Unprotect a sectors in the flash device
    *  
    *  Input parameters: 
    *  	   softc - device context
    *  	   range - offset of sector to unprotect
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int hyundai_flash_unprotect_sector(flashdrv_t *softc, int offset)
{
    int trycnt = 1000;
    int chipsize = softc->flashdrv_probe.chipsize;

    offset &= ~(0xff);			/* Clear least significant byte */

    /* Address bits 6 and 1 must be set.  */
    /* Note:  in byte mode, address bits left shifted by 1 */

    if (softc->flashdrv_widemode)
        offset |= 0x84;
    else
        offset |= 0x42;

    /* Get device ready */

    if (softc->flashdrv_probe.nchips > 1)
        WRITEFLASH_K1(softc, chipsize*(offset/chipsize), 0x60);
    else
        WRITEFLASH_K1(softc,0x0,0x60);			/* Get device ready */
    
xprintf ("unprotect!, offset = 0x%x\n",offset);
    WRITEFLASH_K1(softc,offset,0x60);			/* Unprotect CMD 1 */
    cfe_usleep (15000);					/* Wait 15 msec */
    WRITEFLASH_K1(softc,offset,0x40);			/* Unprotect CMD 2 */

    while (READFLASH_K1(softc,offset) != 0x00 && trycnt) {
        trycnt--;
        WRITEFLASH_K1(softc,offset,0x60);		/* Unprotect CMD 1 */
        cfe_usleep (15000);				/* Wait 15 msec */
        WRITEFLASH_K1(softc,offset,0x40);		/* Unprotect CMD 2 */
	}

    if (trycnt == 0 && READFLASH_K1(softc,offset) != 0x00)
        {
xprintf ("protect! failed\n");
        return 1;
        }
    else
        {
xprintf ("protect! succeeded trycnt = 0x%x\n",trycnt);
        return 0;
        }
}
/*  *********************************************************************
    *  flash_unprotect_range(softc,range)
    *  
    *  Unprotect a range of sectors in the flash device
    *  
    *  Input parameters: 
    *  	   softc - device context
    *  	   range - range of bytes to unprotect
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */

static int flash_unprotect_range(flashdrv_t *softc, flash_range_t *range)
{
    int status;
    flash_sector_t sector;
    int soffset;
    int res;
    int i;

    if (((softc)->flashdrv_ops->unprotectsector) == NULL)
        {
        return CFE_ERR_UNSUPPORTED;
        }

    if ((softc)->flashdrv_ops == &hyundai_flashops) {

        xprintf ("\n *** Apply high voltage to flash, then hit any key (or q to quit):");
        if (console_readkey() == 'q') {
            xprintf ("\n");
            return 0;
        }

        xprintf ("\n");
    }

    sector.flash_sector_idx = 0;



    for (;;) {
        res = flash_sector_query(softc,&sector);
        if (res != 0) break;
        if (sector.flash_sector_status == FLASH_SECTOR_INVALID) {
            break;
            }

        soffset = sector.flash_sector_offset;
        if ((soffset >= range->range_base) &&
            (soffset < (range->range_base+range->range_length-1))) {
            res = FLASHOP_UNPROTECT_SECTOR(softc,soffset);
            if (res != 0) break;
            }
        else if ((soffset < range->range_base) &&
            (soffset + sector.flash_sector_size > range->range_base)) {
            res = FLASHOP_UNPROTECT_SECTOR(softc,soffset);
            if (res != 0) break;
        }
        else if (soffset >= range->range_base + range->range_length) {
            res = 0;
            break;
        }
        sector.flash_sector_idx++;
    }
    status = res;

    /* Now that we've done the unprotecting, send a reset to every flash */

    if ((softc)->flashdrv_ops == &hyundai_flashops) {

        xprintf ("\n *** Restore normal voltage to flash, then hit any key:");
        console_readkey();
        xprintf ("\n");
        for (i = 0; i < softc->flashdrv_probe.nchips; i++)
            {
            WRITEFLASH_K1(softc,i*softc->flashdrv_probe.chipsize, AMD_FLASH_RESET);
            }
    }

    return status;
}

/*  *********************************************************************
    *  intel_flash_write_byte(softc,offset,val)
    *  
    *  Write a single byte to the flash.  The sector that the flash
    *  byte is in should have been previously erased, or else this
    *  routine may hang.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   offset - distance in bytes into the flash
    *  	   val - byte to write
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else if flash could not be written
    ********************************************************************* */
static inline int intel_flash_write_byte(flashdrv_t *softc,
				       int offset, unsigned char val) 
{
    unsigned int value;

    /* Send a program command */
    WRITEFLASH_K1(softc,offset,INTEL_FLASH_PROGRAM);

    /* Write a byte */
    WRITEFLASH_K1(softc,offset,val);


    while ((READFLASH_K1(softc,offset) & 0x80) != 0x80) {
	/* NULL LOOP */
	}

    value = READFLASH_K1(softc,offset) & 0xFF;

    if (value & (0x01|0x08|0x10)) return -1;
    return 0;
}

/*  *********************************************************************
    *  intel_flash_write_word(softc,offset,val)
    *  
    *  Write a single word to the flash.  The sector that the flash
    *  byte is in should have been previously erased, or else this
    *  routine may hang.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   offset - distance in bytes into the flash
    *  	   val - word to write
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else if flash could not be written
    ********************************************************************* */
static inline int intel_flash_write_word(flashdrv_t *softc,
				       int offset, unsigned short val) 
{
    unsigned int value;


    /* Send a program command */
    WRITEFLASH_K1W(softc,offset,INTEL_FLASH_PROGRAM);

    /* Write a byte */
    WRITEFLASH_K1W(softc,offset,val);


    while ((READFLASH_K1W(softc,offset) & 0x80) != 0x80) {
	/* NULL LOOP */
	}

    value = READFLASH_K1W(softc,offset) & 0xFF;

    if (value & (0x01|0x08|0x10)) return -1;
    return 0;
}

/*  *********************************************************************
    *  intel_flash_write_block(softc,offset,val)
    *  
    *  Write a single byte to the flash.  The sector that the flash
    *  byte is in should have been previously erased, or else this
    *  routine may hang.
    *  
    *  Input parameters: 
    *  	   softc - flash context
    *  	   offset - distance in bytes into the flash
    *  	   buf - buffer of bytes to write
    *	   len - number of bytes to write
    *  	   
    *  Return value:
    *  	   number of bytes written
    ********************************************************************* */
static int intel_flash_write_block(flashdrv_t *softc,int offset,void *buf,int len)
{
    unsigned char *ptr;
    unsigned short *ptrw;

    if (softc->flashdrv_probe.flash_flags & FLASH_FLG_16BIT) {
	ptrw = buf;
	offset &= ~1;		/* offset must be even */
	while (len > 0) {
	    if (intel_flash_write_word(softc,offset,*ptrw) < 0) break;
	    len-=2;
	    ptrw++;
	    offset+=2;
	    }
	WRITEFLASH_K1(softc,offset,INTEL_FLASH_READ_MODE);
	return ((unsigned char *) ptrw - (unsigned char *)buf);
	}
    else {
	ptr = buf;
	while (len) {
	    if (intel_flash_write_byte(softc,offset,*ptr) < 0) break;
	    len--;
	    ptr++;
	    offset++;
	    }
	WRITEFLASH_K1(softc,offset,INTEL_FLASH_READ_MODE);
	return (ptr - (unsigned char *)buf);
	}

}


/*  *********************************************************************
    *  intel_flash_erase_sector(softc,offset)
    *  
    *  Erase a single sector on the flash device
    *  
    *  Input parameters: 
    *  	   softc - device context
    *  	   offset - offset in flash of sector to erase
    *  	   
    *  Return value:
    *  	   0 if ok, else error code
    ********************************************************************* */
static int intel_flash_erase_sector(flashdrv_t *softc,int offset)
{
    WRITEFLASH_K1(softc,offset,INTEL_FLASH_ERASE_BLOCK);
    WRITEFLASH_K1(softc,offset,INTEL_FLASH_ERASE_CONFIRM);

    while ((READFLASH_K1(softc,offset) & 0x80) != 0x80) {
	/* NULL LOOP */
	}
    WRITEFLASH_K1(softc,offset,INTEL_FLASH_READ_MODE);

    return 0;
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

static int flash_erase_range(flashdrv_t *softc,flash_range_t *range)
{
    flash_sector_t sector;
    int soffset;
    int res;

    if (softc->flashdrv_info.flash_type != FLASH_TYPE_FLASH) {
	return CFE_ERR_UNSUPPORTED;
	}


    if (range->range_base+range->range_length > softc->flashdrv_devsize) {
	return CFE_ERR_INV_PARAM;
	}

    res = 0;

    sector.flash_sector_idx = 0;
    softc->chip_select = range->range_base / softc->flashdrv_probe.chipsize;

    for (;;) {
	res = flash_sector_query(softc,&sector);
	if (res != 0) break;
	if (sector.flash_sector_status == FLASH_SECTOR_INVALID) {
	    break;
	    }

        soffset = sector.flash_sector_offset;
	if ((soffset >= range->range_base) && 
            (soffset < (range->range_base+range->range_length-1))) {
	    if (softc->flashdrv_nvram_ok && 
		(soffset >= softc->flashdrv_nvraminfo.nvram_offset)) {
		break;
		}
	    res = FLASHOP_ERASE_SECTOR(softc,soffset);
	    if (res != 0) break;
	    }
        else if ((soffset < range->range_base) &&
            (soffset + sector.flash_sector_size > range->range_base)) {
	    if (softc->flashdrv_nvram_ok && 
		(soffset >= softc->flashdrv_nvraminfo.nvram_offset)) {
		break;
		}
	    res = FLASHOP_ERASE_SECTOR(softc,soffset);
	    if (res != 0) break;
	}
        else if (soffset >= range->range_base + range->range_length) { 
            res = 0;
            break;
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

static int flash_erase_all(flashdrv_t *softc)
{
    flash_range_t range;
    int i;


    if (softc->flashdrv_probe.nchips == 1)
        {
        range.range_base = 0;
        range.range_length = softc->flashdrv_devsize;

        return flash_erase_range(softc,&range);
        }
    else
        {
        for (i = 0; i < softc->flashdrv_probe.nchips; i++)
            {
            range.range_base = i*softc->flashdrv_probe.chipsize;
            range.range_length = softc->flashdrv_probe.chipsize;
            flash_erase_range(softc,&range);
            }
        return 0;
        }
}

/*  *********************************************************************
    *  FLASH_CFI_GETSECTORS(softc)
    *  
    *  Query the CFI information and store the sector info in our 
    *  private probe structure.
    *  
    *  Input parameters: 
    *  	   softc - our flash info
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int flash_cfi_getsectors(flashdrv_t *softc)
{
    int idx;
    int regcnt;
    int nblks;
    int blksiz;

    regcnt = GETCFIBYTE(softc,FLASH_CFI_REGION_COUNT);

    softc->flashdrv_probe.flash_nsectors = regcnt;

    for (idx = 0; idx < regcnt; idx++) {
	nblks = ((int)GETCFIBYTE(softc,FLASH_CFI_REGION_TABLE+0+idx*4) + 
		 (int)(GETCFIBYTE(softc,FLASH_CFI_REGION_TABLE+1+idx*4)<<8)) + 1;
	blksiz = ((int)GETCFIBYTE(softc,FLASH_CFI_REGION_TABLE+2+idx*4) + 
		  (int)(GETCFIBYTE(softc,FLASH_CFI_REGION_TABLE+3+idx*4)<<8)) * 256;
	softc->flashdrv_probe.flash_sectors[idx] = 
	    FLASH_SECTOR_RANGE(nblks,blksiz);
	}
    

    return 0;
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

static int flash_sector_query(flashdrv_t *softc,flash_sector_t *sector)
{
    int idx = 0;
    int nblks;
    int blksiz;
    unsigned int offset;
    int curblk;
    int nchip;

    if (softc->flashdrv_info.flash_type != FLASH_TYPE_FLASH) {
	return CFE_ERR_UNSUPPORTED;
	}	

    if (softc->flashdrv_probe.flash_nsectors == 0) {
	return CFE_ERR_UNSUPPORTED;
	}

    offset = 0;
    curblk = 0;
    for (nchip = 0; nchip < softc->flashdrv_probe.nchips; nchip++) {
        for (idx = 0; idx < softc->flashdrv_probe.flash_nsectors; idx++) {
	nblks = FLASH_SECTOR_NBLKS(softc->flashdrv_probe.flash_sectors[idx]);
	blksiz = FLASH_SECTOR_SIZE(softc->flashdrv_probe.flash_sectors[idx]);
	if (sector->flash_sector_idx < curblk+nblks) {
	    sector->flash_sector_status = FLASH_SECTOR_OK;
	    sector->flash_sector_offset = 
		offset + (sector->flash_sector_idx-curblk)*blksiz;
	    sector->flash_sector_size = blksiz;
            goto done;
	    }

	offset += (nblks)*blksiz;
	curblk += nblks;
	}
    }
    
done:

    if (idx == softc->flashdrv_probe.flash_nsectors) {
	sector->flash_sector_status = FLASH_SECTOR_INVALID;
	}

    return 0;
}

/*  *********************************************************************
    *  FLASH_SET_CMDSET(softc,cmdset)
    *  
    *  Set the command-set that we'll honor for this flash.
    *  
    *  Input parameters: 
    *  	   softc - our flash
    *  	   cmdset - FLASH_CFI_CMDSET_xxx
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flash_set_cmdset(flashdrv_t *softc,int cmdset)
{
    switch (cmdset) {
	case FLASH_CFI_CMDSET_INTEL_ECS:
	case FLASH_CFI_CMDSET_INTEL_STD:
	    softc->flashdrv_ops = &intel_flashops;
	    softc->flashdrv_widemode = 0;
	    break;
	case FLASH_CFI_CMDSET_AMD_STD:
            if (jedec_flash_manufacturer(softc) == FLASH_MFR_HYUNDAI)
                {
	        softc->flashdrv_ops = &hyundai_flashops;
                break;
                }
	case FLASH_CFI_CMDSET_AMD_ECS:
	    softc->flashdrv_ops = &amd_flashops;
	    break;
	default:
	    /* we don't understand the command set - treat it like ROM */
	    softc->flashdrv_info.flash_type = FLASH_TYPE_ROM;
	}
}


/*  *********************************************************************
    *  FLASH_CFI_PROBE(softc)
    *  
    *  Try to do a CFI query on this device.  If we find the 
    *  magic signature, extract some useful information from the
    *  query structure.
    *  
    *  Input parameters: 
    *  	   softc - out flash
    *  	   
    *  Return value:
    *  	   0 if successful, <0 if error
    ********************************************************************* */
static int flash_cfi_probe(flashdrv_t *softc)
{
    softc->chip_select = 0;
    FLASHCMD(softc,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_MODE);

    if (!((GETCFIBYTE(softc,FLASH_CFI_SIGNATURE+0) == 'Q') &&
	 (GETCFIBYTE(softc,FLASH_CFI_SIGNATURE+1) == 'R') &&
	 (GETCFIBYTE(softc,FLASH_CFI_SIGNATURE+2) == 'Y'))) {

	FLASHCMD(softc,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_EXIT);
	return CFE_ERR_UNSUPPORTED;
	}

    /* 
     * Gather info from flash 
     */

    softc->flashdrv_cfidata.cfidata_cmdset = 
	((unsigned int) (GETCFIBYTE(softc,FLASH_CFI_COMMAND_SET))) +
	(((unsigned int) (GETCFIBYTE(softc,FLASH_CFI_COMMAND_SET+1))) << 8);

    softc->flashdrv_cfidata.cfidata_devif = 
	((unsigned int) (GETCFIBYTE(softc,FLASH_CFI_DEVICE_INTERFACE))) +
	(((unsigned int) (GETCFIBYTE(softc,FLASH_CFI_DEVICE_INTERFACE+1))) << 8);

    softc->flashdrv_cfidata.cfidata_size =
	1 << ((unsigned int) (GETCFIBYTE(softc,FLASH_CFI_DEVICE_SIZE)));

    flash_cfi_getsectors(softc);

    /*
     * Don't need to be in query mode anymore.
     */

    FLASHCMD(softc,FLASH_CFI_QUERY_ADDR,FLASH_CFI_QUERY_EXIT);

    softc->flashdrv_info.flash_type = FLASH_TYPE_FLASH;

    flash_set_cmdset(softc,softc->flashdrv_cfidata.cfidata_cmdset);

    return 0;

}

/*  *********************************************************************
    *  FLASH_GETWIDTH(softc,info)
    *  
    *  Try to determine the width of the flash.  This is needed for
    *  management purposes, since some 16-bit flash parts in 8-bit mode
    *  have an "A-1" (address line -1) wire to select bytes within
    *  a 16-bit word.  When this is present, the flash commands
    *  will have different offsets.
    * 
    *  Input parameters: 
    *  	   softc - our flash
    *      info - flash info structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flash_getwidth(flashdrv_t *softc,flash_info_t *info)
{
    softc->flashdrv_widemode = 0;		/* first try narrow */

    if (flash_cfi_probe(softc) == 0) {
	return;
	}

    softc->flashdrv_widemode = 1;		/* then wide */

    if (flash_cfi_probe(softc) == 0) {
	return;
	}

    /* Just return, assume not wide if no CFI interface */
    softc->flashdrv_widemode = 0;

    softc->flashdrv_info.flash_type = FLASH_TYPE_ROM;	/* no CFI: treat as ROM */
}

/*  *********************************************************************
    *  flash_getinfo(softc)
    *  
    *  Try to determine if the specified region is flash, ROM, SRAM,
    *  or something else.
    *  
    *  Input parameters: 
    *  	   softc - our context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void flash_getinfo(flashdrv_t *softc)
{
    uint8_t save0,save1;
    volatile uint8_t *ptr;
    flash_info_t *info = &(softc->flashdrv_info);

    /*
     * Set up some defaults based on the probe data
     */

    softc->flashdrv_widemode = 0;
    info->flash_base = softc->flashdrv_probe.flash_phys;
    info->flash_size = softc->flashdrv_probe.flash_size;
    softc->flashdrv_devsize = softc->flashdrv_probe.flash_size;
    info->flash_type = FLASH_TYPE_UNKNOWN;
    info->flash_flags = 0;

    /*
     * If we've been told not to try probing, just assume
     * we're a flash part.
     */

    if (softc->flashdrv_probe.flash_flags & FLASH_FLG_MANUAL) {
	info->flash_type = FLASH_TYPE_FLASH;
	if  (softc->flashdrv_probe.flash_flags & FLASH_FLG_WIDE) {
	    softc->flashdrv_widemode = TRUE;
	    }
	if (softc->flashdrv_probe.flash_cmdset) {
	    flash_set_cmdset(softc,softc->flashdrv_probe.flash_cmdset);
	    }
	return;
	}

    /*
     * Attempt to read/write byte zero.  If it is changable,
     * this is SRAM (or maybe a ROM emulator with the write line hooked up)
     */

    ptr = (volatile uint8_t *) UNCADDR(softc->flashdrv_probe.flash_phys);
    save0 = *ptr;			/* save old value */
    save1 = *(ptr+1);			/* save old value */
    *(ptr) = 0x55; 
    if ((*ptr) == 0x55) {
	*(ptr) = 0xAA;
	if ((*ptr) == 0xAA) {
	    info->flash_type = FLASH_TYPE_SRAM;
	    }
	}

    if (*ptr == save0) info->flash_type = FLASH_TYPE_ROM;
    else (*ptr) = save0;		/* restore old value */

    /* 
     * If we thought it was ROM, try doing a CFI query
     * to see if it was flash.  This check is kind of kludgey
     * but should work.
     */

    if (info->flash_type == FLASH_TYPE_ROM) {
	flash_getwidth(softc,info);
	if (info->flash_type == FLASH_TYPE_FLASH) {
	    }
	}
}

/*  *********************************************************************
    *  flashdrv_setup_nvram(softc)
    *  
    *  If we're going to be using a sector of the flash for NVRAM, 
    *  go find that sector and set it up.
    *  
    *  Input parameters: 
    *  	   softc - our flash
    *  	   
    *  Return value:
    *  	   nothing.  flashdrv_nvram_ok might change though.
    ********************************************************************* */

static void flashdrv_setup_nvram(flashdrv_t *softc)
{
    flash_sector_t sector;
    int res;

    softc->flashdrv_nvram_ok = FALSE;

    if (softc->flashdrv_info.flash_type != FLASH_TYPE_FLASH) {
	return;
	}

    sector.flash_sector_idx = 0;
    for (;;) {
	res = flash_sector_query(softc,&sector);
	if (res == CFE_ERR_UNSUPPORTED) break;
	if (res == 0) {
	    if (sector.flash_sector_status != FLASH_SECTOR_INVALID) {
		sector.flash_sector_idx++;
		continue;
		}
	    }
	break;
	}

    /* The sector offset will still contain the value at the end
       of the last successful call.  That's the last sector, so
       we can now use this to fill in the NVRAM info structure */

    if (res != CFE_ERR_UNSUPPORTED) {
	softc->flashdrv_nvraminfo.nvram_offset = sector.flash_sector_offset;
	softc->flashdrv_nvraminfo.nvram_size =   sector.flash_sector_size;
	softc->flashdrv_nvraminfo.nvram_eraseflg = TRUE; /* needs erase */
	softc->flashdrv_nvram_ok = TRUE;
	/*
	 * Set the flash's size as reported in the flash_info structure
	 * to be the size without the NVRAM sector at the end.
	 */
	softc->flashdrv_info.flash_size = sector.flash_sector_offset;
	softc->flashdrv_devsize = sector.flash_sector_offset;
	}

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
    flashdrv_t *softc;
    flash_probe_t *probe;
    char descr[80];

    probe = (flash_probe_t *) probe_ptr;

    /* 
     * probe_a is the flash base address
     * probe_b is the size of the flash
     * probe_ptr is unused.
     */

    softc = (flashdrv_t *) KMALLOC(sizeof(flashdrv_t),0);
    if (softc) {
	memset(softc,0,sizeof(flashdrv_t));

	if (probe) {
	    /* Passed probe structure, do fancy stuff */
	    memcpy(&(softc->flashdrv_probe),probe,sizeof(flash_probe_t));
	    if (softc->flashdrv_probe.flash_prog_phys == 0) {
		softc->flashdrv_probe.flash_prog_phys = 
		    softc->flashdrv_probe.flash_phys;
		}
	    }
	else {
	    /* Didn't pass probe structure, do the compatible thing */
	    softc->flashdrv_probe.flash_phys = probe_a;
	    softc->flashdrv_probe.flash_prog_phys = probe_a;
	    softc->flashdrv_probe.flash_size = probe_b;
	    softc->flashdrv_probe.flash_flags = FLASH_FLG_NVRAM;
	    softc->flashdrv_probe.nchips = 1;
	    }

#if __long64
	softc->flashdrv_cmdaddr = (char *) (softc->flashdrv_probe.flash_prog_phys);
#else
	softc->flashdrv_cmdaddr = (char *) UNCADDR(softc->flashdrv_probe.flash_prog_phys);
#endif
	softc->flashdrv_initialized = 0;
	softc->flashdrv_ops = &FLASHOPS_DEFAULT;
	xsprintf(descr,"%s at %08X size %uKB",drv->drv_description,
		 softc->flashdrv_probe.flash_phys,
		 softc->flashdrv_probe.flash_size/1024);
	cfe_attach(drv,softc,NULL,descr);
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
    flashdrv_t *softc = ctx->dev_softc;

    /*
     * do initialization
     */

    if (!softc->flashdrv_initialized) {

	/*
	 * Assume it's not an NVRAM-capable flash
	 */

	softc->flashdrv_nvram_ok = FALSE;

	/*
	 * Probe flash for geometry
	 */
	flash_getinfo(softc);

	/* 
	 * Find the last sector if in NVRAM mode
	 */

	if (softc->flashdrv_probe.flash_flags & FLASH_FLG_NVRAM) {
	    flashdrv_setup_nvram(softc);
	    }

	softc->flashdrv_initialized = TRUE;
	}

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
    flashdrv_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    unsigned char *flashbase;
    int offset;
    int blen;

    /*
     * For now, read the flash from K1 (always).  Eventually
     * we need to flush the cache after a write.
     */

    flashbase = (unsigned char *) UNCADDR(softc->flashdrv_probe.flash_phys);

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;
    offset = (int) buffer->buf_offset;

    if (!(softc->flashdrv_unlocked)) {
	if ((offset + blen) > softc->flashdrv_devsize) {
	    blen = softc->flashdrv_devsize - offset;
	    } 
	}

#ifdef _FLASH_BROKEN_BYTEREAD_
    /*
     * BCM1250 users: don't worry about this.  This hack is for
     * something else and should not be used with the BCM1250.
     */
    if (softc->flashdrv_probe.flash_flags & FLASH_FLG_16BIT) {
	uint16_t *src;
	int len = blen;
	int idx = 0;
	union {
	    uint16_t x;
	    char b[2];
	} u;

	src = (uint16_t *) flashbase;
	while (len > 0) {
	    u.x = src[(idx+offset)>>1];
	    *bptr++ = u.b[(idx+offset)&1];
	    len--;
	    idx++;
	    }
	}
    else {
	memcpy(bptr,flashbase + offset, blen);
	}
#else /* not _FLASH_BROKEN_BYTEREAD_ */

#ifdef __long64
    {
        /* Do it this way to allow flash at CS0 to be > 4 MB */
        int i;
        for (i = 0; i < blen; i++) {
            *bptr++ = (READFLASH_K1(softc,offset));
            offset++;
        }
    }
#else
    memcpy(bptr,flashbase + offset, blen);
#endif

#endif /* ifdef  _FLASH_BROKEN_BYTEREAD_ */

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
    /* flashdrv_t *softc = ctx->dev_softc; */

    inpstat->inp_status = 1;
    return 0;
}


/*  *********************************************************************
    *  flash_writeall(softc,buffer)
    *  
    *  Write the entire flash and reboot.  This is a special case
    *  used for when the flash currently being used for CFE's
    *  execution is updated.  A small assembly routine is relocated
    *  to DRAM to do the update (so that the programming routine is
    *  not erased while we're running it), and then the update
    *  is done.  When completed, CFE is restarted.
    *  
    *  (we could get really sleazy here and touch the routine first
    *  so it will stay in the cache, thereby eliminating the need
    *  to relocate it, but that's dangerous)
    *  
    *  Input parameters: 
    *  	   softc - our context
    *  	   buffer - buffer descriptor
    *  	   
    *  Return value:
    *  	   does not return
    ********************************************************************* */

static int flash_writeall(flashdrv_t *softc,iocb_buffer_t *buffer)
{
    void *rptr;
    void (*routine)(unsigned char *data,unsigned int flashbase,
		    unsigned int size,unsigned int secsize);

    rptr = KMALLOC(flash_write_all_len,0);

    if (!rptr) return CFE_ERR_NOMEM;

    memcpy(rptr,flash_write_all_ptr,flash_write_all_len);

    _cfe_flushcache(0);

    routine = rptr;

    (*routine)(buffer->buf_ptr,
	       softc->flashdrv_probe.flash_phys,
	       buffer->buf_length,
	       65536);

    return -1;    
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

static int flashdrv_write(cfe_devctx_t *ctx,iocb_buffer_t *buffer)
{
    flashdrv_t *softc = ctx->dev_softc;
    unsigned char *bptr;
    int offset;
    int blen;
    int res;

    bptr = buffer->buf_ptr;
    blen = buffer->buf_length;
    offset = (int) buffer->buf_offset;

    if (!(softc->flashdrv_unlocked)) {
	if ((offset + blen) > softc->flashdrv_devsize) {
	    blen = softc->flashdrv_devsize - offset;
	    }
	}

    res = FLASHOP_WRITE_BLOCK(softc,offset,bptr,blen);

    buffer->buf_retlen = res;


    return (res == blen) ? 0 : CFE_ERR_IOERR;
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
    flashdrv_t *softc = ctx->dev_softc;
    nvram_info_t *info;
    int offset;

    /*	
     * If using flash to store environment, only the last sector
     * is used for environment stuff.
     */

    switch ((int)buffer->buf_ioctlcmd) {
	case IOCTL_NVRAM_ERASE:
	    if (softc->flashdrv_nvram_ok == FALSE) return CFE_ERR_UNSUPPORTED;
	    FLASHOP_ERASE_SECTOR(softc,softc->flashdrv_nvraminfo.nvram_offset);
	    return 0;

	case IOCTL_NVRAM_GETINFO:
	    info = (nvram_info_t *) buffer->buf_ptr;
	    if (buffer->buf_length != sizeof(nvram_info_t)) return CFE_ERR_INV_PARAM;
	    if (softc->flashdrv_nvram_ok == FALSE) return CFE_ERR_UNSUPPORTED;
	    info->nvram_offset   = softc->flashdrv_nvraminfo.nvram_offset;
	    info->nvram_size     = softc->flashdrv_nvraminfo.nvram_size;
	    info->nvram_eraseflg = softc->flashdrv_nvraminfo.nvram_eraseflg;
	    buffer->buf_retlen = sizeof(nvram_info_t);
	    return 0;

	case IOCTL_FLASH_ERASE_SECTOR:
	    offset = (int) buffer->buf_offset;
	    if (!(softc->flashdrv_unlocked)) {
		if (offset >= softc->flashdrv_devsize) return -1;
		}
	    FLASHOP_ERASE_SECTOR(softc,offset);
	    return 0;

	case IOCTL_FLASH_ERASE_ALL:
	    offset = (int) buffer->buf_offset;
	    if (offset != 0) return -1;
	    flash_erase_all(softc);
	    return 0;

	case IOCTL_FLASH_WRITE_ALL:
	    flash_writeall(softc,buffer);
	    return -1;		/* should not return */

	case IOCTL_FLASH_GETINFO:
	    memcpy(buffer->buf_ptr,&(softc->flashdrv_info),sizeof(flash_info_t));
	    return 0;

	case IOCTL_FLASH_GETSECTORS:
	    return flash_sector_query(softc,(flash_sector_t *) buffer->buf_ptr);


	case IOCTL_FLASH_ERASE_RANGE:
	    return flash_erase_range(softc,(flash_range_t *) buffer->buf_ptr);	    

	case IOCTL_NVRAM_UNLOCK:
	    softc->flashdrv_unlocked = TRUE;
	    break;

        case IOCTL_FLASH_PROTECT_RANGE:
            return flash_protect_range(softc,(flash_range_t *) buffer->buf_ptr);

        case IOCTL_FLASH_UNPROTECT_RANGE:
            return flash_unprotect_range(softc,(flash_range_t *) buffer->buf_ptr);

	default:
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
    /* flashdrv_t *softc = ctx->dev_softc; */


    return 0;
}
