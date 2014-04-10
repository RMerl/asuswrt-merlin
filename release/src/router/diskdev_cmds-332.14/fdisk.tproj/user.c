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
#include <string.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <machine/param.h>
#include "user.h"
#include "disk.h"
#include "misc.h"
#include "mbr.h"
#include "cmd.h"


/* Our command table */
static cmd_table_t cmd_table[] = {
	{"help",   Xhelp,	"Command help list"},
	{"manual", Xmanual,	"Show entire man page for fdisk"},
	{"reinit", Xreinit,	"Re-initialize loaded MBR (to defaults)"},
	{"auto",   Xauto,       "Auto-partition the disk with a partition style"},
	{"setpid", Xsetpid,	"Set the identifier of a given table entry"},
	{"disk",   Xdisk,	"Edit current drive stats"},
	{"edit",   Xedit,	"Edit given table entry"},
	{"erase",  Xerase,      "Erase current MBR"},
	{"flag",   Xflag,	"Flag given table entry as bootable"},
	{"update", Xupdate,	"Update machine code in loaded MBR"},
	{"select", Xselect,	"Select extended partition table entry MBR"},
	{"print",  Xprint,	"Print loaded MBR partition table"},
	{"write",  Xwrite,	"Write loaded MBR to disk"},
	{"exit",   Xexit,	"Exit edit of current MBR, without saving changes"},
	{"quit",   Xquit,	"Quit edit of current MBR, saving current changes"},
	{"abort",  Xabort,	"Abort program without saving current changes"},
	{NULL,     NULL,	NULL}
};


int
USER_write(disk, tt, preserve, force)
	disk_t *disk;
	mbr_t *tt;     /* Template MBR to write */
	int preserve;  /* Preserve partition table and just write boot code */
	int force;     /* Don't ask any questions */
{
	int fd, yn;
	char *msgp = "\nDo you wish to write new MBR?";
	char *msgk = "\nDo you wish to write new MBR and partition table?";

	/* Write sector 0 */
	if (force) {
	  yn = 1;
	} else {
	  printf("\a\n"
		 "\t-----------------------------------------------------\n"
		 "\t------ ATTENTION - UPDATING MASTER BOOT RECORD ------\n"
		 "\t-----------------------------------------------------\n");
	  if (preserve)
	    yn = ask_yn(msgp, 0);
	  else
	    yn = ask_yn(msgk, 0);
	}

	if (yn) {
	  if (preserve) {
	    int shared;
	    /* Only write the first one, if there's more than one in an extended partition chain */
	    /* Since we're updating boot code, we don't require exclusive access */
	    fd = DISK_openshared(disk->name, O_RDWR, &shared);
	    MBR_make(tt);
	    MBR_write(fd, tt);
	    DISK_close(fd);
	  } else {
	    MBR_write_all(disk, tt);
	  }
	} else {
	  printf("MBR is unchanged\n");
	}

	return (0);
}


int
USER_modify(disk, tt, offset, reloff)
	disk_t *disk;
	mbr_t *tt;
	off_t offset;
	off_t reloff;
{
	static int editlevel;
	mbr_t *mbr;
	cmd_t cmd;
	int i, st, fd;
	int modified = 0;	

	/* One level deeper */
	editlevel += 1;

	/* Set up command table pointer */
	cmd.table = cmd_table;

	/* Read MBR & partition */
	mbr = MBR_alloc(NULL);
	fd = DISK_open(disk->name, O_RDONLY);
	MBR_read(fd, offset, mbr);
	DISK_close(fd);

	/* Parse the sucker */
	MBR_parse(disk, offset, reloff, mbr);

	if (mbr->signature != MBR_SIGNATURE) {
	    int yn = ask_yn("The signature for this MBR is invalid.\nWould you like to initialize the partition table?", 1);
	    if (yn) {
	      strcpy(cmd.cmd, "erase");
	      cmd.args[0] = '\0';
	      st = Xerase(&cmd, disk, mbr, tt, offset);
	      modified = 1;
	    }
	}

	printf("Enter 'help' for information\n");

	/* Edit cycle */
	do {
again:
		printf("fdisk:%c%d> ", (modified)?'*':' ', editlevel);
		fflush(stdout);
		ask_cmd(&cmd);

		if (cmd.cmd[0] == '\0')
			goto again;
		for (i = 0; cmd_table[i].cmd != NULL; i++)
			if (strstr(cmd_table[i].cmd, cmd.cmd)==cmd_table[i].cmd)
				break;

		/* Quick hack to put in '?' == 'help' */
		if (!strcmp(cmd.cmd, "?"))
			i = 0;

		/* Check for valid command */
		if (cmd_table[i].cmd == NULL) {
			printf("Invalid command '%s'.  Try 'help'.\n", cmd.cmd);
			continue;
		} else
			strcpy(cmd.cmd, cmd_table[i].cmd);

		/* Call function */
		st = cmd_table[i].fcn(&cmd, disk, mbr, tt, offset);

		/* Update status */
		if (st == CMD_EXIT)
			break;
		if (st == CMD_SAVE)
			break;
		if (st == CMD_CLEAN)
			modified = 0;
		if (st == CMD_DIRTY)
			modified = 1;
	} while (1);

	/* Write out MBR */
	if (modified) {
		if (st == CMD_SAVE) {
		  	int shared = 0;
			printf("Writing current MBR to disk.\n");
			fd = DISK_openshared(disk->name, O_RDWR, &shared);
			if(shared) {
       			  if(!ask_yn("Device could not be accessed exclusively.\nA reboot will be needed for changes to take effect. OK?", 0)) {
			    close(fd);
			    goto again;
			  }
			}

			MBR_make(mbr);
			MBR_write(fd, mbr);
			close(fd);
		} else {
	                int yn = ask_yn("MBR was modified; really quit without saving?", 0);
			if (yn) {
				printf("Aborting changes to current MBR.\n");
			} else {
				goto again;
			}
		}
	}

	/* One level less */
	editlevel -= 1;

	MBR_free(mbr);
	
	return (0);
}

int
USER_print_disk(disk, do_dump)
	disk_t *disk;
	int do_dump;
{
	int fd, offset, firstoff;
	mbr_t *mbr;

	fd = DISK_open(disk->name, O_RDONLY);
	offset = firstoff = 0;

	if (!do_dump)
	  DISK_printmetrics(disk);

	mbr = MBR_read_all(disk);
	if (do_dump)
	  MBR_dump_all(mbr);
	else
	  MBR_print_all(mbr);
	MBR_free(mbr);

	return (DISK_close(fd));
}



