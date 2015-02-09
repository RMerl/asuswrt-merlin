/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Test commands				File: cfe_tests.c
    *  
    *  A temporary sandbox for misc test routines and commands.
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

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"

#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"
#include "cfe_timer.h"
#include "cfe_ioctl.h"

#include "cfe_error.h"

#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"

#include "net_ebuf.h"
#include "net_ether.h"

#include "net_api.h"

#include "cfe_fileops.h"

#include "cfe_bootblock.h"

#include "cfe_boot.h"

#include "bsp_config.h"

#include "dev_flash.h"

#include "cfe_mem.h"

extern long prog_entrypt;
extern void cfe_go(void);

extern int vapi_flushtest(void);

int ui_init_testcmds(void);
static int ui_cmd_timertest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_ethertest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_disktest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_exittest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_fstest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_copydisk(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_bootblock(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_flashtest(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_flashcopy(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_readnvram(ui_cmdline_t *cmd,int argc,char *argv[]);
static int ui_cmd_erasenvram(ui_cmdline_t *cmd,int argc,char *argv[]);
#ifdef __long64
static int ui_cmd_memorytest(ui_cmdline_t *cmd,int argc,char *argv[]);
#endif

extern int cfe_elfload(fileio_dispatch_t *ops,char *file,int flags,unsigned long *ept);

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


int ui_init_testcmds(void)
{

    cmd_addcmd("test timer",
	       ui_cmd_timertest,
	       NULL,
	       "Test the timer",
	       "test timer",
	       "");

    cmd_addcmd("test exit",
	       ui_cmd_exittest,
	       NULL,
	       "Try the firmware restart command",
	       "test exit [code]",
	       "");

    cmd_addcmd("test disk",
	       ui_cmd_disktest,
	       NULL,
	       "Do a disk test, reading random sectors on the disk",
	       "test disk device-name",
	       "-random");

    cmd_addcmd("test ether",
	       ui_cmd_ethertest,
	       NULL,
	       "Do an ethernet test, reading packets from the net",
	       "test ether device-name",
	       "");

    cmd_addcmd("test flash",
	       ui_cmd_flashtest,
	       NULL,
	       "Read manufacturer ID from flash",
	       "test flash",
	       "");

    cmd_addcmd("flashcopy",
	       ui_cmd_flashcopy,
	       NULL,
	       "Copy flash to flash",
	       "flashcopy sourcedev destdev",
	       "");

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

    cmd_addcmd("nvram read",
	       ui_cmd_readnvram,
	       NULL,
	       "read the NVRAM",
	       "test nvram devname offset",
	       "");

    cmd_addcmd("nvram erase",
	       ui_cmd_erasenvram,
	       NULL,
	       "erase the NVRAM",
	       "erasenvram devname",
	       "-pattern");

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

#ifdef __long64
    cmd_addcmd("memorytest",
	       ui_cmd_memorytest,
	       NULL,
	       "Extensive memory test",
	       "",
	       "");
#endif
    return 0;
}

static int ui_cmd_readnvram(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *dev;
    char *tok;
    int fd;
    int offset = 0;
    int res;
    uint8_t buf[512];
    int idx;

    dev = cmd_getarg(cmd,0);
    if (!dev) return ui_showusage(cmd);

    tok = cmd_getarg(cmd,1);
    if (tok) offset = xtoi(tok);
    else offset = 0;

    fd = cfe_open(dev);
    if (fd < 0) {
	ui_showerror(fd,"could not open NVRAM");
	return fd;
	}

    res = cfe_readblk(fd,offset,buf,512);
    printf("Offset %d Result %d\n",offset,res);
    for (idx = 0; idx < 512; idx++) {
	if ((idx % 16) == 0) printf("\n");
	printf("%02X ",buf[idx]);
	}
    printf("\n");	

    cfe_close(fd);
    return 0;
	
}

static int ui_cmd_erasenvram(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *dev;
    int fd;
    uint8_t buffer[2048];
    int res;
    char *tok;
    int offset;
    int length;
    uint8_t data;

    dev = cmd_getarg(cmd,0);
    if (!dev) return ui_showusage(cmd);

    offset = 0;
    if ((tok = cmd_getarg(cmd,1))) offset = xtoi(tok);
    length = 512; 

    if ((tok = cmd_getarg(cmd,2))) length = xtoi(tok);
    if (length > 2048) length = 2048;

    data = 0xFF;
    if ((tok = cmd_getarg(cmd,3))) data = xtoi(tok);

    fd = cfe_open(dev);
    if (fd < 0) {
	ui_showerror(fd,"could not open NVRAM");
	return fd;
	}

    if (cmd_sw_isset(cmd,"-pattern")) {
	memset(buffer,0,sizeof(buffer));
	for (res = 0; res < 2048; res++) {
	    buffer[res] = res & 0xFF;
	    }
	}
    else memset(buffer,data,sizeof(buffer));

    printf("Fill offset %04X length %04X\n",offset,length);

    res = cfe_writeblk(fd,offset,buffer,length);

    printf("write returned %d\n",res);

    cfe_close(fd);
    return 0;
	
}

static int ui_cmd_flashcopy(ui_cmdline_t *cmd,int argc,char *argv[])
{
    uint8_t *buffer;
    int sfd,dfd;
    char *sf,*df;
    int res;
    int idx;

    buffer = KMALLOC(1024,0);
    if (!buffer) return -1;

    sf = cmd_getarg(cmd,0);
    df = cmd_getarg(cmd,1);
    if (!sf || !df) {
	return ui_showusage(cmd);
	}

    sfd = cfe_open(sf);
    if (sfd < 0) {
	ui_showerror(sfd,"Could not open source device %s\n",sf);
	return sfd;
	}

    dfd = cfe_open(df);
    if (dfd < 0) {
	ui_showerror(sfd,"Could not open destination device %s\n",df);
	cfe_close(sfd);
	KFREE(buffer);
	return dfd;
	}

    printf("Erasing destination device %s...",df);

    res = cfe_ioctl(dfd,IOCTL_FLASH_ERASE_ALL,NULL,0,NULL,0);

    if (res < 0) {
	ui_showerror(res,"Failed: ");
	cfe_close(sfd);
	cfe_close(dfd);
	KFREE(buffer);
	return -1;
	}

    printf("Done\n");

    printf("Copying: ");

    for (idx = 0; idx < 2048; idx++) {
	cfe_readblk(sfd,idx*1024,buffer,1024);
	cfe_writeblk(dfd,idx*1024,buffer,1024);
	if ((idx % 256) == 0) printf(".");
	}

    printf("Done.\n");

    cfe_close(sfd);
    cfe_close(dfd);
    KFREE(buffer);

    return 0;
    
}

static int ui_cmd_flashtest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    flash_info_t info;
    int fd;
    int retlen;
    int res = 0;
    int idx;
    flash_sector_t sector;
    nvram_info_t nvraminfo;

    fd = cfe_open(cmd_getarg(cmd,0));
    if (fd < 0) return fd;

    res = cfe_ioctl(fd,IOCTL_NVRAM_GETINFO,(uint8_t *) &nvraminfo,sizeof(nvram_info_t),&retlen,0);
    if (res == 0) {
	printf("NVRAM: Offset %08X Size %08X EraseFlg %d\n",
	       nvraminfo.nvram_offset,nvraminfo.nvram_size,nvraminfo.nvram_eraseflg);
	}

    res = cfe_ioctl(fd,IOCTL_FLASH_GETINFO,(uint8_t *) &info,sizeof(flash_info_t),&retlen,0);
    if (res == 0) {
	printf("FLASH: Base %016llX size %08X type %02X flags %08X\n",
	   info.flash_base,info.flash_size,info.flash_type,info.flash_flags);
	}

    idx = 0;
    for (;;) {
	sector.flash_sector_idx = idx;
	res = cfe_ioctl(fd,IOCTL_FLASH_GETSECTORS,(uint8_t *) &sector,sizeof(flash_sector_t),&retlen,0);
	if (res != 0) {
	    printf("ioctl error\n");
	    break;
	    }
	if (sector.flash_sector_status == FLASH_SECTOR_INVALID) break;
	printf("Sector %d offset %08X size %d\n",
	       sector.flash_sector_idx,
	       sector.flash_sector_offset,
	       sector.flash_sector_size);     
	idx++;
	}

    cfe_close(fd);
    return 0;

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


static int ui_cmd_exittest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int val = 0;
    char *x;

    x = cmd_getarg(cmd,0);
    if (x) val = atoi(x);

    cfe_exit(1,val);

    return -1;
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


static int ui_cmd_ethertest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    char *tok;
    int fh;
    uint8_t packet[2048];
    int res;
    int idx;

    tok = cmd_getarg(cmd,0);
    if (!tok) return -1;

    fh = cfe_open(tok);
    if (fh < 0) {
	xprintf("Could not open device: %s\n",cfe_errortext(fh));
	return fh;
	}

    xprintf("Receiving... prese enter to stop\n");
    while (!console_status()) {
	res = cfe_read(fh,packet,sizeof(packet));
	if (res == 0) continue;
	if (res < 0) {
	    xprintf("Read error: %s\n",cfe_errortext(res));
	    break;
	    }

	xprintf("%4d ",res);
	if (res > 32) res = 32;

	for (idx = 0; idx < res; idx++) {
	    xprintf("%02X",packet[idx]);
	    if ((idx == 5) || (idx == 11) || (idx == 13)) xprintf(" ");
	    }

	xprintf("\n");
	}

    cfe_close(fh);

    return 0;
}


static int ui_cmd_disktest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int fh;
    char *tok;
    char *tok2;
    cfe_offset_t offset;
    uint64_t sectors;
    int secsize;
    long secnum = 0;
    unsigned char buffer[2048];
    int res;
    int idx,idx2;
    int count = 0;

    tok = cmd_getarg(cmd,0);
    if (!tok) return -1;

    tok2 = cmd_getarg(cmd,1);

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


static int ui_cmd_timertest(ui_cmdline_t *cmd,int argc,char *argv[])
{
    int64_t t;

    t = cfe_ticks;

    while (!console_status()) {
	cfe_sleep(CFE_HZ);
	if (t != cfe_ticks) {
	    xprintf("Time is %lld\n",cfe_ticks);
	    t = cfe_ticks;	    
	    }
	}

    return 0;
}




#ifdef __long64
/* extensive memory tests */

static void inline uacwrite(long *srcadr,long *dstadr) 
{
__asm __volatile ("ld $8, 0(%0) ; "
		  "ld $9, 8(%0) ; "  
		  "ld $10, 16(%0) ; " 
		  "ld $11, 24(%0) ; " 
		  "sync ; " 
		  "sd $8,  0(%1) ; " 
		  "sd $9,  8(%1) ; " 
		  "sd $10, 16(%1) ; " 
		  "sd $11, 24(%1) ; " 
		  "sync" :: "r"(srcadr),"r"(dstadr) : "$8","$9","$10","$11");
}


#define TEST_DATA_LEN 4
#define CACHE_LINE_LEN 32

static int ui_cmd_memorytest(ui_cmdline_t *cmd,int argc,char *argv[])
{

    static long test_data[TEST_DATA_LEN] = {
//    0, 0, 0, 0, /* one cache line */
//    -1, -1, -1, -1, /* one cache line */
	0xaaaaaaaaaaaaaaaa, 0x5555555555555555, 0xcccccccccccccccc, 0x3333333333333333, /* one cache line */
    };
    int arena, exitLoop;
    int error;
    int arena_type;
    uint64_t arena_start, arena_size;
    long phys_addr, offset, mem_base, cache_mem_base, i;
    long *dst_adr, *cache_dst_adr;
    long cda,tda;

    arena = 0;
    exitLoop = 0; 
    offset = 0; 
    mem_base = 0;

    xprintf("arenas:\n");
    while (cfe_arena_enum(arena, &arena_type, &arena_start, &arena_size, FALSE) == 0) {
	phys_addr = (long) arena_start;    /* actual physical address */
	mem_base = PHYS_TO_XKPHYS(K_CALG_UNCACHED_ACCEL, phys_addr); /* virtual address */
	xprintf("phys = %016llX, virt = %016llX, size = %016llX\n", phys_addr, mem_base, arena_size);
	arena++;
	}

    arena = 0; 
    exitLoop = 0;
    error = 0;

    while (cfe_arena_enum(arena, &arena_type, &arena_start, &arena_size, FALSE) == 0) {

	test_data[0] = 0xAAAAAAAAAAAAAAAA;
	test_data[1] = 0x5555555555555555;
	test_data[2] = 0xCCCCCCCCCCCCCCCC;
	test_data[3] = 0x3333333333333333;

	phys_addr = (long) arena_start;    /* actual physical address */
	mem_base = PHYS_TO_XKPHYS(K_CALG_UNCACHED_ACCEL, phys_addr); /* virtual address */
	cache_mem_base = PHYS_TO_K0(phys_addr);

	xprintf("phys = %016llX, virt = %016llX, size = %016llX\n", phys_addr, mem_base, arena_size);

	xprintf("Writing: a/5/c/e\n");

	for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
	    dst_adr = (long*)(mem_base+offset);
	    uacwrite(test_data, dst_adr);
	    }

	xprintf("Reading: a/5/c/e\n");

	for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
	    dst_adr = (long*)(mem_base+offset);
	    cache_dst_adr = (long*)(mem_base+offset);
	    for (i = 0; i < TEST_DATA_LEN; i++) {
		cda = cache_dst_adr[i];
		tda = test_data[i];
		if (cda != tda) {
		    xprintf("mem[%4d] %016llX != %016llX\n",
			    mem_base+offset+i, cda, tda);
		    exitLoop = 1;  
		    }	
		}
	    if (exitLoop) break;
	    }


	if (exitLoop) {
	    exitLoop = 0;
	    error++;
	    arena++;
	    continue;
	    }

	xprintf("Writing: address|5555/inv/aaaa|address\n");
	exitLoop = 0;

	for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
	    dst_adr = (long*)(mem_base+offset);
	    test_data[0] = ((long)dst_adr<<32)|0x55555555;
	    test_data[1] = ~test_data[0];
	    test_data[2] = 0xaaaaaaaa00000000|((long)dst_adr & 0xffffffff);
	    test_data[3] = ~test_data[2];
	    uacwrite(test_data, dst_adr);
	    }

	xprintf("Reading: address|5555/inv/aaaa|address\n");

	for (offset = 0; (offset < arena_size); offset += CACHE_LINE_LEN) {
	    dst_adr = (long*)(mem_base+offset);
	    test_data[0] = ((long)dst_adr<<32)|0x55555555;
	    test_data[1] = ~test_data[0];
	    test_data[2] = 0xaaaaaaaa00000000|((long)dst_adr & 0xffffffff);
	    test_data[3] = ~test_data[2];
	    cache_dst_adr = (long*)(mem_base+offset);
	    for (i = 0; i < TEST_DATA_LEN; i++) {
		cda = cache_dst_adr[i];
		tda = test_data[i];
		if (cda != tda) {
		    xprintf("mem[%4d] %016llX != %016llX\n",
			    mem_base+offset+i,cda,tda);
		    exitLoop = 1;
		    }	
		}
	    if (exitLoop) break;
	    }

	if (exitLoop) {
	    error++;
	    exitLoop = 0;
	    }

	arena++;
	}

    if (error) printf("Failing address: %016llX\n",mem_base+offset);

    return error ? -1 : 0;
}

#endif
