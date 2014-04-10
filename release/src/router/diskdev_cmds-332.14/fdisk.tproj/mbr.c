/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 2002 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.2 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Copyright (c) 1997 Tobias Weingartner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <util.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#if 0
#include <sys/dkio.h>
#endif
#include <machine/param.h>
#include "disk.h"
#include "misc.h"
#include "mbr.h"
#include "part.h"


void
MBR_init(disk, mbr)
	disk_t *disk;
	mbr_t *mbr;
{
	/* Fix up given mbr for this disk */
	mbr->part[0].flag = 0;
	mbr->part[1].flag = 0;
	mbr->part[2].flag = 0;
#if !defined(DOSPTYP_OPENBSD)
	mbr->part[3].flag = 0;
	mbr->signature = MBR_SIGNATURE;
#else

	mbr->part[3].flag = DOSACTIVE;
	mbr->signature = DOSMBR_SIGNATURE;

	/* Use whole disk, save for first head, on first cyl. */
	mbr->part[3].id = DOSPTYP_OPENBSD;
	mbr->part[3].scyl = 0;
	mbr->part[3].shead = 1;
	mbr->part[3].ssect = 1;

	/* Go right to the end */
	mbr->part[3].ecyl = disk->real->cylinders - 1;
	mbr->part[3].ehead = disk->real->heads - 1;
	mbr->part[3].esect = disk->real->sectors;

	/* Fix up start/length fields */
	PRT_fix_BN(disk, &mbr->part[3], 3);

#if defined(__powerpc__) || defined(__mips__)
	/* Now fix up for the MS-DOS boot partition on PowerPC. */
	mbr->part[0].flag = DOSACTIVE;	/* Boot from dos part */
	mbr->part[3].flag = 0;
	mbr->part[3].ns += mbr->part[3].bs;
	mbr->part[3].bs = mbr->part[0].bs + mbr->part[0].ns;
	mbr->part[3].ns -= mbr->part[3].bs;
	PRT_fix_CHS(disk, &mbr->part[3], 3);
	if ((mbr->part[3].shead != 1) || (mbr->part[3].ssect != 1)) {
		/* align the partition on a cylinder boundary */
		mbr->part[3].shead = 0;
		mbr->part[3].ssect = 1;
		mbr->part[3].scyl += 1;
	}
	/* Fix up start/length fields */
	PRT_fix_BN(disk, &mbr->part[3], 3);
#endif
#endif
}

void
MBR_parse(disk, offset, reloff, mbr)
	disk_t *disk;
	off_t offset;
	off_t reloff;
	mbr_t *mbr;
{
	int i;
	char *mbr_buf = mbr->buf;

	memcpy(mbr->code, mbr_buf, MBR_CODE_SIZE);
	mbr->offset = offset;
	mbr->reloffset = reloff;
	mbr->signature = getshort(&mbr_buf[MBR_SIG_OFF]);

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(disk, &mbr_buf[MBR_PART_OFF + MBR_PART_SIZE * i], 
		    offset, reloff, &mbr->part[i], i);
}

void
MBR_make(mbr)
	mbr_t *mbr;
{
	int i;
	char *mbr_buf = mbr->buf;

	memcpy(mbr_buf, mbr->code, MBR_CODE_SIZE);
	putshort(&mbr_buf[MBR_SIG_OFF], mbr->signature);

	for (i = 0; i < NDOSPART; i++)
		PRT_make(&mbr->part[i], mbr->offset, mbr->reloffset, 
		    &mbr_buf[MBR_PART_OFF + MBR_PART_SIZE * i]);
}

void
MBR_print(mbr)
	mbr_t *mbr;
{
	int i;

	/* Header */
	printf("Signature: 0x%X\n",
	    (int)mbr->signature);
	PRT_print(0, NULL);

	/* Entries */
	for (i = 0; i < NDOSPART; i++)
		PRT_print(i, &mbr->part[i]);
}

int
MBR_read(fd, where, mbr)
	int fd;
	off_t where;
	mbr_t *mbr;
{
	off_t off;
	int len;
	char *buf = mbr->buf;

	where *= MBR_BUF_SIZE;
	off = lseek(fd, where, SEEK_SET);
	if (off != where)
		return (off);
	len = read(fd, buf, MBR_BUF_SIZE);
	if (len != MBR_BUF_SIZE)
		return (len);
	return (0);
}

int
MBR_write(fd, mbr)
	int fd;
	mbr_t *mbr;
{
	off_t off;
	int len;
	char *buf = mbr->buf;
	off_t where;

	where = mbr->offset * MBR_BUF_SIZE;
	off = lseek(fd, where, SEEK_SET);
	if (off != where)
		return (off);
	len = write(fd, buf, MBR_BUF_SIZE);
	if (len != MBR_BUF_SIZE)
		return (len);
#if defined(DIOCRLDINFO)
	(void) ioctl(fd, DIOCRLDINFO, 0);
#endif
	return (0);
}

