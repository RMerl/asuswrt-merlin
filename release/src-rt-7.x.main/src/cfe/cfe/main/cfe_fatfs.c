/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  FAT file system				File: cfe_fatfs.c
    *  
    *  This module knows how to read files from a FAT formatted
    *  file system (12 or 16 bit fats only for now)
    *
    *  Eventually, we'll also include support for the FAT Translation
    *  Layer (FTL) on PCMCIA flash file systems.
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

#include "cfe_error.h"
#include "cfe_fileops.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"

#include "cfe_loader.h"

#include "cfe.h"


/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#define SECTORSIZE 		512
#define DIRENTRYSIZE		32
#define DIRPERSECTOR		(SECTORSIZE/DIRENTRYSIZE)

/*#define _FATFS_DEBUG_*/

/*
 * Bios Parameter Block offsets and values
 */

#define BPB_JMPINSTR		0x00
#define BPB_JMPINSTR_VALUE	0xEB
#define BPB_JMPINSTR_VALUE2	0xE9
#define BPB_SEAL		0x1FE
#define BPB_SEAL_VALUE		0xAA55

#define BPB_BYTESPERSECTOR	0x0B
#define BPB_SECTORSPERCLUSTER	0x0D
#define BPB_RESERVEDSECTORS	0x0E
#define BPB_NUMFATS		0x10
#define BPB_MAXROOTDIR		0x11
#define BPB_TOTALSECTORS	0x13
#define BPB_SECTORSPERFAT	0x16
#define BPB_SECTORSPERTRACK	0x18
#define BPB_NUMHEADS		0x1A
#define BPB_HIDDENSECTORS	0x1C
#define BPB_SYSTEMID		54
#define BPB_MEDIADESCRIPTOR	21
#define BPB_SIGNATURE		38
#define BPB_SIGNATURE_VALUE1	0x28
#define BPB_SIGNATURE_VALUE2	0x29

/*
 * Partition types
 */

#define PARTTYPE_EMPTY	0
#define PARTTYPE_FAT12	1
#define PARTTYPE_FAT16	4
#define PARTTYPE_FAT16BIG 6
#define PARTTYPE_FAT32	0x0B

/*
 * Partition table offsets
 */
#define PTABLE_STATUS		0
#define PTABLE_STARTHEAD	1
#define PTABLE_STARTSECCYL	2	/* 2 bytes */
#define PTABLE_TYPE		4
#define PTABLE_ENDHEAD		5
#define PTABLE_ENDSECCYL	6	/* 2 bytes */
#define PTABLE_BOOTSECTOR	8	/* 4 bytes */
#define PTABLE_NUMSECTORS	12	/* 4 bytes */

#define PTABLE_SIZE		16
#define PTABLE_COUNT		4
#define PTABLE_OFFSET		(512-2-(PTABLE_COUNT*PTABLE_SIZE))

#define PTABLE_STATUS_ACTIVE	0x80

/*
 * Directory attributes
 */

#define ATTRIB_NORMAL     0x00 
#define ATTRIB_READONLY   0x01 
#define ATTRIB_HIDDEN     0x02 
#define ATTRIB_SYSTEM     0x04 
#define ATTRIB_LABEL      0x08 
#define ATTRIB_DIR        0x10 
#define ATTRIB_ARCHIVE    0x20 

#define ATTRIB_LFN	  0x0F

/*
 * Macros to read fields in directory & BPB entries
 */

#define READWORD(buffer,x) (((unsigned int) (buffer)[(x)]) | \
                           (((unsigned int) (buffer)[(x)+1]) << 8))

#define READWORD32(buffer,x) (READWORD(buffer,(x)) | (READWORD(buffer,(x)+2) << 16))

#define READBYTE(buffer,x) ((unsigned int) (buffer)[(x)])

/*
 * Directory entry offsets and values
 */

#define DIR_CHECKSUM		13
#define DIR_FILELENGTH		28
#define DIR_STARTCLUSTER	26
#define DIR_ATTRIB		11
#define DIR_NAMEOFFSET		0
#define DIR_NAMELEN		8
#define DIR_EXTOFFSET		8
#define DIR_EXTLEN		3

#define DIRENTRY_CHECKSUM(e) READBYTE(e,DIR_CHECKSUM)
#define DIRENTRY_FILELENGTH(e) READWORD32(e,DIR_FILELENGTH)
#define DIRENTRY_STARTCLUSTER(e) READWORD(e,DIR_STARTCLUSTER)
#define DIRENTRY_ATTRIB(e) READBYTE(e,DIR_ATTRIB)

#define DIRENTRY_LAST		0
#define DIRENTRY_DELETED	0xE5
#define DIRENTRY_PARENTDIR	0x2E

#define DIRENTRY_LFNIDX(e) READBYTE(e,0)
#define LFNIDX_MASK	0x1F
#define LFNIDX_END	0x40
#define LFNIDX_MAX	20

/*  *********************************************************************
    *  Types
    ********************************************************************* */

/*
 * Internalized BPB
 */

typedef struct bpb_s {
    unsigned int bpb_bytespersector;
    unsigned int bpb_sectorspercluster;
    unsigned int bpb_reservedsectors;
    unsigned int bpb_numfats;
    unsigned int bpb_maxrootdir;
    unsigned int bpb_totalsectors;
    unsigned int bpb_sectorsperfat;
    unsigned int bpb_sectorspertrack;
    unsigned int bpb_numheads;
    unsigned int bpb_hiddensectors;
} bpb_t;

/*
 * FAT Filesystem descriptor - contains working information
 * about an "open" file system
 */

typedef struct fatfs_s {
    int fat_fh;
    int fat_refcnt;
    bpb_t fat_bpb;
    int fat_twelvebit;
    int fat_partstart;
    uint8_t fat_dirsector[SECTORSIZE];
    int fat_dirsecnum;
    uint8_t fat_fatsector[SECTORSIZE];
    int fat_fatsecnum;
} fatfs_t;

/*
 * FAT Chain - describes a series of FAT entries
 */

typedef struct fatchain_s {
    int fat_start;
    uint16_t *fat_entries;
    int fat_count;
} fatchain_t;

/*
 * FAT File descriptor - contains working information
 * about an open file (including the filesystem info) 
 */

typedef struct fatfile_s {
    fatfs_t *ff_fat;
    int ff_filelength;
    fatchain_t ff_chain;
    int ff_curpos;
    int ff_cursector;
    uint8_t ff_sector[SECTORSIZE];
} fatfile_t;

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

