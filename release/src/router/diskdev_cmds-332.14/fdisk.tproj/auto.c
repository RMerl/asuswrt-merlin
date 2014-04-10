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
 * Auto partitioning code.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include "disk.h"
#include "mbr.h"
#include "auto.h"

int AUTO_boothfs __P((disk_t *, mbr_t *));
int AUTO_bootufs __P((disk_t *, mbr_t *));
int AUTO_hfs __P((disk_t *, mbr_t *));
int AUTO_ufs __P((disk_t *, mbr_t *));
int AUTO_dos __P((disk_t *, mbr_t *));
int AUTO_raid __P((disk_t *, mbr_t *));

/* The default style is the first one in the list */
struct _auto_style {
  char *style_name;
  int (*style_fn)(disk_t *, mbr_t *);
  char *description;
} style_fns[] = {
  {"boothfs", AUTO_boothfs, "8Mb boot plus HFS+ root partition"},
  {"bootufs", AUTO_bootufs, "8Mb boot plus UFS root partition"},
  {"hfs",     AUTO_hfs,     "Entire disk as one HFS+ partition"},
  {"ufs",     AUTO_ufs,     "Entire disk as one UFS partition"},
  {"dos",     AUTO_dos,     "Entire disk as one DOS partition"},
  {"raid",    AUTO_raid,    "Entire disk as one 0xAC partition"},
  {0,0}
};

void
AUTO_print_styles(FILE *f)
{
  struct _auto_style *fp;
  int i;

  for (i=0, fp = &style_fns[0]; fp->style_name != NULL; i++, fp++) {
    fprintf(f, "  %-10s  %s%s\n", fp->style_name, fp->description, (i==0) ? " (default)" : "");
  }
}


int
AUTO_init(disk_t *disk, char *style, mbr_t *mbr)
{
  struct _auto_style *fp;

  for (fp = &style_fns[0]; fp->style_name != NULL; fp++) {
    /* If style is NULL, use the first (default) style */
    if (style == NULL || strcasecmp(style, fp->style_name) == 0) {
      return (*fp->style_fn)(disk, mbr);
    }
  }
  warnx("No such auto-partition style %s", style);
  return AUTO_ERR;
}


static int
use_whole_disk(disk_t *disk, unsigned char id, mbr_t *mbr)
{
  MBR_clear(mbr);
  mbr->part[0].id = id;
  mbr->part[0].bs = 63;
  mbr->part[0].ns = disk->real->size - 63;
  PRT_fix_CHS(disk, &mbr->part[0], 0);
  return AUTO_OK;
}

/* DOS style: one partition for the whole disk */
int
AUTO_dos(disk_t *disk, mbr_t *mbr)
{
  int cc;
  cc = use_whole_disk(disk, 0x0C, mbr);
  if (cc == AUTO_OK) {
    mbr->part[0].flag = DOSACTIVE;
  }
  return cc;
}

/* HFS style: one partition for the whole disk */
int
AUTO_hfs(disk_t *disk, mbr_t *mbr)
{
  int cc;
  cc = use_whole_disk(disk, 0xAF, mbr);
  if (cc == AUTO_OK) {
    mbr->part[0].flag = DOSACTIVE;
  }
  return cc;
}

/* UFS style: one partition for the whole disk */
int
AUTO_ufs(disk_t *disk, mbr_t *mbr)
{
  int cc;
  cc = use_whole_disk(disk, 0xA8, mbr);
  if (cc == AUTO_OK) {
    mbr->part[0].flag = DOSACTIVE;
  }
  return cc;
}

/* One boot partition, one HFS+ root partition */
int
AUTO_boothfs (disk_t *disk, mbr_t *mbr)
{
  /* Check disk size. */
  if (disk->real->size < 16 * 2048) {
    errx(1, "Disk size must be greater than 16Mb");
    return AUTO_ERR;
  }

  MBR_clear(mbr);

  /* 8MB boot partition */
  mbr->part[0].id = 0xAB;
  mbr->part[0].bs = 63;
  mbr->part[0].ns = 8 * 1024 * 2;
  mbr->part[0].flag = DOSACTIVE;
  PRT_fix_CHS(disk, &mbr->part[0], 0);

  /* Rest of the disk for rooting */
  mbr->part[1].id = 0xAF;
  mbr->part[1].bs = (mbr->part[0].bs + mbr->part[0].ns);
  mbr->part[1].ns = disk->real->size - mbr->part[0].ns - 63;
  PRT_fix_CHS(disk, &mbr->part[1], 1);

  return AUTO_OK;
}

/* One boot partition, one UFS root partition */
int
AUTO_bootufs(disk_t *disk, mbr_t *mbr)
{
  /* Check disk size. */
  if (disk->real->size < 16 * 2048) {
    errx(1, "Disk size must be greater than 16Mb");
    return AUTO_ERR;
  }

  MBR_clear(mbr);

  /* 8MB boot partition */
  mbr->part[0].id = 0xAB;
  mbr->part[0].bs = 63;
  mbr->part[0].ns = 8 * 1024 * 2;
  mbr->part[0].flag = DOSACTIVE;
  PRT_fix_CHS(disk, &mbr->part[0], 0);

  /* Rest of the disk for rooting */
  mbr->part[1].id = 0xA8;
  mbr->part[1].bs = (mbr->part[0].bs + mbr->part[0].ns);
  mbr->part[1].ns = disk->real->size - mbr->part[0].ns - 63;
  PRT_fix_CHS(disk, &mbr->part[1], 1);

  return AUTO_OK;
}

/* RAID style: one 0xAC partition for the whole disk */
int
AUTO_raid(disk_t *disk, mbr_t *mbr)
{
  return use_whole_disk(disk, 0xAC, mbr);
}

