/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Commands to test block devices		File: ui_test_disk.c
    *  
    *  Commands to manipulate block devices live here.
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
#include "cfe_console.h"
#include "cfe_error.h"
#include "cfe_ioctl.h"
#include "cfe_devfuncs.h"
#include "ui_command.h"
#include "cfe.h"

#include "cfe_fileops.h"
#include "cfe_bootblock.h"
#include "cfe_boot.h"

static int ui_cmd_disktest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_fstest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_copydisk(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_bootblock(ui_cmdline_t *cmd,int argc,char *argv[]);

int ui_init_disktestcmds(void);

int ui_init_disktestcmds(void)
{

    cmd_addcmd("test disk",
	       ui_cmd_disktest,
	       NULL,
	       "Do a disk test, read/write sectors on the disk",
	       "test disk device-name [-random | sector# | {-w sector offset byte}]",
	       "-random;|"
	       "-w;Write a byte at offset in sector.**DANGER!! BE CAREFUL WHICH DEVICE YOU WRITE TO.**");

    cmd_addcmd("test fatfs",
	       ui_cmd_fstest,
	       NULL,
	       "Do a FAT file system test",
	       "test fatfs device-name",
	       "");

    cmd_addcmd("copydisk",
	       ui_cmd_copydisk,
	       NULL,
	       "Copy a remote disk image to a local disk device via TFTP",
	       "copydisk host:filename device-name [offset]",
	       "");

    cmd_addcmd("show boot",
	       ui_cmd_bootblock,
	       NULL,
	       "Display boot block from device,",
	       "show boot device-name\n\n"
	       "This command displays the boot block on the specified device.  The\n"
	       "device-name parameter identifies a block device (disk, tape, CD-ROM)\n"
	       "to be scanned for boot blocks.  The first boot block found will be\n"
	       "displayed.",
	       "");
    return 0;
}


static unsigned long rand(void)
{
    static unsigned long seed = 1;
    long x, hi, lo, t;

    x = seed;
    hi = x / 127773;
    lo = x % 127773;
    t = 16807 * lo - 2836 * hi;
    if (t <= 0) t += 0x7fffffff;
    seed = t;
    return t;
}


static int ui_cmd_bootblock(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int fh;
    char *tok;
    struct boot_block bootblock;
    int res;
    int idx;
    int sec;
    uint32_t checksum;
    uint32_t checksumd;
    uint32_t calcsum;
    uint32_t secsize;
    uint64_t secoffset;
    uint8_t *code;

    tok = cmd_getarg(cmd,0);
    if (!tok) return -1;

    fh = cfe_open(tok);
    if (fh < 0) {
	xprintf("Could not open device; %d\n",fh);
	return -1;
	}
    for (sec = 0; sec < BOOT_BLOCK_MAXLOC; sec++) {
        res = cfe_readblk(fh,sec * BOOT_BLOCK_BLOCKSIZE,
			  (unsigned char *) &bootblock,sizeof(bootblock));
	    
	if (bootblock.bb_magic != BOOT_MAGIC_NUMBER) {
		continue;
	}
	xprintf("Found boot block in sector %d\n", sec);
	if (res != sizeof(bootblock)) {
	    xprintf("Could not read boot block\n");
	    cfe_close(fh);
	    return -1;
	    }

	xprintf("Boot block data:\n");
	for (idx = 59; idx < 64; idx++) {
	    xprintf("  %d: %016llX\n",idx,bootblock.bb_data[idx]);
	    }    
        xprintf("\n");

	xprintf("Boot block version is %d\n",
		(uint32_t) ((bootblock.bb_hdrinfo & BOOT_HDR_VER_MASK) >> BOOT_HDR_VER_SHIFT));
	xprintf("Boot block flags are %02X\n",
		(uint32_t) ((bootblock.bb_hdrinfo & BOOT_HDR_FLAGS_MASK) >> 56));
	checksum = ((uint32_t) (bootblock.bb_hdrinfo & BOOT_HDR_CHECKSUM_MASK));
	checksumd = ((uint32_t) ((bootblock.bb_secsize & BOOT_DATA_CHECKSUM_MASK) >> BOOT_DATA_CHECKSUM_SHIFT));
	bootblock.bb_hdrinfo &= ~BOOT_HDR_CHECKSUM_MASK;
	secsize = ((uint32_t) (bootblock.bb_secsize & BOOT_SECSIZE_MASK));
	secoffset = bootblock.bb_secstart;
	
	xprintf("Boot code is %d bytes at %016llX\n",secsize,secoffset);
	
	CHECKSUM_BOOT_DATA(&(bootblock.bb_magic),BOOT_BLOCK_SIZE,&calcsum);
	
	if (checksum != calcsum) {
	    xprintf("Header checksum does not match Blk=%08X Calc=%08X\n",
		    checksum,calcsum);
	    }
	else {
	    xprintf("Header checksum is ok\n");
	    }
	
	code = KMALLOC(secsize,0);
	if (code) {
	    res = cfe_readblk(fh,secoffset,code,secsize);
	    if (res != secsize) {
	    	xprintf("Could not read boot code\n");
	    	cfe_close(fh);
	    	KFREE(code);
	    	return -1;
		}
	    CHECKSUM_BOOT_DATA(code,secsize,&calcsum);
	    if (calcsum == checksumd) xprintf("Boot code checksum is ok\n");
	    else xprintf("Boot code checksum is incorrect (Calc=%08X, Blk=%08X)\n",
			 calcsum,checksumd);
	    KFREE(code);
	    }
	break;
        }
    if (sec == BOOT_BLOCK_MAXLOC) {
	    xprintf("No valid boot blocks found in the first %d sectors\n",
		    BOOT_BLOCK_MAXLOC);
    }
    cfe_close(fh);

    return 0;    
}





extern int fatfs_fileop_dir(void *fsctx);

static int ui_cmd_fstest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *tok;
    char *fname;
    fileio_ctx_t *fsctx;
    void *filectx;
    uint8_t buffer[1000];
    int res;
    int total;
    
    tok = cmd_getarg(cmd,0);
    if (!tok) return -1;

    fname = cmd_getarg(cmd,1);

    res = fs_init("fat",&fsctx,tok);
    if (res < 0) {
	xprintf("Could not init file system: %s\n",cfe_errortext(res));
	return res;
	}

    if (!fname) {
	fatfs_fileop_dir(fsctx->fsctx);
	}
    else {
	res = fs_open(fsctx,&filectx,fname,FILE_MODE_READ);
	if (res < 0) {
	    xprintf("Could not open %s: %s\n",fname,cfe_errortext(res));
	    }
	else {

	    total = 0;
	    for (;;) {
		res = fs_read(fsctx,filectx,buffer,sizeof(buffer));
		if (res < 0) break;
		total += res;
		if (res != sizeof(buffer)) break;
		xprintf(".");
		}
	    if (res < 0) xprintf("read error %s\n",cfe_errortext(res));
	    else xprintf("Total bytes read: %d\n",total);
	    fs_close(fsctx,filectx);
	    }
	}

    fs_uninit(fsctx);
    return 0;
}