static int fatfs_fileop_xinit(void **fsctx,void *filename);
static int fatfs_fileop_pinit(void **fsctx,void *filename);
static int fatfs_fileop_open(void **ref,void *fsctx,char *filename,int mode);
static int fatfs_fileop_read(void *ref,uint8_t *buf,int len);
static int fatfs_fileop_write(void *ref,uint8_t *buf,int len);
static int fatfs_fileop_seek(void *ref,int offset,int how);
static void fatfs_fileop_close(void *ref);
static void fatfs_fileop_uninit(void *fsctx);

static int fatfs_check_for_partition_table(fatfs_t *fatfs);

/*  *********************************************************************
    *  FAT fileio dispatch table
    ********************************************************************* */

/*
 * Raw FAT (no partition table) - used only on floppies
 */

const fileio_dispatch_t fatfs_fileops = {
    "rfat",
    LOADFLG_NOBB,
    fatfs_fileop_xinit,
    fatfs_fileop_open,
    fatfs_fileop_read,
    fatfs_fileop_write,
    fatfs_fileop_seek,
    fatfs_fileop_close,
    fatfs_fileop_uninit
};

/*
 * Partitioned FAT - used on Zip disks, removable hard disks,
 * hard disks, flash cards, etc.
 */

const fileio_dispatch_t pfatfs_fileops = {
    "fat",
    LOADFLG_NOBB,
    fatfs_fileop_pinit,
    fatfs_fileop_open,
    fatfs_fileop_read,
    fatfs_fileop_write,
    fatfs_fileop_seek,
    fatfs_fileop_close,
    fatfs_fileop_uninit
};


/*  *********************************************************************
    *  fat_readsector(fatfs,sector,numsec,buffer)
    *  
    *  Read one or more sectors from the disk into memory
    *  
    *  Input parameters: 
    *  	   fatfs - fat filesystem descriptor
    *  	   sector - sector number
    *  	   numsec - number of sectors to read
    *  	   buffer - buffer to read sectors into
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int fat_readsector(fatfs_t *fatfs,int sector,int numsec,uint8_t *buffer)
{
    int res;

    res = cfe_readblk(fatfs->fat_fh,(sector+fatfs->fat_partstart)*SECTORSIZE,
		      buffer,numsec*SECTORSIZE);

    if (res != numsec*SECTORSIZE) return CFE_ERR_IOERR;

    return 0;
}


/*  *********************************************************************
    *  fat_dumpbpb(bpb)
    *  
    *  Debug function; display fields in a BPB
    *  
    *  Input parameters: 
    *  	   bpb - BIOS parameter block structure
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

#ifdef _FATFS_DEBUG_
static void fat_dumpbpb(bpb_t *bpb)
{
    xprintf("Bytes per sector    %d\n",bpb->bpb_bytespersector);
    xprintf("Sectors per cluster %d\n",bpb->bpb_sectorspercluster);
    xprintf("Reserved sectors    %d\n",bpb->bpb_reservedsectors);
    xprintf("Number of FATs      %d\n",bpb->bpb_numfats);
    xprintf("Root dir entries    %d\n",bpb->bpb_maxrootdir);
    xprintf("Total sectors       %d\n",bpb->bpb_totalsectors);
    xprintf("Sectors per FAT     %d\n",bpb->bpb_sectorsperfat);
    xprintf("Sectors per track   %d\n",bpb->bpb_sectorspertrack);
    xprintf("Number of heads     %d\n",bpb->bpb_numheads);
    xprintf("Hidden sectors      %d\n",bpb->bpb_hiddensectors);
}
#endif

/*  *********************************************************************
    *  fat_findpart(fatfs)
    *  
    *  For partitioned disks, locate the active partition
    *  and set "fat_partstart" accordingly
    *  
    *  Input parameters: 
    *  	   fatfs - FAT filesystem descriptor
    *  	   
    *  Return value:
    *  	   0 if we found a valid partition table; else error code
    ********************************************************************* */

static int fat_findpart(fatfs_t *fatfs)
{
    uint8_t buffer[SECTORSIZE];
    uint8_t *part;
    int res;
    int idx;

    fatfs->fat_partstart = 0;		/* make sure we get real boot sector */
    res = fat_readsector(fatfs,0,1,buffer);
    if (res < 0) return res;

    /*
     * Normally you're supposed to check for a JMP instruction.
     * At least that's what many people do.  Flash MBRs don't start
     * with JMP instructions, so just look for the seal.
     *
     *
     *  if (READBYTE(buffer,BPB_JMPINSTR) != BPB_JMPINSTR_VALUE) {
     *                 return CFE_ERR_BADFILESYS;
     *		       }
     */

    /*
     * Check the seal at the end of th sector
     */

    if (READWORD(buffer,BPB_SEAL) != BPB_SEAL_VALUE) return CFE_ERR_BADFILESYS;

    /*
     * Look for an active FAT partition.  The partition we want must
     * be the active one.   We do not deal with extended partitions
     * here.  Hey, this is supposed to be boot code!
     */

    part = &buffer[PTABLE_OFFSET];
    for (idx = 0; idx < PTABLE_COUNT; idx++) {
	if ((part[PTABLE_STATUS] == PTABLE_STATUS_ACTIVE) &&
	    ((part[PTABLE_TYPE] == PARTTYPE_FAT12) ||
	     (part[PTABLE_TYPE] == PARTTYPE_FAT16) ||
	     (part[PTABLE_TYPE] == PARTTYPE_FAT16BIG))) {
	    break;
	    }

	part += PTABLE_SIZE;
	}

    if (idx == PTABLE_COUNT) return CFE_ERR_BADFILESYS;

    /* 
     * The info we want is really just the pointer to the
     * boot (BPB) sector.  Get that and we'll use it for an
     * offset into the disk later.
     */

    fatfs->fat_partstart = READWORD32(part,PTABLE_BOOTSECTOR);

    return 0;
}


/*  *********************************************************************
    *  fat_readbpb(fatfs)
    *  
    *  Read and internalize the BIOS Parameter Block
    *  
    *  Input parameters: 
    *  	   fatfs - FAT filesystem descriptor
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code (usually, BPB is not valid)
    ********************************************************************* */

