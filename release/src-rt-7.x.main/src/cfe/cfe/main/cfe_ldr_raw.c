/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  RAW Program Loader			File: cfe_ldr_raw.c
    *  
    *  This program reads raw binaries into memory.
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
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_error.h"
#include "cfe_devfuncs.h"

#include "cfe.h"
#include "cfe_fileops.h"

#include "cfe_boot.h"
#include "cfe_bootblock.h"

#include "cfe_loader.h"

/*  *********************************************************************
    *  Externs
    ********************************************************************* */

static int cfe_rawload(cfe_loadargs_t *la);

const cfe_loader_t rawloader = {
    "raw",
    cfe_rawload,
    0};

/*  *********************************************************************
    *  cfe_findbootblock(la,fsctx,ref)
    *  
    *  Find the boot block on the specified device.
    *  
    *  Input parameters: 
    *  	   la - loader args (to be filled in)
    *  	   ops - file operations
    *  	   ref - reference for open file handle
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */
static int cfe_findbootblock(cfe_loadargs_t *la,
			     fileio_ctx_t *fsctx,
			     void *ref,
			     struct boot_block *bootblock)
{
    uint32_t checksum = 0;
    uint32_t calcsum = 0; 
    uint32_t secsize = 0;
    uint64_t secoffset = 0;
    int res;
    int curblk;

    /* 
     * Search for the boot block.  Stop when we find
     * something with a matching checksum and magic
     * number.
     */

    fs_seek(fsctx,ref,0,FILE_SEEK_BEGINNING);

    for (curblk = 0; curblk < BOOT_BLOCK_MAXLOC; curblk++) {


	/* Read a block */

	res = fs_read(fsctx,ref,
		     (unsigned char *) bootblock,
		     sizeof(struct boot_block));

	if (res != sizeof(struct boot_block)) {
	    return CFE_ERR_IOERR;
	    }

	/* Verify magic number */

	if (bootblock->bb_magic != BOOT_MAGIC_NUMBER) {
	    continue;
	    }

	/* Extract fields from block */

	checksum = ((uint32_t) (bootblock->bb_hdrinfo & BOOT_HDR_CHECKSUM_MASK));
	bootblock->bb_hdrinfo &= ~BOOT_HDR_CHECKSUM_MASK;
	secsize = ((uint32_t) (bootblock->bb_secsize & BOOT_SECSIZE_MASK));
	secoffset = bootblock->bb_secstart;

	/* Verify block's checksum */

	CHECKSUM_BOOT_DATA(&(bootblock->bb_magic),BOOT_BLOCK_SIZE,&calcsum);

	if (checksum == calcsum) {
	    break;
	    }
	}

    /*
     * Okay, determine if we were successful.
     */

    if (bootblock->bb_magic != BOOT_MAGIC_NUMBER) {
	return CFE_ERR_INVBOOTBLOCK;
	}

    if (checksum != calcsum) {
	return CFE_ERR_BBCHECKSUM;
	}

    /*
     * If we get here, we had a valid boot block.
     */

    return 0;    
}


/*  *********************************************************************
    *  cfe_rawload(la)
    *
    *  Read a raw (unformatted) boot file
    *
    *  Input parameters:
    *      la - loader args
    *
    *  Return value:
    *      0 if ok, else error code
    ********************************************************************* */
