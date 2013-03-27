/* scsi-start / scsi-stop
 * Copyright (C) 1999 Trent Piepho <xyzzy@speakeasy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/sg.h>

#ifdef SCSI_DISK0_MAJOR
#define IS_SCSI_DISK(rdev)	SCSI_DISK_MAJOR(MAJOR(rdev))
#else
#define IS_SCSI_DISK(rdev)	(MAJOR(rdev)==SCSI_DISK_MAJOR)
#endif

#ifndef START_STOP
#define START_STOP            0x1b
#endif

int main(int argc, char *argv[])
{
	int fd, mode;
	struct stat statbuf;

	mode = argv[0][strlen(argv[0])-1];
	if(mode=='p' || mode=='P')  {
		mode = 0;	/* stoP */
	} else if(mode=='t' || mode=='T')  {
		mode = 1;	/* starT */
	} else {
		fprintf(stderr, "Try ending the executable name with 'stop' or 'start'\n");
		exit(1);
	}

	if (argc != 2) {
		fprintf(stderr, "Usage: %s device\n",argv[0]);
		fprintf(stderr, "%s the device's motor\n", mode?"Starts":"Stops");
		exit(1);
	}
	if ((stat(argv[1], &statbuf)) < 0) {
		perror(argv[1]);
		exit(1);
	}
	if (!S_ISBLK(statbuf.st_mode)
		|| !IS_SCSI_DISK(statbuf.st_rdev) )  {
		fprintf(stderr, "%s is not a SCSI block device\n", argv[1]);
		exit(1);
	}
	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror(argv[1]);
		exit(1);
	}

#ifdef SCSI_SG
	char sg_command[] = { START_STOP, 0, 0, 0, 0 /* mode */, 0 };
	sg_io_hdr_t io_hdr;

	sg_command[4] = (char)mode;
	memset(&io_hdr, 0, sizeof(io_hdr));
	io_hdr.interface_id = 'S';
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.cmd_len = sizeof(sg_command);
	io_hdr.cmdp = sg_command;
	io_hdr.timeout = 60 * 1000; /* 1 min */

	if (ioctl(fd, SG_IO, &io_hdr) < 0) {
#else
	if (ioctl(fd, mode?SCSI_IOCTL_START_UNIT:SCSI_IOCTL_STOP_UNIT) < 0) {
#endif
		perror(argv[1]);
		close(fd);
		exit(1);
	}

	close(fd);
	exit(0);
}