static int fat_readbpb(fatfs_t *fatfs)
{
    uint8_t buffer[SECTORSIZE];
    int res;

    res = fat_readsector(fatfs,0,1,buffer);
    if (res < 0) return res;

    if (READBYTE(buffer,BPB_JMPINSTR) != BPB_JMPINSTR_VALUE) return CFE_ERR_BADFILESYS;
    if (READWORD(buffer,BPB_SEAL) != BPB_SEAL_VALUE) return CFE_ERR_BADFILESYS;

    fatfs->fat_bpb.bpb_bytespersector = READWORD(buffer,BPB_BYTESPERSECTOR);
    fatfs->fat_bpb.bpb_sectorspercluster = READBYTE(buffer,BPB_SECTORSPERCLUSTER);
    fatfs->fat_bpb.bpb_reservedsectors = READWORD(buffer,BPB_RESERVEDSECTORS);
    fatfs->fat_bpb.bpb_numfats = READBYTE(buffer,BPB_NUMFATS);
    fatfs->fat_bpb.bpb_maxrootdir = READWORD(buffer,BPB_MAXROOTDIR);
    fatfs->fat_bpb.bpb_totalsectors = READWORD(buffer,BPB_TOTALSECTORS);
    fatfs->fat_bpb.bpb_sectorsperfat = READWORD(buffer,BPB_SECTORSPERFAT);
    fatfs->fat_bpb.bpb_sectorspertrack = READWORD(buffer,BPB_SECTORSPERTRACK);
    fatfs->fat_bpb.bpb_numheads = READWORD(buffer,BPB_NUMHEADS);
    fatfs->fat_bpb.bpb_hiddensectors = READWORD(buffer,BPB_HIDDENSECTORS);

    fatfs->fat_twelvebit = 1;
    if (memcmp(&buffer[0x36],"FAT16   ",8) == 0) fatfs->fat_twelvebit = 0;

    if (fatfs->fat_bpb.bpb_bytespersector != SECTORSIZE) return CFE_ERR_BADFILESYS;
    if (fatfs->fat_bpb.bpb_numfats > 2) return CFE_ERR_BADFILESYS;


#ifdef _FATFS_DEBUG_
    fat_dumpbpb(&(fatfs->fat_bpb));
#endif

    return 0;
}



/*  *********************************************************************
    *  fat_getentry(fatfs,entry)
    *  
    *  Read a FAT entry.  This is more involved than you'd think,
    *  since we have to deal with 12 and 16 (and someday 32) bit FATs,
    *  and the nasty case where a 12-bit FAT entry crosses a sector
    *  boundary.
    *  
    *  Input parameters: 
    *  	   fatfs - FAT filesystem descriptor
    *  	   entry - index of FAT entry
    *  	   
    *  Return value:
    *  	   FAT entry, or <0 if an error occured
    ********************************************************************* */

static int fat_getfatentry(fatfs_t *fatfs,int entry)
{
    int fatsect;
    int byteoffset;
    int fatstart;
    int fatoffset;
    uint8_t b1,b2,b3;
    int res;

    fatstart = fatfs->fat_bpb.bpb_reservedsectors;

    if (fatfs->fat_twelvebit) {
	int odd;
	odd = entry & 1;
	byteoffset = ((entry & ~1) * 3) / 2;
	fatsect = byteoffset / SECTORSIZE;
	fatoffset = byteoffset % SECTORSIZE;

	if (fatfs->fat_fatsecnum != fatsect) {
	    res = fat_readsector(fatfs,fatsect+fatstart,1,fatfs->fat_fatsector);
	    if (res < 0) {
		return res;
		}
	    fatfs->fat_fatsecnum = fatsect;
	    }

	b1 = fatfs->fat_fatsector[fatoffset];

	if ((fatoffset+1) >= SECTORSIZE) {
	    res = fat_readsector(fatfs,fatsect+1+fatstart,1,fatfs->fat_fatsector);
	    if (res < 0) {
		return res;
		}
	    fatfs->fat_fatsecnum = fatsect+1;
	    fatoffset -= SECTORSIZE;
	    }
	    
	b2 = fatfs->fat_fatsector[fatoffset+1];

	if ((fatoffset+2) >= SECTORSIZE) {
	    res = fat_readsector(fatfs,fatsect+1+fatstart,1,fatfs->fat_fatsector);
	    if (res < 0) {
		return res;
		}
	    fatfs->fat_fatsecnum = fatsect+1;
	    fatoffset -= SECTORSIZE;
	    }

	b3 = fatfs->fat_fatsector[fatoffset+2];

	if (odd) {
	    return ((unsigned int) b3 << 4) + ((unsigned int) (b2 & 0xF0) >> 4);
	    }
	else {
	    return ((unsigned int) (b2 & 0x0F) << 8) + ((unsigned int) b1);
	    }

	}
    else {
	byteoffset = entry * 2;
	fatsect = byteoffset / SECTORSIZE;
	fatoffset = byteoffset % SECTORSIZE;

	if (fatfs->fat_fatsecnum != fatsect) {
	    res = fat_readsector(fatfs,fatsect+fatstart,1,fatfs->fat_fatsector);
	    if (res < 0) {
		return res;
		}
	    fatfs->fat_fatsecnum = fatsect;
	    }

	b1 = fatfs->fat_fatsector[fatoffset];
	b2 = fatfs->fat_fatsector[fatoffset+1];
	return ((unsigned int) b1) + (((unsigned int) b2) << 8);
	}
}

/*  *********************************************************************
    *  fat_getrootdirentry(fatfs,entryidx,entry)
    *  
    *  Read a root directory entry.  The FAT12/16 root directory
    *  is a contiguous group of sectors, whose size is specified in
    *  the BPB.  This routine just digs out an entry from there
    *  
    *  Input parameters: 
    *  	   fatfs - FAT filesystem descriptor
    *  	   entryidx - 0-based entry index to read
    *  	   entry - pointer to directory entry (32 bytes)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   <0 if error occured
    ********************************************************************* */

static int fat_getrootdirentry(fatfs_t *fatfs,int entryidx,uint8_t *entry)
{
    int rootdirstart;
    int rootdirsize;
    int dirsecnum;
    int res;

    if (entryidx >= fatfs->fat_bpb.bpb_maxrootdir) {
	memset(entry,0,DIRENTRYSIZE);
	return CFE_ERR_INV_PARAM;
	}

    rootdirstart = fatfs->fat_bpb.bpb_reservedsectors + 
	fatfs->fat_bpb.bpb_numfats * fatfs->fat_bpb.bpb_sectorsperfat;

    rootdirsize = fatfs->fat_bpb.bpb_maxrootdir / DIRENTRYSIZE;

    dirsecnum = rootdirstart + entryidx / DIRPERSECTOR;

    if (fatfs->fat_dirsecnum != dirsecnum) {
	res = fat_readsector(fatfs,dirsecnum,1,fatfs->fat_dirsector);
	if (res < 0) {
	    return res;
	    }
	fatfs->fat_dirsecnum = dirsecnum;
	}

    memcpy(entry,&(fatfs->fat_dirsector[(entryidx % DIRPERSECTOR)*DIRENTRYSIZE]),
	   DIRENTRYSIZE);

    return 0;
}