static int cfe_rawload(cfe_loadargs_t *la)
{
    int res;
    fileio_ctx_t *fsctx;
    const fileio_dispatch_t *ops;
    void *ref;
    int ttlcopy = 0;
    int findbb;
    int devinfo;
    struct boot_block bootblock;
    uint8_t *ptr;
    uint8_t *bootcode;
    uint32_t checksum,calcsum;
    uint64_t secoffset = 0;
    int32_t maxsize;
    int amtcopy;
    int thisamt;
    uint32_t loadflags;
    int onedot;

    loadflags = la->la_flags;

    /*
     * Set starting address and maximum size.  You can either
     * explicitly set this (with LOADFLG_SPECADDR) or
     * let CFE decide.  If CFE decides, the load address
     * will be BOOT_START_ADDRESS in all cases.
     * The size is dependant on the device type: block and flash
     * devices will get this info from the boot block,
     * and network devices will get the info by reaching EOF
     * on reads, up to the maximum size of the boot area.
     */

    if (loadflags & LOADFLG_SPECADDR) {
	bootcode = (uint8_t *) la->la_address;
	maxsize = la->la_maxsize;
	findbb = FALSE;		/* don't find a boot block */
	}
    else {
	bootcode = (uint8_t *) BOOT_START_ADDRESS;
	maxsize = BOOT_AREA_SIZE;
	findbb = FALSE;
	devinfo = la->la_device ? cfe_getdevinfo(la->la_device) : 0;

	/*
	 * If the device is either a disk or a flash device,
	 * we will expect to find a boot block.
	 * Serial and network devices do not have boot blocks.
	 */
	if ((devinfo >= 0) && 
	    ( ((devinfo & CFE_DEV_MASK) == CFE_DEV_DISK) || 
	      ((devinfo & CFE_DEV_MASK) == CFE_DEV_FLASH) ))  {
	    findbb = TRUE;
	    }
	}


    /*
     * merge in any filesystem-specific flags
     */

    ops = cfe_findfilesys(la->la_filesys);
    if (!ops) return CFE_ERR_FSNOTAVAIL;
    loadflags |= ops->loadflags;

    /*
     * turn off the boot block if requested.
     */

    if (loadflags & LOADFLG_NOBB) findbb = FALSE;

    /*
     * Create a file system context
     */

    res = fs_init(la->la_filesys,&fsctx,la->la_device);
    if (res != 0) {
	return res;
	}

    /*
     * Turn on compression if we're doing that.
     */

    if (!findbb && (la->la_flags & LOADFLG_COMPRESSED)) {
		uint8_t c[2];
		int len;
		char *zh = "z";

		/* 
		* Check compression method
		*/
		res = fs_open(fsctx, &ref, la->la_filename, FILE_MODE_READ);
		if (res != 0)
			goto errout;

		len = fs_read(fsctx, ref, c, 2);
		fs_close(fsctx, ref);
		if (len != 2) {
			res = CFE_ERR_IOERR;
			goto errout;
		}

#ifdef	CFG_ZLIB
		if (memcmp(c, "\x1f\x8b", 2) == 0)
			zh = "z";
		else
#endif
#ifdef	CFG_LZMA
		if (memcmp(c, "\x5d\x00", 2) == 0)
			zh = "lzma";
		else
#endif
		{
			xprintf("Image compressed with unsupported method\n");
			res = CFE_ERR_UNSUPPORTED;
			goto errout;
		}

		res = fs_hook(fsctx, zh);
		if (res != 0)
			goto errout;
	}

	/*
     * Open the boot device
     */

    res = fs_open(fsctx,&ref,la->la_filename,FILE_MODE_READ);
    if (res != 0)
		goto errout;

    /*
     * If we need to find a boot block, do it now.
     */

    if (findbb) {
	res = cfe_findbootblock(la,fsctx,ref,&bootblock);

	/*
	 * If we found the boot block, seek to the part of the
	 * disk where the boot code is.
	 * Otherwise, get out now, since the disk has no boot block.
	 */

	if (res == 0) {
	    maxsize = (int) ((uint32_t) (bootblock.bb_secsize & BOOT_SECSIZE_MASK));
	    secoffset = bootblock.bb_secstart;
	    fs_seek(fsctx,ref,secoffset,FILE_SEEK_BEGINNING);
	    }
	else {
	    fs_close(fsctx,ref);
	    goto errout;
	    }
	
	}

    /*
     * Okay, go load the boot file.
     */

    ptr = bootcode;
    amtcopy = maxsize;
    ttlcopy = 0;

    onedot = amtcopy / 10;			/* ten dots for entire load */
    if (onedot < 4096) onedot = 4096;		/* but minimum 4096 bytes per dot */
    onedot = (onedot + 1) & ~4095;		/* round to multiple of 4096 */

    while (amtcopy > 0) {
	thisamt = onedot;
	if (thisamt > amtcopy) thisamt = amtcopy;

	res = fs_read(fsctx,ref,ptr,thisamt);
	if (la->la_flags & LOADFLG_NOISY) {
	    xprintf(".");
	    }
	if (res <= 0) break;
	ptr += res;
	amtcopy -= res;
	ttlcopy += res;
	}

    /*
     * We're done with the file.
     */

    fs_close(fsctx,ref);
    fs_uninit(fsctx);

    /*
     * Verify the boot loader checksum if we were reading
     * the disk.
     */

    if (findbb) {
	CHECKSUM_BOOT_DATA(bootcode,maxsize,&calcsum);
	checksum = (uint32_t) ((bootblock.bb_secsize & BOOT_DATA_CHECKSUM_MASK) 
			       >> BOOT_DATA_CHECKSUM_SHIFT);

	if (checksum != calcsum) {
	    return CFE_ERR_BOOTPROGCHKSUM;
	    }
	}

    la->la_entrypt = (uintptr_t) bootcode;

    if (la->la_flags & LOADFLG_NOISY) xprintf(" %d bytes read\n",ttlcopy);

    return (res < 0) ? res : ttlcopy;

errout:
	fs_uninit(fsctx);
	return res;
}
