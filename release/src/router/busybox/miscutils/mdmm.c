/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
 *
 */ 

//config:config MDMM
//config:	bool "MDMM Utility"
//config:	default n
//config:	help
//config:	  The md/mm utility can be used to read/write memory and memory
//config:	  mapped registers.

//kbuild:lib-$(CONFIG_MDMM) += mdmm.o

//applet:IF_MDMM(APPLET(md, BB_DIR_USR_SBIN, BB_SUID_DROP))
//applet:IF_MDMM(APPLET(mm, BB_DIR_USR_SBIN, BB_SUID_DROP))

//usage:#define md_trivial_usage
//usage:       "address [count]"
//usage:#define md_full_usage "\n"
//usage:#define mm_trivial_usage
//usage:       "address value"
//usage:#define mm_full_usage "\n"

#include "libbb.h"

/*
 * Simple Atheros-specific tool to inspect and monitor network traffic
 * statistics.
 *	athstats [-i interface] [interval]
 * (default interface is ath0).  If interval is specified a rolling output
 * a la netstat -i is displayed every interval seconds.
 *
 * To build: cc -o athstats athstats.c -lkvm
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
//#include <sys/sockio.h>
#include <sys/socket.h>
#include <net/if.h>
//#include <net/if_media.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <string.h>
#include <limits.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

static void usage(void);
static int opendev(int);
int md_main(int, char *[]);
int mm_main(int, char *[]);

static void
usage(void)
{
	fprintf(stderr, "Usage:\n"
			"md address [count]\n"
			"mm address value\n");
	exit(1);
}

static int
opendev(int mode)
{
	int		fd;
#define AR_MEM_DEV_NAME	"/dev/armem"
	fd = open(AR_MEM_DEV_NAME, mode);

	if (fd < 0) {
		perror("open: " AR_MEM_DEV_NAME);
		fprintf(stderr, "Create using: mknod " AR_MEM_DEV_NAME " c 1 13\n");
	}

	return fd;
}

static int
closedev(int fd)
{
	return close(fd);
}

int md_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int md_main(int argc, char *argv[])
{
	int		i, fd, count;
	unsigned	val;
	loff_t		addr;

	if (argc < 2 || argc > 3) {
		usage();
		return EINVAL;
	}

	if ((fd = opendev(O_RDONLY)) < 0) {
		return fd;
	}

	if (argc == 2) {
		count = 1;
	} else {
		count = atoi(argv[2]);
	}

	addr = strtoul(argv[1], NULL, 16) & 0xffffffff;

	lseek(fd, addr, SEEK_SET);
	for (i = 0; i < count; i++, addr += sizeof(val)) {
		if (read(fd, &val, sizeof(val)) != sizeof(val)) {
			perror("read");
			closedev(fd);
			return -1;
		}
                printf("%08llx : 0x%08x %12d\n", addr, val, val);
	}

	closedev(fd);
	return 0;
}

int mm_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mm_main(int argc, char *argv[])
{
	int		fd;
	unsigned	new;
	loff_t		addr;

	if (argc != 3) {
		usage();
		return EINVAL;
	}

	if ((fd = opendev(O_RDWR)) < 0) {
		return fd;
	}

	addr = strtoul(argv[1], NULL, 16) & 0xffffffff;
	new = strtoul(argv[2], NULL, 16);

	lseek(fd, addr, SEEK_SET);
	if (write(fd, &new, sizeof(new)) != sizeof(new)) {
		perror("write");
		closedev(fd);
		return -1;
	}

	closedev(fd);
	return 0;
}