/*  *********************************************************************
    *  fat_checksumname(name)
    *  
    *  Calculate the "long filename" checksum for a given short name.
    *  All LFN directory entries associated with the short name are
    *  given the same checksum byte, to help keep the long name
    *  consistent.
    *  
    *  Input parameters: 
    *  	   name - pointer to 32-byte directory entry
    *  	   
    *  Return value:
    *  	   checksum value
    ********************************************************************* */

static uint8_t fat_checksumname(uint8_t *name)
{
   uint8_t sum = 0;
   uint8_t newbit;
   int idx;

   for (idx = 0; idx < 11; idx++) {
       newbit = (sum & 1) ? 0x80 : 0x00;
       sum >>= 1;     
       sum |= newbit;
       sum += name[idx];
       }

   return sum;
}

#ifdef _FATFS_DEBUG_
void fat_dumpdirentry(uint8_t *entry);
void fat_dumpdirentry(uint8_t *entry)
{
    uint8_t name[32];
    int idx;

    if (entry[11] != ATTRIB_LFN) {
	memcpy(name,entry,11);
	name[11] = 0;
	xprintf("%s   %02X %04X %d\n",
	       name,DIRENTRY_ATTRIB(entry),
	       DIRENTRY_STARTCLUSTER(entry),
	       DIRENTRY_FILELENGTH(entry));
	}
    else {
	for (idx = 0; idx < 5; idx++) {
	    name[idx] = entry[(idx*2)+1];
	    }
	for (idx = 0; idx < 6; idx++) {
	    name[idx+5] = entry[(idx*2)+14];
	    }
	for (idx = 0; idx < 2; idx++) {
	    name[idx+11] = entry[(idx*2)+28];
	    }
	name[13] = '\0';
	xprintf("%02X: %s   %04X  cksum %02X\n",entry[0],
	       name,READWORD(entry,0x1A),entry[13]);
	}
}
#endif


/*  *********************************************************************
    *  fat_walkfatchain(fat,start,arg,func)
    *  
    *  Walk a FAT chain, calling a callback routine for each entry
    *  we find along the way.
    *  
    *  Input parameters: 
    *  	   fat - FAT filesystem descriptor
    *  	   start - starting FAT entry (from the directory, usually)
    *  	   arg - argument to pass to callback routine
    *  	   func - function to call for each FAT entry
    *  	   
    *  Return value:
    *  	   0 if all elements processed
    *  	   <0 if error occured
    *  	   >0 if callback routine returned a nonzero value
    ********************************************************************* */

static int fat_walkfatchain(fatfs_t *fat,int start,
		     void *arg,
		     int (*func)(fatfs_t *fat,
				 int e,
				 int prev_e,
				 void *arg))
{
    int prev_e = 0;
    int ending_e;
    int e;
    int res = 0;

    e = start;

    /* Note: ending FAT entry can be 0x(F)FF8..0x(F)FFF. We assume that the
       'getfatentry' call won't return values above that. */
    if (fat->fat_twelvebit) {
	ending_e = 0xFF8;
	}
    else {
	ending_e = 0xFFF8;
	}

    while (e < ending_e) {
	res = (*func)(fat,e,prev_e,arg);
	if (res) break;
	prev_e = e;
	e = fat_getfatentry(fat,e);
	if (e < 0) return e;
	}

    return res;
}

/*  *********************************************************************
    *  fat_getwalkfunc(fat,e,prev_e,arg)
    *  
    *  Callback routien to collect all of the FAT entries into
    *  a FAT chain descriptor
    *  
    *  Input parameters: 
    *  	   fat - FAT filesystem descriptor
    *  	   e - current entry
    *  	   prev_e - previous entry (0 if first entry)
    *  	   arg - argument passed to fat_walkfatchain
    *  	   
    *  Return value:
    *  	   0 to keep walking
    *  	   else value to return from fat_walkfatchain
    ********************************************************************* */

static int fat_getwalkfunc(fatfs_t *fat,int e,
			   int prev_e,void *arg)
{
    fatchain_t *chain = arg;

    if (chain->fat_entries) {
	chain->fat_entries[chain->fat_count] = (uint16_t) e;
	}

    chain->fat_count++;

    return 0;
}

/*  *********************************************************************
    *  fat_getchain(fat,start,chain)
    *  
    *  Walk an entire FAT chain and remember the chain in a
    *  FAT chain descriptor
    *  
    *  Input parameters: 
    *  	   fat - FAT filesystem descriptor
    *  	   start - starting FAT entry
    *  	   chain - chain descriptor
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int fat_getchain(fatfs_t *fat,int start,fatchain_t *chain)
{
    int res;

    chain->fat_entries = NULL;
    chain->fat_count = 0;
    chain->fat_start = start;

    /* 
     * walk once to count the entries. 
     *
     * For regular files, you probably don't have to do this
     * since you can predict exactly how many FAT entries
     * there are given the file length.
     */

    res = fat_walkfatchain(fat,start,chain,fat_getwalkfunc);
    if (res < 0) return res;

    /* 
     * allocate space for the entries. Include one extra
     * slot for the first entry, since the first entry
     * does not actually appear in the FAT (the fat is
     * only the 'next' pointers).
     */

    if (chain->fat_count == 0) return 0;
    chain->fat_entries = KMALLOC((chain->fat_count+1)*sizeof(uint16_t),0);
    chain->fat_count = 0;

    /* 
     * walk again to collect entries 
     */
    res = fat_walkfatchain(fat,start,chain,fat_getwalkfunc);
    if (res < 0) return res;

    return chain->fat_count;
}


/*  *********************************************************************
    *  fat_freechain(chain)
    *  
    *  Free memory associated with a FAT chain
    *  
    *  Input parameters: 
    *  	   chain - chain descriptor
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void fat_freechain(fatchain_t *chain)
{
    if (chain->fat_entries) {
	KFREE(chain->fat_entries);
	chain->fat_entries = NULL;
	}
    chain->fat_count = 0;
}

/*  *********************************************************************
    *  fat_clusteridx(fat,chain,idx)
    *  
    *  Index into a FAT chain and return the nth cluster number
    *  from the chain
    *  
    *  Input parameters: 
    *  	   fat - fat filesystem descriptor
    *  	   chain - chain descriptor
    *  	   idx - index into FAT chain
    *  	   
    *  Return value:
    *  	   FAT entry at the nth index, or
    *  	   <0 if an error occured
    ********************************************************************* */