static int ui_cmd_copydisk(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *fname;
    fileio_ctx_t *fsctx;
    void *filectx;
    char *devname;
    uint8_t buffer[1024];
    int fh;
    int res;
    int total;
    int count;
    int offset;
    char *toffset;

    fname = cmd_getarg(cmd,0);
    if (!fname) return ui_showusage(cmd);

    devname = cmd_getarg(cmd,1);
    if (!devname) return ui_showusage(cmd);

    toffset = cmd_getarg(cmd,2);
    if (!toffset) offset = 0; else offset = atoi(toffset);

    if ((cfe_getdevinfo(devname) & CFE_DEV_MASK) != CFE_DEV_DISK) {
	xprintf("Device %s is not a disk.\n",devname);
	return CFE_ERR_INV_PARAM;
	}

    fh = cfe_open(devname);
    if (fh < 0) {
	return ui_showerror(fh,"Could not open device %s",devname);
	}

    res = fs_init("tftp",&fsctx,"");
    if (res < 0) {
	return ui_showerror(res,"Could not init file system");
	}

    res = fs_open(fsctx,&filectx,fname,FILE_MODE_READ);
    if (res < 0) {
	return ui_showerror(res,"Could not open %s",fname);
	}
    else {
	total = 0;
	count = 0;
	for (;;) {
	    res = fs_read(fsctx,filectx,buffer,sizeof(buffer));
	    if (res < 0) break;
	    if (res > 0) cfe_writeblk(fh,total+offset*512,buffer,res);
	    total += res;
	    if (res != sizeof(buffer)) break;
	    count++;
	    if (count == 256) {
		xprintf(".");
		count = 0;
		}
	    }
	if (res < 0) xprintf("read error %s\n",cfe_errortext(res));
	else xprintf("Total bytes read: %d\n",total);
	fs_close(fsctx,filectx);
	}

    fs_uninit(fsctx);
    cfe_close(fh);
    return 0;
}

