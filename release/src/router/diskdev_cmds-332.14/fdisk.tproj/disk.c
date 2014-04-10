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
 * Copyright (c) 1997, 2001 Tobias Weingartner
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/disk.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#ifdef __i386__
#include <pexpert/i386/boot.h>
#endif
#include "disk.h"

int
DISK_open(disk, mode)
	char *disk;
	int mode;
{
	int fd;
	struct stat st;

	fd = open(disk, mode);
	if (fd == -1)
		err(1, "%s", disk);
	if (fstat(fd, &st) == -1)
		err(1, "%s", disk);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file", disk);
	return (fd);
}

int
DISK_openshared(disk, mode, shared)
	char *disk;
	int mode;
	int *shared;
{
	int fd;
	struct stat st;
	*shared = 0;

	fd = open(disk, mode|O_EXLOCK);
	if (fd == -1) {
	  // if we can't have exclusive access, attempt
	  // to gracefully degrade to shared access
	  fd = open(disk, mode|O_SHLOCK);
	  if(fd == -1)
		err(1, "%s", disk);

	  *shared = 1;
	}

	if (fstat(fd, &st) == -1)
		err(1, "%s", disk);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file", disk);
	return (fd);
}

int
DISK_close(fd)
	int fd;
{

	return (close(fd));
}

/* Given a size in the metrics,
 * fake up a CHS geometry.
 */
void
DISK_fake_CHS(DISK_metrics *lm)
{
  int heads = 4;
  int spt = 63;
  int cylinders = (lm->size / heads / spt);

  while (cylinders > 1024 && heads < 256) {
    heads *= 2;
    cylinders /= 2;
  }
  if (heads == 256) {
    heads = 255;
    cylinders = (lm->size / heads / spt);
  }	
  lm->cylinders = cylinders;
  lm->heads = heads;
  lm->sectors = spt;
}

/* Routine to go after the disklabel for geometry
 * information.  This should work everywhere, but
 * in the land of PC, things are not always what
 * they seem.
 */
DISK_metrics *
DISK_getlabelmetrics(name)
	char *name;
{
	DISK_metrics *lm = NULL;
	long long size;
	int fd;
	struct stat st;

	/* Get label metrics */
	if ((fd = DISK_open(name, O_RDONLY)) != -1) {
		lm = malloc(sizeof(DISK_metrics));

		if (fstat(fd, &st) == -1)
		  err(1, "%s", name);
		if (S_ISCHR(st.st_mode)) {
		  if (ioctl(fd, DKIOCGETBLOCKCOUNT, &size) == -1) {
		    err(1, "Could not get disk metrics");
		    free(lm);
		    return NULL;
		  }
		} else {
		  size = st.st_size;
		}

		lm->size = size;
		DISK_fake_CHS(lm);
		DISK_close(fd);
	}

	return (lm);
}

/*
 * Don't try to get BIOS disk metrics.
 */
DISK_metrics *
DISK_getbiosmetrics(name)
	char *name;
{
	return (NULL);
}

/* This is ugly, and convoluted.  All the magic
 * for disk geo/size happens here.  Basically,
 * the real size is the one we will use in the
 * rest of the program, the label size is what we
 * got from the disklabel.  If the disklabel fails,
 * we assume we are working with a normal file,
 * and should request the user to specify the
 * geometry he/she wishes to use.
 */
int
DISK_getmetrics(disk, user)
	disk_t *disk;
	DISK_metrics *user;
{

	disk->label = DISK_getlabelmetrics(disk->name);
	disk->bios = DISK_getbiosmetrics(disk->name);

	/* If user supplied, use that */
	if (user) {
		disk->real = user;
		return (0);
	}

	/* Fixup bios metrics to include cylinders past 1023 boundary */
	if(disk->label && disk->bios){
		int cyls, secs;

		cyls = disk->label->size / (disk->bios->heads * disk->bios->sectors);
		secs = cyls * (disk->bios->heads * disk->bios->sectors);
		if ((disk->label->size - secs) < 0)
			errx(1, "BIOS fixup botch (%d sectors)", disk->label->size - secs);
		disk->bios->cylinders = cyls;
		disk->bios->size = secs;
	}

	/* If we have a (fixed) BIOS geometry, use that */
	if (disk->bios) {
		disk->real = disk->bios;
		return (0);
	}

	/* If we have a label, use that */
	if (disk->label) {
		disk->real = disk->label;
		return (0);
	}

	/* Can not get geometry, punt */
	disk->real = NULL;
	return (1);
}

int
DISK_printmetrics(disk)
	disk_t *disk;
{

	printf("Disk: %s\t", disk->name);
	if (disk->real)
		printf("geometry: %d/%d/%d [%d sectors]\n", disk->real->cylinders,
		    disk->real->heads, disk->real->sectors, disk->real->size);
	else
		printf("geometry: <none>\n");

	return (0);
}

