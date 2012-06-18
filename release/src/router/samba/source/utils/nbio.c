#define NBDEBUG 0

/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define NO_SYSLOG

#include "includes.h"

#define MAX_FILES 1000

static char buf[70000];
extern int line_count;

static struct {
	int fd;
	int handle;
} ftable[MAX_FILES];

static struct cli_state *c;

static void sigsegv(int sig)
{
	char line[200];
	printf("segv at line %d\n", line_count);
	slprintf(line, sizeof(line), "/usr/X11R6/bin/xterm -e gdb /proc/%d/exe %d", 
		getpid(), getpid());
	system(line);
	exit(1);
}

void nb_setup(struct cli_state *cli)
{
	signal(SIGSEGV, sigsegv);
	/* to be like a true Windows client we need to negotiate oplocks */
	cli->use_oplocks = True;
	c = cli;
}


void nb_unlink(char *fname)
{
	strupper(fname);

	if (!cli_unlink(c, fname)) {
#if NBDEBUG
		printf("(%d) unlink %s failed (%s)\n", 
		       line_count, fname, cli_errstr(c));
#endif
	}
}

void nb_open(char *fname, int handle, int size)
{
	int fd, i;
	int flags = O_RDWR|O_CREAT;
	size_t st_size;
	static int count;

	strupper(fname);

	if (size == 0) flags |= O_TRUNC;

	fd = cli_open(c, fname, flags, DENY_NONE);
	if (fd == -1) {
#if NBDEBUG
		printf("(%d) open %s failed for handle %d (%s)\n", 
		       line_count, fname, handle, cli_errstr(c));
#endif
		return;
	}
	cli_getattrE(c, fd, NULL, &st_size, NULL, NULL, NULL);
	if (size > st_size) {
#if NBDEBUG
		printf("(%d) needs expanding %s to %d from %d\n", 
		       line_count, fname, size, (int)st_size);
#endif
	} else if (size < st_size) {
#if NBDEBUG
		printf("(%d) needs truncating %s to %d from %d\n", 
		       line_count, fname, size, (int)st_size);
#endif
	}
	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == 0) break;
	}
	if (i == MAX_FILES) {
		printf("file table full for %s\n", fname);
		exit(1);
	}
	ftable[i].handle = handle;
	ftable[i].fd = fd;
	if (count++ % 100 == 0) {
		printf(".");
	}
}

void nb_write(int handle, int size, int offset)
{
	int i;

	if (buf[0] == 0) memset(buf, 1, sizeof(buf));

	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == handle) break;
	}
	if (i == MAX_FILES) {
#if NBDEBUG
		printf("(%d) nb_write: handle %d was not open size=%d ofs=%d\n", 
		       line_count, handle, size, offset);
#endif
		return;
	}
	if (cli_smbwrite(c, ftable[i].fd, buf, offset, size) != size) {
		printf("(%d) write failed on handle %d\n", 
		       line_count, handle);
	}
}

void nb_read(int handle, int size, int offset)
{
	int i, ret;

	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == handle) break;
	}
	if (i == MAX_FILES) {
		printf("(%d) nb_read: handle %d was not open size=%d ofs=%d\n", 
		       line_count, handle, size, offset);
		return;
	}
	if ((ret=cli_read(c, ftable[i].fd, buf, offset, size)) != size) {
#if NBDEBUG
		printf("(%d) read failed on handle %d ofs=%d size=%d res=%d\n", 
		       line_count, handle, offset, size, ret);
#endif
	}
}

void nb_close(int handle)
{
	int i;
	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == handle) break;
	}
	if (i == MAX_FILES) {
		printf("(%d) nb_close: handle %d was not open\n", 
		       line_count, handle);
		return;
	}
	cli_close(c, ftable[i].fd);
	ftable[i].handle = 0;
}

void nb_mkdir(char *fname)
{
	strupper(fname);

	if (!cli_mkdir(c, fname)) {
#if NBDEBUG
		printf("mkdir %s failed (%s)\n", 
		       fname, cli_errstr(c));
#endif
	}
}

void nb_rmdir(char *fname)
{
	strupper(fname);

	if (!cli_rmdir(c, fname)) {
#if NBDEBUG
		printf("rmdir %s failed (%s)\n", 
		       fname, cli_errstr(c));
#endif
	}
}

void nb_rename(char *old, char *new)
{
	strupper(old);
	strupper(new);

	if (!cli_rename(c, old, new)) {
#if NBDEBUG
		printf("rename %s %s failed (%s)\n", 
		       old, new, cli_errstr(c));
#endif
	}
}


void nb_stat(char *fname, int size)
{
	size_t st_size;

	strupper(fname);

	if (!cli_getatr(c, fname, NULL, &st_size, NULL)) {
#if NBDEBUG
		printf("(%d) nb_stat: %s size=%d %s\n", 
		       line_count, fname, size, cli_errstr(c));
#endif
		return;
	}
	if (st_size != size) {
#if NBDEBUG
		printf("(%d) nb_stat: %s wrong size %d %d\n", 
		       line_count, fname, (int)st_size, size);
#endif
	}
}

void nb_create(char *fname, int size)
{
	nb_open(fname, 5000, size);
	nb_close(5000);
}