static int fat_clusteridx(fatfs_t *fat,fatchain_t *chain,int idx)
{
    if (idx >= chain->fat_count) return CFE_ERR_INV_PARAM;	/* error! */
    return (int) (unsigned int) chain->fat_entries[idx];
}

/*  *********************************************************************
    *  fat_sectoridx(fat,chain,idx)
    *  
    *  Return the sector nunber of the nth sector in a given
    *  FAT chain.  This routine knows how to translate cluster
    *  numbers into sector numbers.
    *  
    *  Input parameters: 
    *  	   fat - FAT filesystem descriptor
    *  	   chain - FAT chain
    *  	   idx - index of which sector to find
    *  	   
    *  Return value:
    *  	   sector number 
    *  	   <0 if an error occured
    ********************************************************************* */
static int fat_sectoridx(fatfs_t *fat,fatchain_t *chain,int idx)
{
    int clusteridx;
    int sectoridx;
    int sector;
    int fatentry;

    clusteridx = idx / fat->fat_bpb.bpb_sectorspercluster;
    sectoridx = idx % fat->fat_bpb.bpb_sectorspercluster;

    fatentry = fat_clusteridx(fat,chain,clusteridx);
    
    if (fatentry < 0) xprintf("ran off end of fat chain!\n");
    if (fatentry < 2) xprintf("fat entries should be >= 2\n");

    sector = fat->fat_bpb.bpb_reservedsectors + 
	(fat->fat_bpb.bpb_maxrootdir * DIRENTRYSIZE)/SECTORSIZE +
	(fat->fat_bpb.bpb_numfats * fat->fat_bpb.bpb_sectorsperfat) +
	(fatentry - 2) * fat->fat_bpb.bpb_sectorspercluster +
	sectoridx;

    return sector;
}

/*  *********************************************************************
    *  fat_getsubdirentry(fat,chain,idx,direntry)
    *  
    *  This routine is similar to fat_getrootdirentry except it
    *  works on a subdirectory.  FAT subdirectories are like files
    *  containing directory entries, so we use the "get nth sector
    *  in chain" routines to walk the chains of sectors reading directory
    *  entries.
    *  
    *  Input parameters: 
    *      fat - FAT filesystem descriptor
    *  	   chain - FAT chain
    *  	   idx - index of entry to read
    *  	   direntry - place to put directory entry we read
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int fat_getsubdirentry(fatfs_t *fat,fatchain_t *chain,
		       int idx,uint8_t *direntry)
{
    int sector;
    int res;

    sector = fat_sectoridx(fat,chain,idx/DIRPERSECTOR);

    if (sector < 0) return sector;

    if (fat->fat_dirsecnum != sector) {
	res = fat_readsector(fat,sector,1,fat->fat_dirsector);
	if (res < 0) {
	    return res;
	    }
	fat->fat_dirsecnum = sector;
	}

    memcpy(direntry,&(fat->fat_dirsector[(idx % DIRPERSECTOR)*DIRENTRYSIZE]),
	   DIRENTRYSIZE);

    return 0;
}

/*  *********************************************************************
    *  fat_getshortname(direntry,name)
    *  
    *  Read the short filename from a directory entry, converting
    *  it into its classic 8.3 form
    *  
    *  Input parameters: 
    *  	   direntry - directory entry
    *  	   name - place to put 8.3 name
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void fat_getshortname(uint8_t *direntry,char *name)
{
    int idx;

    /*
     * Collect the base file name
     */

    for (idx = DIR_NAMEOFFSET; idx < (DIR_NAMEOFFSET+DIR_NAMELEN); idx++) {
	if (direntry[idx] == ' ') break;
	*name++ = direntry[idx];
	}

    /*
     * Put in the dot for the extension only if there
     * is an extension.
     */

    if (direntry[DIR_EXTOFFSET] != ' ') *name++ = '.';

    /* 
     * Collect the extension
     */

    for (idx = DIR_EXTOFFSET; idx < (DIR_EXTOFFSET+DIR_EXTLEN); idx++) {
	if (direntry[idx] == ' ') break;
	*name++ = direntry[idx];
	}

    *name = '\0';
}


/*  *********************************************************************
    *  fat_getlongname(fat,chain,diridx,shortentry,longname)
    *  
    *  Look backwards in the directory to locate the long file name
    *  that corresponds to the short file name passed in 'shortentry'
    *  
    *  Input parameters: 
    *  	   fat - FAT filesystem descriptor
    *  	   chain - chain describing current directory, or NULL
    *  	           if the current directory is the root directory
    *  	   diridx - index of the short file name
    *  	   shortentry - points to the short directory entry
    *  	   longname - buffer to receive long file name (up to 261 chars)
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int fat_getlongname(fatfs_t *fat,fatchain_t *chain,int diridx,
			   uint8_t *shortentry,char *longname)
{
    int lfnidx = 1;
    uint8_t checksum;
    uint8_t direntry[DIRENTRYSIZE];
    int idx;
    char *lfnptr;
    int badlfn = 0;

    *longname = '\0';

    /*
     * idx is the entry # of the short name
     */

    checksum = fat_checksumname(shortentry);

    /*
     * Start working backwards from current entry
     * and collect pieces of the lfn
     */

    lfnptr = longname;
    diridx--;

    while (diridx > 0) {

	/*
	 * Read previous entry 
	 */

	if (chain) {
	    fat_getsubdirentry(fat,chain,diridx,direntry);
	    }
	else {
	    fat_getrootdirentry(fat,diridx,direntry);
	    }

	/*
	 * Checksum must match, it must have the right entry index,
	 * and it must have the LFN attribute
	 */

	if (DIRENTRY_CHECKSUM(direntry) != checksum) {
	    badlfn = 1;
	    break;
	    }
	if ((DIRENTRY_LFNIDX(direntry) & LFNIDX_MASK) != lfnidx) {
	    badlfn = 1;
	    break;
	    }

	if (DIRENTRY_ATTRIB(direntry) != ATTRIB_LFN) {
	    badlfn = 1;
	    break;
	    }

	/*
	 * Collect the chars from the filename.  Note we
	 * don't deal well with real unicode chars here.
	 */

	for (idx = 0; idx < 5; idx++) {
	    *lfnptr++ = direntry[(idx*2)+1];
	    }
	for (idx = 0; idx < 6; idx++) {
	    *lfnptr++ = direntry[(idx*2)+14];
	    }
	for (idx = 0; idx < 2; idx++) {
	    *lfnptr++ = direntry[(idx*2)+28];
	    }

	/*	
	 * Don't go too far
	 */

	if (DIRENTRY_LFNIDX(direntry) & LFNIDX_END) break;
	lfnidx++;
	if (lfnidx > LFNIDX_MAX) {
	    badlfn = 1;
	    break;
	    }

	diridx--;
	}

    /*
     * Null terminate the lfn
     */

    *lfnptr = 0;

    if (badlfn) {
	longname[0] = 0;
	return CFE_ERR_FILENOTFOUND;
	}

    return 0;
}