void 
MBR_pcopy(disk, mbr)
	disk_t *disk;
	mbr_t *mbr;
{
	/* 
	 * Copy partition table from the disk indicated
	 * to the supplied mbr structure 
	 */

	int i, fd, offset = 0, reloff = 0;
	mbr_t *mbrd;
	
	mbrd = MBR_alloc(NULL);
	fd = DISK_open(disk->name, O_RDONLY);
	MBR_read(fd, offset, mbrd);
	DISK_close(fd);
	MBR_parse(disk, offset, reloff, mbrd);
	for (i = 0; i < NDOSPART; i++) {
		PRT_parse(disk, &mbrd->buf[MBR_PART_OFF +
					 MBR_PART_SIZE * i], 
			  offset, reloff, &mbr->part[i], i);
		PRT_print(i, &mbr->part[i]);
	}
	MBR_free(mbrd);
}


static int
parse_number(char *str, int default_val, int base) {
	if (str != NULL && *str != '\0') {
	  default_val = strtol(str, NULL, base);
	}
	return default_val;
}

static inline int
null_arg(char *arg) {
  if (arg == NULL || *arg == NULL)
    return 1;
  else
    return 0;
}

/* Parse a partition spec into a partition structure.
 * Spec is of the form:
 *   <start>,<size>,<id>,<bootable>[,<c,h,s>,<c,h,s>]
 * We require passing in the disk and mbr so we can
 * set reasonable defaults for values, e.g. "the whole disk"
 * or "starting after the last partition."
 */
#define N_ARGS 10
static int
MBR_parse_one_spec(char *line, disk_t *disk, mbr_t *mbr, int pn)
{
  int i;
  char *args[N_ARGS];
  prt_t *part = &mbr->part[pn];
  int next_start, next_size;

  /* There are up to 10 arguments. */
  for (i=0; i<N_ARGS; i++) {
    char *arg;
    while (isspace(*line))
      line++;
    arg = strsep(&line, ",\n");
    if (arg == NULL || line == NULL) {
      break;
    }
    args[i] = arg;
  }
  for (; i<N_ARGS; i++) {
    args[i] = NULL;
  }
  /* Set reasonable defaults. */
  if (pn == 0) {
    next_start = 0;
  } else {
    next_start = mbr->part[pn-1].bs + mbr->part[pn-1].ns;
  }
  next_size = disk->real->size;
  for(i=0; i<pn; i++) {
    next_size -= mbr->part[i].ns;
  }

  part->id = parse_number(args[2], 0xA8, 16);
  if (!null_arg(args[3]) && *args[3] == '*') {
    part->flag = 0x80;
  } else {
    part->flag = 0;
  }
  /* If you specify the start or end sector,
     you have to give both. */
  if ((null_arg(args[0]) && !null_arg(args[1])) ||
       (!null_arg(args[0]) && null_arg(args[1]))) {
    errx(1, "You must specify both start and size, or neither");
    return -1;
  }

  /* If you specify one of the CHS args,
     you have to give them all. */
  if (!null_arg(args[4])) {
    for (i=5; i<10; i++) {
      if (null_arg(args[i])) {
	errx(1, "Either all CHS arguments must be specified, or none");
	return -1;
      }
    }

    part->scyl = parse_number(args[4], 0, 10);
    part->shead = parse_number(args[5], 0, 10);
    part->ssect = parse_number(args[6], 0, 10);	  
    part->scyl = parse_number(args[7], 0, 10);
    part->shead = parse_number(args[8], 0, 10);
    part->ssect = parse_number(args[9], 0, 10);	  
    if (null_arg(args[0])) {
      PRT_fix_BN(disk, part, pn);
    }
  } else {
    /* See if they gave no CHS and no start/end */
    if (null_arg(args[0])) {
      errx(1, "You must specify either start sector and size or CHS");
      return -1;
    }
  }
  if (!null_arg(args[0])) {
    part->bs = parse_number(args[0], next_start, 10);
    part->ns = parse_number(args[1], next_size, 10);
    PRT_fix_CHS(disk, part, pn);
  }
  return 0;
}


typedef struct _mbr_chain {
  mbr_t mbr;
  struct _mbr_chain *next;
} mbr_chain_t;

/* Parse some number of MBR spec lines.
 * Spec is of the form:
 *   <start>,<size>,<id>,<bootable>[,<c,h,s>,<c,h,s>]
 * 
 */
