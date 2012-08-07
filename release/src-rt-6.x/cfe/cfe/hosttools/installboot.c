/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Boot program installer			File: installboot.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *  This program converts a binary file (bootstrap program)
    *  into a boot block by prepending the boot block sector
    *  to the program.  It generates the appropriate
    *  boot block checksums and such.  The resulting file may
    *  be used by installboot (for example) to put into a disk
    *  image for the simulator.
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

#include <sys/types.h>
#include <sys/stat.h>

typedef unsigned long long uint64_t;
typedef unsigned long uint32_t;

#include "cfe_bootblock.h"
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define roundup(x,align) (((x)+(align)-1)&~((align)-1))
#define howmany(x,align) (((x)+(align)-1)/(align))

static int verbose = 0;
static int big_endian = 1;
static int swapflg = 1;
static unsigned long bootcode_offset = 2;
static unsigned long bootsect_offset = 1;

static void usage(void)
{
    fprintf(stderr,"usage: installboot [-bootsect <sector offset>] [-bootcode <sector offset>]\n"
	           "                   [-v] [-EB] [-EL] bootloaderfile.bin devicefile\n\n");
    fprintf(stderr,"This program installs a boot block onto a disk.  Typically, installboot\n"
	           "is used to install sibyl or other OS bootloaders.  The binary you install\n"
	           "should be a raw program (not ELF) located to run in CFE's boot area\n"
	           "at address 0x2000_0000.  The devicefile should be the name of the raw device\n"
	           "such as /dev/hda on Linux.\n\n"
	           "Care must be taken when choosing the values for -bootsect and -bootcode\n"
	           "not to interfere with other partitions on your disk.   When partitioning,\n"
	           "it's a good idea to reserve 3 cylinders at the beginning of the disk for\n"
	           "the boot loader.\n\n"
	           "-bootsect nn    specifies that the boot sector will be placed on sector 'nn'\n"
	           "-bootcode nn    specifies that the boot program itself will be placed\n"
	           "                on sectors starting with 'nn'.  The boot sector will point\n"
	           "                to this sector.\n");
	    
    exit(1);
}

static void bswap32(uint32_t *ptr)
{
    unsigned char b;
    unsigned char *bptr;

    if (swapflg) {
	bptr = (unsigned char *) ptr;
	b = bptr[0]; bptr[0] = bptr[3]; bptr[3] = b;
	b = bptr[1]; bptr[1] = bptr[2]; bptr[2] = b;
	}
}

static void bswap64(uint64_t *ptr)
{
    unsigned char b;
    unsigned char *bptr;

    bptr = (unsigned char *) ptr;

    if (swapflg) {
	b = bptr[0]; bptr[0] = bptr[7]; bptr[7] = b;
	b = bptr[1]; bptr[1] = bptr[6]; bptr[6] = b;
	b = bptr[2]; bptr[2] = bptr[5]; bptr[5] = b;
	b = bptr[3]; bptr[3] = bptr[4]; bptr[4] = b;
	}
}


static void bswap_bootblock(struct boot_block *bb)
{
    int idx;

    for (idx = 59; idx <= 63; idx++) {
	bswap64(&(bb->bb_data[idx]));
	}
}

static void do_checksum(void *ptr,int length,uint32_t *csptr,int swap)
{
    uint32_t *p;
    uint32_t chksum = 0;
    uint32_t d;

    p = (uint32_t *) ptr;

    length /= sizeof(uint32_t);

    while (length) {
	d = *p;
	if (swap) bswap32(&d);
	chksum += d;
	p++;
	length--;
	}

    if (swap) bswap32(&chksum);

    *csptr = chksum;
}


static void dumpbootblock(struct boot_block *bb) 
{
    int idx;

    fprintf(stderr,"Magic Number:       %016llX\n",
	    bb->bb_magic);
    fprintf(stderr,"Boot code offset:   %llu\n",
	    (unsigned long long)bb->bb_secstart);
    fprintf(stderr,"Boot code size:     %u\n",
	    (uint32_t) (bb->bb_secsize & BOOT_SECSIZE_MASK));
    fprintf(stderr,"Header checksum:    %08X\n",
	    (uint32_t) (bb->bb_hdrinfo & BOOT_HDR_CHECKSUM_MASK));
    fprintf(stderr,"Header version:     %d\n",
	    (uint32_t) ((bb->bb_hdrinfo  & BOOT_HDR_VER_MASK) >> BOOT_HDR_VER_SHIFT));
    fprintf(stderr,"Data checksum:      %08X\n",
	    (uint32_t) ((bb->bb_secsize & BOOT_DATA_CHECKSUM_MASK) >> BOOT_DATA_CHECKSUM_SHIFT));
    fprintf(stderr,"Architecture info:  %08X\n",
	    (uint32_t) (bb->bb_archinfo & BOOT_ARCHINFO_MASK));
    fprintf(stderr,"\n");

    for (idx = 59; idx <= 63; idx++) {
	fprintf(stderr,"Word %d = %016llX\n",idx,bb->bb_data[idx]);
	}
}

static int host_is_little(void)
{
    unsigned long var = 1;
    unsigned char *pvar = (unsigned char *) &var;

    return (*pvar == 1);
}