/*  *********************************************************************
    *  fat_scandir(fat,chain,name,direntry)
    *  
    *  Scan a single directory looking for a file name
    *  
    *  Input parameters: 
    *  	   fat - FAT filesystem descriptor
    *  	   chain - FAT chain for directory or NULL for root directory
    *  	   name - name of file to look for (short or long name)
    *  	   direntry - place to put directory entry if we find one
    *  	   
    *  Return value:
    *  	   1 if name was found
    *      0 if name was not found
    *  	   else <0 is error code
    ********************************************************************* */


static int fat_scandir(fatfs_t *fat,fatchain_t *chain,
		       char *name,uint8_t *direntry)
{
    int idx;
    int count;
    char shortname[16];
    char longname[280];

    /*
     * Get directory size
     */

    if (chain) {
	count = (chain->fat_count * fat->fat_bpb.bpb_sectorspercluster) * DIRPERSECTOR;
	}
    else {
	count = (int) fat->fat_bpb.bpb_maxrootdir;
	}

    /*
     * Scan whole directory
     */

    for (idx = 0; idx < count; idx++) {

	/*
	 * Get entry by root or chain depending...
	 */

	if (chain) {
	    fat_getsubdirentry(fat,chain,idx,direntry);
	    }
	else {
	    fat_getrootdirentry(fat,idx,direntry);
	    }

	/*
	 * Ignore stuff we don't want to see
	 */

	if (direntry[0] == DIRENTRY_LAST) break;	/* stop if at end of dir */
	if (direntry[0] == DIRENTRY_DELETED) continue;	/* skip deleted entries */
	if (direntry[0] == DIRENTRY_PARENTDIR) continue; /* skip ./.. entries */

	if (DIRENTRY_ATTRIB(direntry) == ATTRIB_LFN) continue;	/* skip LFNs */
	if (DIRENTRY_ATTRIB(direntry) & ATTRIB_LABEL) continue;	/* skip volume labels */

	/*
	 * Get actual file names from directory
	 */

	fat_getshortname(direntry,shortname);
	fat_getlongname(fat,chain,idx,direntry,longname);


	if (name) {
	    if (strcmpi(name,shortname) == 0) return 1;
	    if (longname[0] && (strcmpi(name,longname) == 0)) return 1;
	    }
	else {
	    xprintf("%-30s",longname[0] ? longname : shortname);
//	    xprintf(" Clus=%04X",DIRENTRY_STARTCLUSTER(direntry));
//	    xprintf(" Attrib=%02X",DIRENTRY_ATTRIB(direntry));
//	    xprintf(" Size=%d",DIRENTRY_FILELENGTH(direntry));
	    xprintf("%d",DIRENTRY_FILELENGTH(direntry));
	    xprintf("\n");
	    }
	}

    return 0;
}

/*  *********************************************************************
    *  fat_findfile(fat,name,direntry)
    *  
    *  Locate a directory entry given a complete path name
    *  
    *  Input parameters: 
    *  	   fat - FAT filesystem descriptor
    *  	   name - name of file to locate (forward or reverse slashses ok)
    *  	   direntry - place to put directory entry we find
    *  	   
    *  Return value:
    *  	   0 if file not found
    *  	   1 if file was found
    *  	   <0 if error occurs
    ********************************************************************* */

static int fat_findfile(fatfs_t *fat,char *name,uint8_t *direntry)
{
    char *namecopy;
    char *namepart;
    char *ptr;
    fatchain_t chain;
    int res;
    int e;

    /* 
     * Copy the name, we're going to hack it up 
     */

    namecopy = strdup(name);

    /*
     * Chew off the first piece up to the first slash.  Remove
     * a leading slash if it is there.
     */

    ptr = namecopy;

    if ((*ptr == '/') || (*ptr == '\\')) ptr++;

    namepart = ptr;
    while (*ptr && (*ptr != '/') && (*ptr != '\\')) ptr++;
    if (*ptr) *ptr++ = '\0';
   
    /*
     * Scan the root directory looking for the first piece
     */

    res = fat_scandir(fat,NULL,namepart,direntry);
    if (res == 0) {
	KFREE(namecopy);
	return 0;		/* file not found */
	}


    /*
     * Start scanning subdirectories until we run out
     * of directory components.
     */

    namepart = ptr;
    while (*ptr && (*ptr != '/') && (*ptr != '\\')) ptr++;
    if (*ptr) *ptr++ = '\0';
    if (!*namepart) namepart = NULL;


    while (namepart) {

	/*
	 * Scan the subdirectory
	 */

	e = DIRENTRY_STARTCLUSTER(direntry);
	memset(&chain,0,sizeof(chain));
	fat_getchain(fat,e,&chain);
	res = fat_scandir(fat,&chain,namepart,direntry);
	if (res == 0) {
	    break;
	    }
	fat_freechain(&chain);

	/*
	 * Advance to the next piece
	 */

	namepart = ptr;
	while (*ptr && (*ptr != '/') && (*ptr != '\\')) ptr++;
	if (*ptr) *ptr++ = '\0';
	if (!*namepart) namepart = NULL;

	/*
	 * If there's more to go and we hit something that
	 * is not a directory, stop here.
	 */

	if (namepart && !(DIRENTRY_ATTRIB(direntry) & ATTRIB_DIR)) {
	    res = 0;
	    }
	}

    KFREE(namecopy);

    /*
     * The last piece we enumerate has to be a file.
     */

    if ((res > 0) &&
	(DIRENTRY_ATTRIB(direntry) & ATTRIB_DIR)) {
	return 0;
	}

    return res;
}


/*  *********************************************************************
    *  fat_init(fat,name)
    *  
    *  Create the filesystem descriptor and attach to the hardware
    *  device.
    *  
    *  Input parameters: 
    *  	   fat - filesystem descriptor
    *  	   name - hardware device name
    *	   part - true to look for partition tables
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int fat_init(fatfs_t *fat,char *name,int part)
{
    int res;

    memset(fat,0,sizeof(fatfs_t));
    fat->fat_dirsecnum = -1;
    fat->fat_fatsecnum = -1;

    fat->fat_fh = cfe_open(name);

    if (fat->fat_fh < 0) return fat->fat_fh;

    res = fatfs_check_for_partition_table(fat);
    /* If we were able to figure it out, use that as the default */
    if (res >= 0) part = res;

    if (part) {
	res = fat_findpart(fat);
	if (res < 0) {
	    cfe_close(fat->fat_fh);
	    fat->fat_fh = -1;
	    return res;
	    }
	}

    res = fat_readbpb(fat);
    if (res != 0) {
	cfe_close(fat->fat_fh);
	fat->fat_fh = -1;
	return res;
	}

    return 0;
}