mbr_t *
MBR_parse_spec(FILE *f, disk_t *disk)
{
  int lineno;
  int offset, firstoffset;
  mbr_t *mbr, *head, *prev_mbr;

  head = mbr = prev_mbr = NULL;
  firstoffset = 0;
  do {

    offset = 0;
    for (lineno = 0; lineno < NDOSPART && !feof(f); lineno++) {
        char line[256];
        char *str;
	prt_t *part;

	do {
	    str = fgets(line, 256, f);
	} while ((str != NULL) && (*str == '\0'));
        if (str == NULL) {
            break;
        }

	if (mbr == NULL) {
	    mbr = MBR_alloc(prev_mbr);
	    if (head == NULL)
		head = mbr;
	}

	if (MBR_parse_one_spec(line, disk, mbr, lineno)) {
	  /* MBR_parse_one_spec printed the error message. */
	  return NULL;
	}
	part = &mbr->part[lineno];
	if ((part->id == DOSPTYP_EXTEND) || (part->id == DOSPTYP_EXTENDL)) {
	  offset = part->bs;
	  if (firstoffset == 0) firstoffset = offset;
	}
    }
    /* If fewer lines than partitions, zero out the rest of the partitions */
    if (mbr != NULL) {
	for (; lineno < NDOSPART; lineno++) {
	    bzero(&mbr->part[lineno], sizeof(prt_t));
	}
    }
    prev_mbr = mbr;
    mbr = NULL;
  } while (offset >= 0 && !feof(f));

  return head;
}

void
MBR_dump(mbr_t *mbr)
{
  int i;
  prt_t *part;

  for (i=0; i<NDOSPART; i++) {
    part = &mbr->part[i];
    printf("%d,%d,0x%02X,%c,%d,%d,%d,%d,%d,%d\n",
	   part->bs,
	   part->ns,
	   part->id,
	   (part->flag == 0x80) ? '*' : '-',
	   part->scyl,
	   part->shead,
	   part->ssect,
	   part->ecyl,
	   part->ehead,
	   part->esect);
  }
}

mbr_t *
MBR_alloc(mbr_t *parent)
{
  mbr_t *mbr = (mbr_t *)malloc(sizeof(mbr_t));
  bzero(mbr, sizeof(mbr_t));
  if (parent) {
    parent->next = mbr;
  }
  mbr->signature = MBR_SIGNATURE;
  return mbr;
}

void
MBR_free(mbr_t *mbr)
{
  mbr_t *tmp;
  while (mbr) {
    tmp = mbr->next;
    free(mbr);
    mbr = tmp;
  }
}

/* Read and parse all the partition tables on the disk,
 * including extended partitions.
 */
mbr_t *
MBR_read_all(disk_t *disk)
{
  mbr_t *mbr = NULL, *head = NULL;
  int i, fd, offset, firstoff;

  fd = DISK_open(disk->name, O_RDONLY);
  firstoff = offset = 0;
  do {
    mbr = MBR_alloc(mbr);
    if (head == NULL) {
      head = mbr;
    }
    MBR_read(fd, offset, mbr);
    MBR_parse(disk, offset, firstoff, mbr);
    if (mbr->signature != MBR_SIGNATURE) {
      /* The MBR signature is invalid. */
      break;
    }    
    offset = 0;
    for (i=0; i<NDOSPART; i++) {
      prt_t *part = &mbr->part[i];
      if ((part->id == DOSPTYP_EXTEND) || (part->id == DOSPTYP_EXTENDL)) {
	offset = part->bs;
	if (firstoff == 0) {
	  firstoff = offset;
	}
      }
    }
  } while (offset > 0);
  DISK_close(fd);

  return head;
}


int
MBR_write_all(disk_t *disk, mbr_t *mbr)
{
  int result = 0;
  int fd;

  fd = DISK_open(disk->name, O_RDWR);
  while (mbr) {
    MBR_make(mbr);
    result = MBR_write(fd, mbr);
    if (result)
      break;
    mbr = mbr->next;
  }
  DISK_close(fd);
  return result;
}

void
MBR_print_all(mbr_t *mbr) {
  while (mbr) {
    MBR_print(mbr);
    mbr = mbr->next;
  }
}

void
MBR_dump_all(mbr_t *mbr) {
  while (mbr) {
    MBR_dump(mbr);
    mbr = mbr->next;
  }
}

void
MBR_clear(mbr_t *mbr) {
  int i;
  if (mbr->next) {
    MBR_free(mbr->next);
    mbr->next = NULL;
  }
  for (i=0; i<4; i++) {
    bzero(&mbr->part[i], sizeof(mbr->part[i]));
  }
  bzero(&mbr->buf, sizeof(mbr->buf));
}