int main(int argc, char *argv[])
{
    int fh;    
    long bootsize;
    long bootbufsize;
    unsigned char *bootcode;
    struct boot_block bootblock;
    uint32_t datacsum,hdrcsum;
    int host_le;

    while ((argc > 1) && (argv[1][0] == '-')) {
	if (strcmp(argv[1],"-v") == 0) {
	    verbose = 1;
	    }
	else if (strcmp(argv[1],"-EB") == 0) {
	    big_endian = 1;
	    }
	else if (strcmp(argv[1],"-EL") == 0) {
	    big_endian = 0;	
	    }
        else if (strcmp(argv[1],"-bootsect") == 0) {
	    char *tmp_ptr;
	    argv++;
	    argc--;
	    if (argc == 1) {
	    	fprintf(stderr,"-bootsect requires an argument\n");
	    	exit(1);
	        }
	    bootsect_offset = strtoul(argv[1], &tmp_ptr, 0);
	    bootcode_offset = bootsect_offset + 1;	/* default if -bootcode not specified */
	    if (*tmp_ptr) {
	    	fprintf(stderr,"Unable to parse %s as a sector offset\n", argv[1]);
	    	exit(1);
		}
	    }
        else if (strcmp(argv[1],"-bootcode") == 0) {
	    char *tmp_ptr;
	    argv++;
	    argc--;
	    if (argc == 1) {
	    	fprintf(stderr,"-bootcode requires an argument\n");
	    	exit(1);
	        }
	    bootcode_offset = strtoul(argv[1], &tmp_ptr, 0);
	    if (*tmp_ptr) {
	    	fprintf(stderr,"Unable to parse %s as a sector offset\n", argv[1]);
	    	exit(1);
		}
	    }
	else {
	    fprintf(stderr,"Invalid switch: %s\n",argv[1]);
	    exit(1);
	    }
	argv++;
	argc--;
	}

    /*
     * We need to swap things around if the host and 
     * target are different endianness
     */

    swapflg = 0;
    host_le = host_is_little();

    if (big_endian && host_is_little()) swapflg = 1;
    if ((big_endian == 0) && !(host_is_little())) swapflg = 1;

    fprintf(stderr,"Host is %s-endian.\n",host_le ? "little" : "big");
    fprintf(stderr,"Target is %s-endian.\n",big_endian ? "big" : "little");

    if (argc != 3) {
	usage();
	}

    /*
     * Read in the boot file 
     */

    fh = open(argv[1],O_RDONLY);
    if (fh < 0) {
	perror(argv[1]);
	}

    bootsize = lseek(fh,0L,SEEK_END);
    lseek(fh,0L,SEEK_SET);

    bootbufsize = roundup(bootsize,BOOT_BLOCK_BLOCKSIZE);

    bootcode = malloc(bootbufsize);
    if (bootcode == NULL) {
	perror("malloc");
	exit(1);
	}
    memset(bootcode,0,bootbufsize);
    if (read(fh,bootcode,bootsize) != bootsize) {
	perror("read");
	exit(1);
	}

    close(fh);

    /*
     * Construct the boot block
     */


    /* Checksum the boot code */
    do_checksum(bootcode,bootbufsize,&datacsum,1);
    bswap32(&datacsum);


    /* fill in the boot block fields, and checksum the boot block */
    bootblock.bb_magic = BOOT_MAGIC_NUMBER;
    bootblock.bb_hdrinfo = (((uint64_t) BOOT_HDR_VERSION) << BOOT_HDR_VER_SHIFT);
    bootblock.bb_secstart = (bootcode_offset * 512);
    bootblock.bb_secsize = ((uint64_t) bootbufsize) |
	(((uint64_t) datacsum) << BOOT_DATA_CHECKSUM_SHIFT);
    bootblock.bb_archinfo = 0;

    do_checksum(&(bootblock.bb_magic),BOOT_BLOCK_SIZE,&hdrcsum,0);
    bootblock.bb_hdrinfo |= (uint64_t) hdrcsum;

    if (verbose) dumpbootblock(&bootblock);

    bswap_bootblock(&bootblock);

    /*
     * Now write the output file
     */

    fh = open(argv[2],O_RDWR|O_CREAT,S_IREAD|S_IWRITE);
    if  (fh < 0) {
	perror(argv[2]);
	exit(1);
	}

    fprintf(stderr,"Installing boot block\n");
    if (lseek(fh, bootsect_offset * 512, SEEK_SET) != (bootsect_offset * 512)) {
        perror(argv[2]);
	exit(1);
        }
    if (write(fh,&bootblock,sizeof(bootblock)) != sizeof(bootblock)) {
	perror(argv[2]);
	exit(1);
	}
    fprintf(stderr,"Installing bootstrap program\n");
    if (lseek(fh, bootcode_offset * 512, SEEK_SET) != (bootcode_offset * 512)) {
        perror(argv[2]);
	exit(1);
        }
    if (write(fh,bootcode,bootbufsize) != bootbufsize) {
	perror(argv[2]);
	exit(1);
	}

    close(fh);

    fprintf(stderr,"Done.  %s installed on %s\n",argv[1],argv[2]);

    exit(0);


}
