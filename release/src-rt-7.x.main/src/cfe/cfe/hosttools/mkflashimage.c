/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  Flash Image generator			File: mkflashimage.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *  This program sticks a header on the front of a binary
    *  file making it suitable for use with the 'flash' command
    *  in CFE.  The header contains the CFE version # and board
    *  type, a CRC, and other info to help prevent us from
    *  flashing bad stuff onto a board.
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

#ifndef _SYS_INT_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned long long uint64_t;
typedef unsigned long uint32_t;
#endif

#include "cfe_flashimage.h"
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static int verbose = 0;
static int big_endian = 1;
static int both_endian = 0;
int mlong64 = 0;
char boardname[32];

/*
 * This is the offset in the flash where the little-endian image goes
 * if we're making a bi-endian flash.
 */

#define CFE_BIENDIAN_LE_OFFSET	(1024*1024)

static void usage(void)
{
    fprintf(stderr,"usage: mkflashimage [-v] [-EB] [-EL] [-64] [-B boardname] [-V v.v.v] binfile outfile\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"       mkflashimage [-v] -EX [-64] [-B boardname] [-V v.v.v] BE-binfile LE-binfile outfile\n");
    fprintf(stderr,"       (this variant used for making bi-endian flash files)\n");
    exit(1);
}


static int host_is_little(void)
{
    unsigned long var = 1;
    unsigned char *pvar = (unsigned char *) &var;

    return (*pvar == 1);
}

#define     CRC32_POLY        0xEDB88320UL    /* CRC-32 Poly */

static unsigned int
crc32(const unsigned char *databuf, unsigned int  datalen) 
{       
    unsigned int idx, bit, data, crc = 0xFFFFFFFFUL;
 
    for (idx = 0; idx < datalen; idx++) {
	for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1) {
	    crc = (crc >> 1) ^ (((crc ^ data) & 1) ? CRC32_POLY : 0);
	    }
	}

    return crc;
}


void stuff_be32(uint8_t *dest,unsigned int src)
{
    *dest++ = (src >> 24) & 0xFF;
    *dest++ = (src >> 16) & 0xFF;
    *dest++ = (src >>  8) & 0xFF;
    *dest++ = (src >>  0) & 0xFF;
}

