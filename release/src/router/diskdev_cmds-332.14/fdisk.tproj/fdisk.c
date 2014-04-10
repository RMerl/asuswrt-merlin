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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include "disk.h"
#include "user.h"
#include "auto.h"

#define _PATH_MBR "/usr/standalone/i386/boot0"

void
usage()
{
	extern char * __progname;
	fprintf(stderr, "usage: %s "
	    "[-ieu] [-f mbrboot] [-c cyl -h head -s sect] [-S size] [-r] [-a style] disk\n"
	    "\t-i: initialize disk with new MBR\n"
	    "\t-u: update MBR code, preserve partition table\n"
	    "\t-e: edit MBRs on disk interactively\n"
	    "\t-f: specify non-standard MBR template\n"
	    "\t-chs: specify disk geometry\n"
	    "\t-S: specify disk size\n"
	    "\t-r: read partition specs from stdin (implies -i)\n"
	    "\t-a: auto-partition with the given style\n"
	    "\t-d: dump partition table\n"
	    "\t-y: don't ask any questions\n"
	    "\t-t: test if disk is partitioned\n"
	    "`disk' is of the form /dev/rdisk0.\n",
	    __progname);
	fprintf(stderr, "auto-partition styles:\n");
	AUTO_print_styles(stderr);
	exit(1);
}

char *mbr_binary = NULL;
int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, fd;
	int i_flag = 0, m_flag = 0, u_flag = 0, r_flag = 0, d_flag = 0, y_flag = 0, t_flag = 0;
	int c_arg = 0, h_arg = 0, s_arg = 0;
	int size_arg = 0;
	disk_t disk;
	DISK_metrics *usermetrics;
	char *mbrfile = _PATH_MBR;
	mbr_t *mp;
	char *auto_style = NULL;

	while ((ch = getopt(argc, argv, "ieuf:c:h:s:S:ra:dyt")) != -1) {
		switch(ch) {
		case 'i':
			i_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'e':
			m_flag = 1;
			break;
		case 'f':
			mbrfile = optarg;
			break;
		case 'c':
			c_arg = atoi(optarg);
			if (c_arg < 1 || c_arg > 262144)
				errx(1, "Cylinder argument out of range.");
			break;
		case 'h':
			h_arg = atoi(optarg);
			if (h_arg < 1 || h_arg > 256)
				errx(1, "Head argument out of range.");
			break;
		case 's':
			s_arg = atoi(optarg);
			if (s_arg < 1 || s_arg > 63)
				errx(1, "Sector argument out of range.");
			break;
		case 'S':
		        size_arg = atoi(optarg);
			break;
		case 'r':
			r_flag = 1;
			break;
		case 'a':
		        auto_style = optarg;
			break;
		case 'd':
		        d_flag = 1;
			break;
		case 'y':
		        y_flag = 1;
			break;
		case 't':
		        t_flag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Argument checking */
	if (argc != 1)
		usage();
	else
		disk.name = argv[0];

	if (i_flag && u_flag) errx(1, "-i and -u cannot be specified simultaneously");

	/* Put in supplied geometry if there */
	if (c_arg | h_arg | s_arg | size_arg) {
		usermetrics = malloc(sizeof(DISK_metrics));
		if (usermetrics != NULL) {
			if (c_arg && h_arg && s_arg) {
				usermetrics->cylinders = c_arg;
				usermetrics->heads = h_arg;
				usermetrics->sectors = s_arg;
				if (size_arg) {
				  usermetrics->size = size_arg;
				} else {
				  usermetrics->size = c_arg * h_arg * s_arg;
				}
			} else {
			  if (size_arg) {
			    usermetrics->size = size_arg;
			    DISK_fake_CHS(usermetrics);
			  } else {
			    errx(1, "Please specify a full geometry with [-chs].");
			  }
			}
		}
	} else {
		usermetrics = NULL;
	}

	/* Get the geometry */
	disk.real = NULL;
	if (DISK_getmetrics(&disk, usermetrics))
		errx(1, "Can't get disk geometry, please use [-chs] to specify.");

	/* If only testing, read MBR and silently exit */
	if (t_flag) {
	  mbr_t *mbr;

	  mp = mbr = MBR_read_all(&disk);
	  while (mp) {
	    if (mp->signature != MBR_SIGNATURE) {
	      MBR_free(mbr);
	      exit(1);
	    }
	    mp = mp->next;
	  }
	  MBR_free(mbr);
	  exit(0);
	}

	/* If not editing the disk, print out current MBRs on disk */
	if ((i_flag + r_flag + u_flag + m_flag) == 0) {
		exit(USER_print_disk(&disk, d_flag));
	}

	/* Parse mbr template or read partition specs, to pass on later */
	if (auto_style && r_flag) {
	  errx(1, "Can't specify both -r and -a");
	}

	mbr_binary = (char *)malloc(MBR_CODE_SIZE);
	if ((fd = open(mbrfile, O_RDONLY)) == -1) {
	  warn("could not open MBR file %s", mbrfile);
	  bzero(mbr_binary, MBR_CODE_SIZE);
	} else {
	  int cc;
	  cc = read(fd, mbr_binary, MBR_CODE_SIZE);
	  if (cc < MBR_CODE_SIZE) {
	    err(1, "could not read MBR code");
	  }
	  close(fd);
	}

	if (u_flag) {
	  /* Don't hose the partition table; just write the boot code */
	  mp = MBR_read_all(&disk);
	  bcopy(mbr_binary, mp->code, MBR_CODE_SIZE);
	  MBR_make(mp);
	} else if (i_flag) {
	  /* If they didn't specify -a, they'll get the default auto style */
	  mp = MBR_alloc(NULL);
	  if (AUTO_init(&disk, auto_style, mp) != AUTO_OK) {
	    errx(1, "error initializing disk");
	  }
	  bcopy(mbr_binary, mp->code, MBR_CODE_SIZE);
	  MBR_make(mp);
	} else if (r_flag) {
	  mp = MBR_parse_spec(stdin, &disk);
	  bcopy(mbr_binary, mp->code, MBR_CODE_SIZE);
	  MBR_make(mp);
	} else {
	  /* Use what's on the disk. */
	  mp = MBR_read_all(&disk);
	}
	  
	/* Now do what we are supposed to */
	if (i_flag || r_flag || u_flag) {
	  USER_write(&disk, mp, u_flag, y_flag);
	}

	if (m_flag) {
	  USER_modify(&disk, mp, 0, 0);
	}

	if (mbr_binary)
	  free(mbr_binary);

	return (0);
}
