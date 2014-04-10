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

#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/fcntl.h>
#include "disk.h"
#include "misc.h"
#include "user.h"
#include "part.h"
#include "cmd.h"
#include "auto.h"
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

int
Xerase(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
        bzero(mbr->part, sizeof(mbr->part));
	mbr->signature = MBR_SIGNATURE;
	return (CMD_DIRTY);
}

int
Xreinit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	/* Copy template MBR */
	MBR_make(tt);
	MBR_parse(disk, offset, 0, mbr);

	MBR_init(disk, mbr);

	/* Tell em we did something */
	printf("In memory copy is initialized to:\n");
	printf("Offset: %d\t", offset);
	MBR_print(mbr);
	printf("Use 'write' to update disk.\n");

	return (CMD_DIRTY);
}

int
Xauto(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	if (cmd->args[0] == '\0') {
	    printf("usage: auto <style>\n");
	    printf("  where style is one of:\n");
	    AUTO_print_styles(stdout);
	    return (CMD_CONT);
	}

        if (AUTO_init(disk, cmd->args, mbr) != AUTO_OK) {
	    return (CMD_CONT);
	}
	MBR_make(mbr);
	return (CMD_DIRTY);
}

int
Xdisk(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int maxcyl  = 1024;
	int maxhead = 256;
	int maxsec  = 63;

	/* Print out disk info */
	DISK_printmetrics(disk);

#if defined (__powerpc__) || defined (__mips__)
	maxcyl  = 9999999;
	maxhead = 9999999;
	maxsec  = 9999999;
#endif

	/* Ask for new info */
	if (ask_yn("Change disk geometry?", 0)) {
		disk->real->cylinders = ask_num("BIOS Cylinders", ASK_DEC,
		    disk->real->cylinders, 1, maxcyl, NULL);
		disk->real->heads = ask_num("BIOS Heads", ASK_DEC,
		    disk->real->heads, 1, maxhead, NULL);
		disk->real->sectors = ask_num("BIOS Sectors", ASK_DEC,
		    disk->real->sectors, 1, maxsec, NULL);

		disk->real->size = disk->real->cylinders * disk->real->heads
			* disk->real->sectors;
	}

	return (CMD_CONT);
}

int
Xedit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int pn, num, ret;
	prt_t *pp;

	ret = CMD_CONT;

	if (!isdigit(cmd->args[0])) {
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return (ret);
	}
	pn = atoi(cmd->args) - 1;

	if (pn < 0 || pn > 3) {
		printf("Invalid partition number.\n");
		return (ret);
	}

	/* Print out current table entry */
	pp = &mbr->part[pn];
	PRT_print(0, NULL);
	PRT_print(pn, pp);

#define	EDIT(p, f, v, n, m, h)				\
	if ((num = ask_num(p, f, v, n, m, h)) != v)	\
		ret = CMD_DIRTY;			\
	v = num;

	/* Ask for partition type */
	EDIT("Partition id ('0' to disable) ", ASK_HEX, pp->id, 0, 0xFF, PRT_printall);

	/* Unused, so just zero out */
	if (pp->id == DOSPTYP_UNUSED) {
		memset(pp, 0, sizeof(*pp));
		printf("Partition %d is disabled.\n", pn + 1);
		return (ret);
	}

	/* Change table entry */
	if (ask_yn("Do you wish to edit in CHS mode?", 0)) {
		int maxcyl, maxhead, maxsect;

		/* Shorter */
		maxcyl = disk->real->cylinders - 1;
		maxhead = disk->real->heads - 1;
		maxsect = disk->real->sectors;

		/* Get data */
		EDIT("BIOS Starting cylinder", ASK_DEC, pp->scyl,  0, maxcyl, NULL);
		EDIT("BIOS Starting head",     ASK_DEC, pp->shead, 0, maxhead, NULL);
		EDIT("BIOS Starting sector",   ASK_DEC, pp->ssect, 1, maxsect, NULL);
		EDIT("BIOS Ending cylinder",   ASK_DEC, pp->ecyl,  0, maxcyl, NULL);
		EDIT("BIOS Ending head",       ASK_DEC, pp->ehead, 0, maxhead, NULL);
		EDIT("BIOS Ending sector",     ASK_DEC, pp->esect, 1, maxsect, NULL);
		/* Fix up off/size values */
		PRT_fix_BN(disk, pp, pn);
		/* Fix up CHS values for LBA */
		PRT_fix_CHS(disk, pp, pn);
	} else {
		u_int m;

		if (pn == 0) {
			pp->bs = 63 + offset;
		} else {
			if (mbr->part[pn-1].id != 0) {
				pp->bs = mbr->part[pn-1].bs + mbr->part[pn-1].ns;
			}
		}
		/* Get data */
		EDIT("Partition offset", ASK_DEC, pp->bs, 0,
		    disk->real->size, NULL);
		m = MAX(pp->ns, disk->real->size - pp->bs);
		if ( m > disk->real->size - pp->bs) {
			/* dont have default value extend beyond end of disk */
			m = disk->real->size - pp->bs;
		}
		pp->ns = m;
		EDIT("Partition size", ASK_DEC, pp->ns, 1,
		    m, NULL);

		/* Fix up CHS values */
		PRT_fix_CHS(disk, pp, pn);
	}