static int ui_cmd_disktest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int fh;
    char *tok;
    char *tok2;
    char *tok3;
    char *tok4;
    cfe_offset_t offset;
    uint64_t sectors;
    int secsize;
    long secnum = 0;
    unsigned char buffer[2048];
    int res;
    int idx,idx2;
    int count = 0;
    uint8_t byte;
    int secoffset = 0;

    tok = cmd_getarg(cmd,0);
    if (!tok) return -1;

    tok2 = cmd_getarg(cmd,1);
    tok3 = cmd_getarg(cmd,2);
    tok4 = cmd_getarg(cmd,3);

    fh = cfe_open(tok);
    if (fh <= 0) {
	xprintf("Could not open device: %s\n",cfe_errortext(fh));
	return fh;
	}

    xprintf("device opened ok\n");

    sectors = 0; secsize = 0;
    cfe_ioctl(fh,IOCTL_BLOCK_GETTOTALBLOCKS,(uint8_t *) &sectors,sizeof(sectors),&res,0);
    cfe_ioctl(fh,IOCTL_BLOCK_GETBLOCKSIZE,(uint8_t *) &secsize,sizeof(secsize),&res,0);
    printf("Total sectors: %lld  Sector size: %d\n",sectors,secsize);
    if (secsize == 0) secsize = 512;
    if (sectors == 0) sectors = 100000;

    if (tok2) {
	secnum = atoi(tok2);
	offset = (cfe_offset_t) secnum * (cfe_offset_t) secsize;
	if (cmd_sw_isset(cmd,"-w")) {
	    secoffset = atoi(tok3);
	    byte = (uint8_t) xtoq(tok4);
	    res = cfe_writeblk(fh,offset+secoffset,&byte,1);
	    if (res != 1) {
		xprintf("Write failed\n");
		return -1;
		}
	    }
	res = cfe_readblk(fh,offset,buffer,secsize);
	if (res != secsize) {
	    xprintf("disk error: %d  sector %d\n",res,secnum);
	    }
	else { 
	    for (idx = 0; idx < secsize; idx+=16) {
		xprintf("%04X: ",idx);
		for (idx2 = 0; idx2 < 16; idx2++) {
		    xprintf("%02X ",buffer[idx+idx2]);
		    }
		for (idx2 = 0; idx2 < 16; idx2++) {
		    if ((buffer[idx+idx2] < 32) ||
			(buffer[idx+idx2] > 127)) {
			xprintf(".");
			}
		    else {
			xprintf("%c",buffer[idx+idx2]);
			}
		    }
		xprintf("\n");
		}
	    }
	}
    else {
	if (cmd_sw_isset(cmd,"-random")) {
	    while (!console_status()) {
		secnum++;
		secnum = rand() % sectors;
		offset = (cfe_offset_t) secnum * (cfe_offset_t) secsize;
		res = cfe_readblk(fh,offset,buffer,secsize);
		if (res != secsize) {
		    xprintf("disk error: %d  sector %d\n",res,secnum);
		    break;
		    }
		count++;
		if ((count % 1000) == 0) xprintf("%d ",count);
		}
	    }
	}

    cfe_close(fh);

    return 0;    
}
