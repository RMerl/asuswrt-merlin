/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
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
/* Copyright 1998 Apple Computer, Inc.
 *
 * Get disk label's sector size  routine.
 *    Input: pointer to a block device string  ie: "/dev/disk1s1"
 *    Return: sector size from the disk label
 *
 * HISTORY
 *
 * 27 May 1998 K. Crippes at Apple
 *      Rhapsody version created.
 * 18 Feb 1999 D. Markarian at Apple
 *      DKIOCGLABEL deprecated;  using DKIOCBLKSIZE instead, which now returns
 *      the appropriate size for ufs partitions created with wierd block sizes.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/disk.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *blockcheck __P((char *));

long dksecsize (dev)
     char *dev;
{
    int    fd;        /* file descriptor for reading device label */
    char   *cdev;
    int    devblklen;
    extern int errno;

    /* Convert block device into a character device string */
    if (cdev = blockcheck(dev)) {
        if ((fd = open(cdev, O_RDONLY)) < 0) {
  	  fprintf(stderr, "Can't open %s, %s\n", cdev, strerror(errno));
	  return (0);
        }
    }
    else
          return (0);

    if (ioctl(fd, DKIOCGETBLOCKSIZE, &devblklen) < 0) {
	(void)close(fd);
        return (0);
    }
    else {
	(void)close(fd);
        return (devblklen);
    }
}