/*  *********************************************************************
    *  fat_uninit(fat)
    *  
    *  Uninit the filesystem descriptor and release any resources
    *  we allocated.
    *  
    *  Input parameters: 
    *  	   fat - filesystem descriptor
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void fat_uninit(fatfs_t *fat)
{
    if (fat->fat_fh >= 0) cfe_close(fat->fat_fh);
    fat->fat_fh = -1;
}

int fatfs_fileop_dir(void *fsctx);
int fatfs_fileop_dir(void *fsctx)
{
    fatfs_t *fatfs = fsctx;
    uint8_t direntry[32];

    fat_scandir(fatfs,NULL,NULL,direntry);

    return 0;
}

/*  *********************************************************************
    *  fatfs_fileop_init(fsctx,devname)
    *  
    *  Create a FAT filesystem context and open the associated
    *  block device.
    *  
    *  Input parameters: 
    *  	   fsctx - file system context (return pointer)
    *  	   devname - device name to open 
    *	   part - true to look for a partition table
    *  	   
    *  Return value:
    *  	   0 if ok, else error
    ********************************************************************* */

static int fatfs_fileop_init(void **fsctx,char *devname,int part)
{
    int res;
    fatfs_t *fatfs;

    /*
     * Allocate a file system context
     */

    fatfs = (fatfs_t *) KMALLOC(sizeof(fatfs_t),0);
    if (!fatfs) return CFE_ERR_NOMEM;

    /*
     * Open a handle to the underlying device
     */

    res = fat_init(fatfs,devname,part);
    if (res != 0) {
	KFREE(fatfs);
	return res;
	}

    *fsctx = fatfs;

    return 0;
}

/*  *********************************************************************
    *  fatfs_check_for_partition_table(fatfs)
    *  
    *  This routine attempts to determine if the disk contains a 
    *  partition table or if it contains a standard MS-DOS boot recod.
    *  We try to find both, and return what we find, or an error
    *  if it is still unclear.
    *  
    *  Input parameters: 
    *  	   fatfs - fat filesystem context
    *  	   
    *  Return value:
    *  	   0 if no partition table
    *  	   1 if partition table
    *  	   <0 = error occured, could not tell or I/O error
    ********************************************************************* */

static int fatfs_check_for_partition_table(fatfs_t *fatfs)
{
    int res;
    uint8_t buffer[SECTORSIZE];
    uint8_t *part;
    int idx;
    int foundit = 0;

    /*
     * Read sector 0
     */

    fatfs->fat_partstart = 0;     
    res = fat_readsector(fatfs,0,1,buffer);
    if (res < 0) return res;

    /*
     * Check the seal at the end of th sector.  Both
     * boot sector and MBR should contain this seal.
     */
    if (READWORD(buffer,BPB_SEAL) != BPB_SEAL_VALUE) {
	res =  CFE_ERR_BADFILESYS;
	return res;
	}

    /*
     * See Microsoft Knowledgebase article # Q140418, it contains
     * a good description of the boot sector format.
     * 
     * If the extended information is present, and SystemID is "FAT" 
     * and the "bytes per sector" is 512, assume it's a regular boot block
     */

    if (((buffer[BPB_SIGNATURE] == BPB_SIGNATURE_VALUE1) || 
	 (buffer[BPB_SIGNATURE] == BPB_SIGNATURE_VALUE2)) &&
	(memcmp(&buffer[BPB_SYSTEMID],"FAT",3) == 0) &&
	 (READWORD(buffer,BPB_BYTESPERSECTOR) == 512)) {
	/* Not partitioned */
	res = 0;
	return res;
	}

    /* If no extended information is present, check a few other key values. */

    if ((READWORD(buffer,BPB_BYTESPERSECTOR) == 512) &&
	(READWORD(buffer,BPB_RESERVEDSECTORS) >= 1) &&
	((READWORD(buffer,BPB_MEDIADESCRIPTOR) & 0xF0) == 0xF0)) {
	res = 0;
	return res;
	}

    /* 
     * If we're still confused, look for a partition table with a valid FAT 
     * partition on it.  We might not detect a partition table that has
     * only non-FAT partitions on it, like a disk with all Linux partitions,
     * but that is fine here in the FATFS module, since we only want to
     * find FAT partitions anyway.
     */
    part = &buffer[PTABLE_OFFSET];
    for (idx = 0; idx < PTABLE_COUNT; idx++) {

	if (((part[PTABLE_STATUS] == PTABLE_STATUS_ACTIVE) || 
	     (part[PTABLE_STATUS] == 0x00)) &&
	    ((part[PTABLE_TYPE] == PARTTYPE_FAT12) ||
	     (part[PTABLE_TYPE] == PARTTYPE_FAT16) ||
	     (part[PTABLE_TYPE] == PARTTYPE_FAT16BIG))) {
	    foundit = 1;
	    res = 1; /*Partition table present*/
	    break;
	    }
	part += PTABLE_SIZE;
	}
 
    /*
     * If at this point we did not find what we were looking for,
     * return an error.
     */
    if (foundit) {
	res = 1; /*Partition table is present.*/
	}
    else {
	/*Error!  We can't decide if partition table exists or not*/
	res = CFE_ERR_BADFILESYS;
	}

    return res;
}

static int fatfs_fileop_xinit(void **fsctx,void *dev)
{
    char *devname = (char *) dev;

    return fatfs_fileop_init(fsctx,devname,0);
}

static int fatfs_fileop_pinit(void **fsctx,void *dev)
{
    char *devname = (char *) dev;

    return fatfs_fileop_init(fsctx,devname,1);
}



/*  *********************************************************************
    *  fatfs_fileop_open(ref,name)
    *  
    *  Open a file on the FAT device.
    *  
    *  Input parameters: 
    *  	   ref - place to store pointer to fileinfo
    *      fsctx - filesystem context
    *  	   name - name of file to open
    *  	   
    *  Return value:
    *  	   0 if ok
    *  	   else error code
    ********************************************************************* */