int main(int argc, char *argv[])
{
    int fh;    
    int flashsize;
    unsigned char *flashcode;
    cfe_flashimage_t header;
    int host_le;
    int majver,minver,ecover;
    uint32_t crc;
    uint32_t flags;
    char *outfile;

    majver = minver = ecover = 0;
    boardname[0] = 0;

    while ((argc > 1) && (argv[1][0] == '-')) {
	if (strcmp(argv[1],"-v") == 0) {
	    verbose = 1;
	    }
	else if (strcmp(argv[1],"-EX") == 0) {
	    if (verbose) fprintf(stderr,"[Image file will be marked Bi-Endian]\n");
	    both_endian = 1;
	    }
	else if (strcmp(argv[1],"-EB") == 0) {
	    if (verbose) fprintf(stderr,"[Image file will be marked Big-Endian]\n");
	    big_endian = 1;
	    }
	else if (strcmp(argv[1],"-EL") == 0) {
	    if (verbose) fprintf(stderr,"[Image file will be marked Little-Endian]\n");
	    big_endian = 0;	
	    }
	else if (strcmp(argv[1],"-64") == 0) {
	    if (verbose) fprintf(stderr,"[Image file will be marked 64-bit]\n");
	    mlong64 = 1;
	    }
	else if (strcmp(argv[1],"-B") == 0) {
	    argc--;
	    argv++;
	    strcpy(boardname,argv[1]);
	    if (verbose) fprintf(stderr,"[Board name: %s]\n",boardname);
	    }
	else if (strcmp(argv[1],"-V") == 0) {
	    argc--;
	    argv++;
	    sscanf(argv[1],"%d.%d.%d",&majver,&minver,&ecover);
	    if (verbose) fprintf(stderr,"[Image version: %d.%d.%d]\n",majver,minver,ecover);
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

    host_le = host_is_little();

    if (verbose) {
	fprintf(stderr,"Host is %s-endian.\n",host_le ? "little" : "big");
	if (both_endian) {
	    fprintf(stderr,"Target is bi-endian.\n");
	    }
	else {
	    fprintf(stderr,"Target is %s-endian.\n",big_endian ? "big" : "little");
	    }
	}

    if ((both_endian && (argc != 4)) || (!both_endian && (argc != 3))) {
	usage();
	}

    /*
     * Read in the boot file(s)
     */

    flags = 0;

    if (both_endian) {
	int be_size;

	flags |= (CFE_IMAGE_EB | CFE_IMAGE_EL);

	if (verbose) fprintf(stderr,"Reading: %s\n",argv[2]);

	fh = open(argv[2],O_RDONLY);
	if (fh < 0) {
	    perror(argv[2]);
	    }

	flashsize = lseek(fh,0L,SEEK_END);
	lseek(fh,0L,SEEK_SET);

	flashcode = malloc(flashsize+CFE_BIENDIAN_LE_OFFSET);
	if (flashcode == NULL) {
	    perror("malloc");
	    exit(1);
	    }
	memset(flashcode,0xFF,flashsize+CFE_BIENDIAN_LE_OFFSET);

	if (read(fh,flashcode+CFE_BIENDIAN_LE_OFFSET,flashsize) != flashsize) {
	    perror("read");
	    exit(1);
	    }

	close(fh);

	if (memcmp(flashcode+CFE_BIENDIAN_LE_OFFSET,CFE_IMAGE_SEAL,4) == 0) {
	    fprintf(stderr,"File '%s' already has an image header.\n",argv[2]);
	    exit(1);
	    }

	flashsize += CFE_BIENDIAN_LE_OFFSET;	/* actual file size */

	if (verbose) fprintf(stderr,"Reading: %s\n",argv[1]);

	fh = open(argv[1],O_RDONLY);
	if (fh < 0) {
	    perror(argv[1]);
	    exit(1);
	    }

	be_size = lseek(fh,0L,SEEK_END);
	lseek(fh,0L,SEEK_SET);
	if (be_size > CFE_BIENDIAN_LE_OFFSET) {
	    fprintf(stderr,"File '%s' will not fit within first %d bytes of flash image\n",
		    argv[1],CFE_BIENDIAN_LE_OFFSET);
	    close(fh);
	    exit(1);
	    }
	
	if (read(fh,flashcode,be_size) != be_size) {
	    perror("read");
	    exit(1);
	    }

	close(fh);

	outfile = argv[3];

	}
    else {
	if (big_endian) flags |= CFE_IMAGE_EB;
	else flags |= CFE_IMAGE_EL;

	fh = open(argv[1],O_RDONLY);
	if (fh < 0) {
	    perror(argv[1]);
	    exit(1);
	    }

	flashsize = lseek(fh,0L,SEEK_END);
	lseek(fh,0L,SEEK_SET);

	flashcode = malloc(flashsize);
	if (flashcode == NULL) {
	    perror("malloc");
	    exit(1);
	    }
	memset(flashcode,0,flashsize);
	if (read(fh,flashcode,flashsize) != flashsize) {
	    perror("read");
	    exit(1);
	    }

	close(fh);

	if (memcmp(flashcode,CFE_IMAGE_SEAL,4) == 0) {
	    fprintf(stderr,"File '%s' already has an image header.\n",argv[1]);
	    exit(1);
	    }

	outfile = argv[2];
	}


    /*
     * Construct the flash header
     */

    if (mlong64) flags |= CFE_IMAGE_MLONG64;
    crc = crc32(flashcode,flashsize);

    memset(&header,0,sizeof(header));
    memcpy(header.seal,CFE_IMAGE_SEAL,sizeof(header.seal));
    stuff_be32(header.flags,flags);
    stuff_be32(header.size,flashsize);
    stuff_be32(header.crc,crc);
    header.majver = majver;
    header.minver = minver;
    header.ecover = ecover;
    header.miscver = 0;
    strcpy(header.boardname,boardname);

    /*
     * Now write the output file
     */

    fh = open(outfile,O_RDWR|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE|S_IRGRP|S_IWGRP|S_IROTH);
    if  (fh < 0) {
	perror(outfile);
	exit(1);
	}
    if (write(fh,&header,sizeof(header)) != sizeof(header)) {
	perror(outfile);
	exit(1);
	}
    if (write(fh,flashcode,flashsize) != flashsize) {
	perror(outfile);
	exit(1);
	}

    fprintf(stderr,"Wrote %d bytes to %s\n",sizeof(header)+flashsize,outfile);

    close(fh);

    exit(0);


}