#undef EDIT
	return (ret);
}

int
Xsetpid(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int pn, num, ret;
	prt_t *pp;

	ret = CMD_CONT;

	if (!isdigit(cmd->args[0])) {
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return (ret);
	}
	pn = atoi(cmd->args) - 1;

	if (pn < 0 || pn > 3) {
		printf("Invalid partition number.\n");
		return (ret);
	}

	/* Print out current table entry */
	pp = &mbr->part[pn];
	PRT_print(0, NULL);
	PRT_print(pn, pp);

#define	EDIT(p, f, v, n, m, h)				\
	if ((num = ask_num(p, f, v, n, m, h)) != v)	\
		ret = CMD_DIRTY;			\
	v = num;

	/* Ask for partition type */
	EDIT("Partition id ('0' to disable) ", ASK_HEX, pp->id, 0, 0xFF, PRT_printall);

#undef EDIT
	return (ret);
}
int
Xselect(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	static int firstoff = 0;
	int off;
	int pn;

	if (!isdigit(cmd->args[0])) {
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return (CMD_CONT);
	}

	pn = atoi(cmd->args) - 1;
	if (pn < 0 || pn > 3) {
		printf("Invalid partition number.\n");
		return (CMD_CONT);
	}

	off = mbr->part[pn].bs;

	/* Sanity checks */
	if ((mbr->part[pn].id != DOSPTYP_EXTEND) &&
	    (mbr->part[pn].id != DOSPTYP_EXTENDL)) {
		printf("Partition %d is not an extended partition.\n", pn + 1);
		return (CMD_CONT);
	}

	if (firstoff == 0)
		firstoff = off;

	if (!off) {
		printf("Loop to offset 0!  Not selected.\n");
		return (CMD_CONT);
	} else {
		printf("Selected extended partition %d\n", pn + 1);
		printf("New MBR at offset %d.\n", off);
	}

	/* Recursion is beautifull! */
	USER_modify(disk, tt, off, firstoff);
	return (CMD_CONT);
}

int
Xprint(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	DISK_printmetrics(disk);
	printf("Offset: %d\t", offset);
	MBR_print(mbr);

	return (CMD_CONT);
}

int
Xwrite(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int fd;
	int shared = 0;

	fd = DISK_openshared(disk->name, O_RDWR, &shared);
	if(shared) {
	  if(!ask_yn("Device could not be accessed exclusively.\nA reboot will be needed for changes to take effect. OK?", 0)) {
	    close(fd);
	    printf("MBR unchanged\n");
	    return (CMD_CONT);
	  }
	}

	printf("Writing MBR at offset %d.\n", offset);

	MBR_make(mbr);
	MBR_write(fd, mbr);
	close(fd);
	return (CMD_CLEAN);
}

int
Xquit(cmd, disk, r, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *r;
	mbr_t *tt;
	int offset;
{

	/* Nothing to do here */
	return (CMD_SAVE);
}

int
Xabort(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	exit(0);

	/* NOTREACHED */
	return (CMD_CONT);
}


int
Xexit(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{

	/* Nothing to do here */
	return (CMD_EXIT);
}

int
Xhelp(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	cmd_table_t *cmd_table = cmd->table;
	int i;

	/* Hmm, print out cmd_table here... */
	for (i = 0; cmd_table[i].cmd != NULL; i++)
		printf("\t%s\t\t%s\n", cmd_table[i].cmd, cmd_table[i].help);
	return (CMD_CONT);
}

int
Xupdate(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	extern char *mbr_binary;
	/* Update code */
	memcpy(mbr->code, mbr_binary, MBR_CODE_SIZE);
	printf("Machine code updated.\n");
	return (CMD_DIRTY);
}

int
Xflag(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	int i, pn = -1;

	/* Parse partition table entry number */
	if (!isdigit(cmd->args[0])) {
		printf("Invalid argument: %s <partition number>\n", cmd->cmd);
		return (CMD_CONT);
	}
	pn = atoi(cmd->args) - 1;

	if (pn < 0 || pn > 3) {
		printf("Invalid partition number.\n");
		return (CMD_CONT);
	}

	/* Set active flag */
	for (i = 0; i < 4; i++) {
		if (i == pn)
			mbr->part[i].flag = DOSACTIVE;
		else
			mbr->part[i].flag = 0x00;
	}

	printf("Partition %d marked active.\n", pn + 1);
	return (CMD_DIRTY);
}

int
Xmanual(cmd, disk, mbr, tt, offset)
	cmd_t *cmd;
	disk_t *disk;
	mbr_t *mbr;
	mbr_t *tt;
	int offset;
{
	char *pager = "/usr/bin/less";
	char *p;
	sig_t opipe;
	extern char manpage[];
	FILE *f;

	opipe = signal(SIGPIPE, SIG_IGN);
	if ((p = getenv("PAGER")) != NULL && (*p != '\0'))
		pager = p;
	f = popen(pager, "w");
	if (f) {
		(void) fwrite(manpage, strlen(manpage), 1, f);
		pclose(f);
	}

	(void)signal(SIGPIPE, opipe);
	return (CMD_CONT);
}