static int fatfs_fileop_open(void **ref,void *fsctx,char *name,int mode)
{
    int res;
    uint8_t direntry[DIRENTRYSIZE];
    fatfile_t *ff;
    fatfs_t *fatfs;

    if (mode != FILE_MODE_READ) return CFE_ERR_UNSUPPORTED;

    fatfs = (fatfs_t *) fsctx;

    ff = (fatfile_t *) KMALLOC(sizeof(fatfile_t),0);
    if (ff == NULL) return CFE_ERR_NOMEM;

    memset(ff,0,sizeof(fatfile_t));

    ff->ff_fat = fatfs;

    res = fat_findfile(ff->ff_fat,name,direntry);
    if (res <= 0) {
	return CFE_ERR_FILENOTFOUND;		/* not found */
	}

    /*
     * Okay, the file was found.  Enumerate the FAT chain
     * associated with this file.
     */

    ff->ff_filelength = DIRENTRY_FILELENGTH(direntry);

    ff->ff_curpos = 0;
    ff->ff_cursector = -1;

    res = fat_getchain(ff->ff_fat,
		       DIRENTRY_STARTCLUSTER(direntry),
		       &(ff->ff_chain));

    if (res < 0) {
	KFREE(ff);
	return res;
	}

    /*
     * Return the file handle
     */


    fatfs->fat_refcnt++;
    *ref = (void *) ff;
    return 0;
}


/*  *********************************************************************
    *  fatfs_fileop_close(ref)
    *  
    *  Close the file.
    *  
    *  Input parameters: 
    *  	   ref - pointer to open file information
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */

static void fatfs_fileop_close(void *ref)
{
    fatfile_t *file = (fatfile_t *) ref;
    fatfs_t *fatctx = file->ff_fat;

    fatctx->fat_refcnt--;

    fat_freechain(&(file->ff_chain));
    KFREE(file);
}


/*  *********************************************************************
    *  fatfs_fileop_uninit(ref)
    *  
    *  Uninitialize the file system.
    *  
    *  Input parameters: 
    *  	   fsctx - filesystem context
    *  	   
    *  Return value:
    *  	   nothing
    ********************************************************************* */
static void fatfs_fileop_uninit(void *fsctx)
{
    fatfs_t *fatctx = (fatfs_t *) fsctx;

    if (fatctx->fat_refcnt) {
	xprintf("fatfs_fileop_unint: warning: refcnt should be zero\n");
	}

    fat_uninit(fatctx);

    KFREE(fatctx);
}


/*  *********************************************************************
    *  fatfs_fileop_seek(ref,offset,how)
    *  
    *  Move the file pointer within the file
    *  
    *  Input parameters: 
    *  	   ref - pointer to open file information
    *  	   offset - new file location or distance to move
    *  	   how - method for moving
    *  	   
    *  Return value:
    *  	   new file offset
    *  	   <0 if error occured
    ********************************************************************* */

static int fatfs_fileop_seek(void *ref,int offset,int how)
{
    fatfile_t *file = (fatfile_t *) ref;

    switch (how) {
	case FILE_SEEK_BEGINNING:
	    file->ff_curpos = offset;
	    break;
	case FILE_SEEK_CURRENT:
	    file->ff_curpos += offset;
	    break;
	default:
	    break;
	}

    if (file->ff_curpos >= file->ff_filelength) {
	file->ff_curpos = file->ff_filelength;
	}

    return file->ff_curpos;
}


/*  *********************************************************************
    *  fatfs_fileop_read(ref,buf,len)
    *  
    *  Read data from the file.
    *  
    *  Input parameters: 
    *  	   ref - pointer to open file information
    *  	   buf - buffer to read data into
    *  	   len - number of bytes to read
    *  	   
    *  Return value:
    *  	   number of bytes read
    *  	   <0 if error occured
    *  	   0 means eof
    ********************************************************************* */

static int fatfs_fileop_read(void *ref,uint8_t *buf,int len)
{
    fatfile_t *file = (fatfile_t *) ref;
    int amtcopy;
    int ttlcopy = 0;
    int offset;
    int sector;
    int secidx;
    int origpos;
    int res;
    uint8_t temp_buf[SECTORSIZE];

    /*
     * Remember orig position in case we have an error
     */

    origpos = file->ff_curpos;

    /*
     * bounds check the length based on the file length
     */

    if ((file->ff_curpos + len) > file->ff_filelength) {
	len = file->ff_filelength - file->ff_curpos;
	}

    res = 0;

    /*
     * while ther is still data to be transferred
     */
   

    while (len) {

	/*
	 * Calculate the sector offset and index in the sector
	 */

	offset = file->ff_curpos % SECTORSIZE;
	secidx = file->ff_curpos / SECTORSIZE;

	sector = fat_sectoridx(file->ff_fat,&(file->ff_chain),secidx);

	if (sector < 0) {
	    xprintf("should not happen, sector = -1!\n");	
	    return sector;
	    }

	/*
	 * first transfer up to the sector boundary
	 */

	amtcopy = len;
	if (amtcopy > (SECTORSIZE-offset)) {
	    amtcopy = (SECTORSIZE-offset);
	    }

	/*
	 * If transferring exactly a sector, on a sector
	 * boundary, read the data directly into the user buffer
	 *
	 * Extra credit:  See if we can transfer more than one
	 * sector at a time, by determining if we can read a run of
	 * contiguous sectors (very likely)
	 *
	 * Otherwise: read into the sector buffer and 
	 * transfer the data to user memory.
	 */

	if ((offset == 0) && (amtcopy == SECTORSIZE)) {
	    res = fat_readsector(file->ff_fat,sector,1,temp_buf);
	    if (res < 0) {
		xprintf("I/O error!\n");
		break;
		}
	    memcpy(buf,temp_buf,amtcopy);
	    }
	else {
	    if (file->ff_cursector != sector) {
		res = fat_readsector(file->ff_fat,sector,1,file->ff_sector);
		if (res < 0) {
		    break;
		    }
		file->ff_cursector = sector;
		}
	    memcpy(buf,&(file->ff_sector[offset]),amtcopy);
	    }

	/*
	 * Adjust/update all our pointers.
	 */

	buf += amtcopy;
	file->ff_curpos += amtcopy;
	ttlcopy += amtcopy;
	len -= amtcopy;

	/*
	 * see if we ran off the end of the file.  Should not
	 * be necessary.
	 */

	if (file->ff_curpos >= file->ff_filelength) {
	    /* should not be necessary */
	    break;
	    }
	}

    /*
     * If an error occured, get out now.
     */

    if (res < 0) {
	file->ff_curpos = origpos;
	return res;
	}

    return ttlcopy;

}

static int fatfs_fileop_write(void *ref,uint8_t *buf,int len)
{
    return CFE_ERR_UNSUPPORTED;
}
