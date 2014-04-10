/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * History:
 * March 25, 2002	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <sys/vnioctl.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <mach/boolean.h>
#include <sys/disk.h>

void
usage()
{
    fprintf(stderr, "usage: vncontrol <command> <args>\n"
	    "vncontrol attach <dev_node> <image_file>\n"
	    "vncontrol shadow <dev_node> <shadow_file>\n"
	    "vncontrol detach <dev_node>\n");
    exit(1);
}

enum {
  kAttach,
  kDetach,
  kShadow,
};

int
main(int argc, char * argv[])
{
    int fd;
    struct vn_ioctl vn;
    int op = kAttach;
    struct stat sb;

    if (argc < 2) {
	usage();
    }
    if (strcmp(argv[1], "attach") == 0) {
	if (argc < 4) {
	    usage();
	}
	op = kAttach;
    }
    else if (strcmp(argv[1], "detach") == 0) {
	if (argc < 3) {
	    usage();
	}
	op = kDetach;
    }
    else if (strcmp(argv[1], "shadow") == 0) {
	if (argc < 4) {
	    usage();
	}
	op = kShadow;
    }
    else {
	usage();
    }
    fd = open(argv[2], O_RDONLY, 0);
    if (fd < 0) {
	perror(argv[2]);
	exit(2);
    }
    switch (op) {
    case kAttach:
    case kShadow:
	if (stat(argv[3], &sb) < 0) {
	    perror(argv[3]);
	    exit(2);
	}
	break;
    default:
	break;
    }
    bzero(&vn, sizeof(vn));
    switch (op) {
    case kAttach:
	vn.vn_file = argv[3];
	vn.vn_control = vncontrol_readwrite_io_e;
    
	if (ioctl(fd, VNIOCATTACH, &vn) < 0) {
	    perror("VNIOCATTACH");
	    exit(2);
	}
	break;
    case kDetach:
	if (ioctl(fd, VNIOCDETACH, &vn) < 0) {
	    perror("VNIOCDETACH");
	    exit(2);
	}
	break;
    case kShadow:
	vn.vn_file = argv[3];
	if (ioctl(fd, VNIOCSHADOW, &vn) < 0) {
	    perror("VNIOCSHADOW");
	    exit(2);
	}
	break;
    }
    close(fd);
    exit(0);
    return (0);
}
