#define NBDEBUG 0

/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "torture/proto.h"
#include "../libcli/security/security.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"

#define MAX_FILES 1000

static char buf[70000];
extern int line_count;
extern int nbio_id;
static int nprocs;
static struct timeval nb_start;

static struct {
	int fd;
	int handle;
} ftable[MAX_FILES];

static struct children {
	double bytes_in, bytes_out;
	int line;
	int done;
} *children;

double nbio_total(void)
{
	int i;
	double total = 0;
	for (i=0;i<nprocs;i++) {
		total += children[i].bytes_out + children[i].bytes_in;
	}
	return total;
}

void nb_alarm(int ignore)
{
	int i;
	int lines=0, num_clients=0;
	if (nbio_id != -1) return;

	for (i=0;i<nprocs;i++) {
		lines += children[i].line;
		if (!children[i].done) num_clients++;
	}

	printf("%4d  %8d  %.2f MB/sec\r",
	       num_clients, lines/nprocs,
	       1.0e-6 * nbio_total() / timeval_elapsed(&nb_start));

	signal(SIGALRM, nb_alarm);
	alarm(1);	
}

void nbio_shmem(int n)
{
	nprocs = n;
	children = (struct children *)shm_setup(sizeof(*children) * nprocs);
	if (!children) {
		printf("Failed to setup shared memory!\n");
		exit(1);
	}
}

#if 0
static int ne_find_handle(int handle)
{
	int i;
	children[nbio_id].line = line_count;
	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == handle) return i;
	}
	return -1;
}
#endif

static int find_handle(int handle)
{
	int i;
	children[nbio_id].line = line_count;
	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == handle) return i;
	}
	printf("(%d) ERROR: handle %d was not found\n", 
	       line_count, handle);
	exit(1);

	return -1;		/* Not reached */
}


static struct cli_state *c;

static void sigsegv(int sig)
{
	char line[200];
	printf("segv at line %d\n", line_count);
	slprintf(line, sizeof(line), "/usr/X11R6/bin/xterm -e gdb /proc/%d/exe %d", 
		(int)getpid(), (int)getpid());
	if (system(line) == -1) {
		printf("system() failed\n");
	}
	exit(1);
}

void nb_setup(struct cli_state *cli)
{
	signal(SIGSEGV, sigsegv);
	c = cli;
	nb_start = timeval_current();
	children[nbio_id].done = 0;
}


void nb_unlink(const char *fname)
{
	if (!NT_STATUS_IS_OK(cli_unlink(c, fname, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN))) {
#if NBDEBUG
		printf("(%d) unlink %s failed (%s)\n", 
		       line_count, fname, cli_errstr(c));
#endif
	}
}


void nb_createx(const char *fname, 
		unsigned create_options, unsigned create_disposition, int handle)
{
	uint16_t fd = (uint16_t)-1;
	int i;
	NTSTATUS status;
	uint32 desired_access;

	if (create_options & FILE_DIRECTORY_FILE) {
		desired_access = FILE_READ_DATA;
	} else {
		desired_access = FILE_READ_DATA | FILE_WRITE_DATA;
	}

	status = cli_ntcreate(c, fname, 0, 
				desired_access,
				0x0,
				FILE_SHARE_READ|FILE_SHARE_WRITE, 
				create_disposition, 
				create_options, 0, &fd);
	if (!NT_STATUS_IS_OK(status) && handle != -1) {
		printf("ERROR: cli_ntcreate failed for %s - %s\n",
		       fname, cli_errstr(c));
		exit(1);
	}
	if (NT_STATUS_IS_OK(status) && handle == -1) {
		printf("ERROR: cli_ntcreate succeeded for %s\n", fname);
		exit(1);
	}
	if (fd == (uint16_t)-1) return;

	for (i=0;i<MAX_FILES;i++) {
		if (ftable[i].handle == 0) break;
	}
	if (i == MAX_FILES) {
		printf("(%d) file table full for %s\n", line_count, 
		       fname);
		exit(1);
	}
	ftable[i].handle = handle;
	ftable[i].fd = fd;
}

void nb_writex(int handle, int offset, int size, int ret_size)
{
	int i;
	NTSTATUS status;

	if (buf[0] == 0) memset(buf, 1, sizeof(buf));

	i = find_handle(handle);
	status = cli_writeall(c, ftable[i].fd, 0, (uint8_t *)buf, offset, size,
			      NULL);
	if (!NT_STATUS_IS_OK(status)) {
		printf("(%d) ERROR: write failed on handle %d, fd %d "
		       "error %s\n", line_count, handle, ftable[i].fd,
		       nt_errstr(status));
		exit(1);
	}

	children[nbio_id].bytes_out += ret_size;
}

void nb_readx(int handle, int offset, int size, int ret_size)
{
	int i, ret;

	i = find_handle(handle);
	if ((ret=cli_read(c, ftable[i].fd, buf, offset, size)) != ret_size) {
		printf("(%d) ERROR: read failed on handle %d ofs=%d size=%d res=%d fd %d errno %d (%s)\n",
			line_count, handle, offset, size, ret, ftable[i].fd, errno, strerror(errno));
		exit(1);
	}
	children[nbio_id].bytes_in += ret_size;
}

void nb_close(int handle)
{
	int i;
	i = find_handle(handle);
	if (!NT_STATUS_IS_OK(cli_close(c, ftable[i].fd))) {
		printf("(%d) close failed on handle %d\n", line_count, handle);
		exit(1);
	}
	ftable[i].handle = 0;
}

void nb_rmdir(const char *fname)
{
	if (!NT_STATUS_IS_OK(cli_rmdir(c, fname))) {
		printf("ERROR: rmdir %s failed (%s)\n", 
		       fname, cli_errstr(c));
		exit(1);
	}
}

void nb_rename(const char *oldname, const char *newname)
{
	if (!NT_STATUS_IS_OK(cli_rename(c, oldname, newname))) {
		printf("ERROR: rename %s %s failed (%s)\n", 
		       oldname, newname, cli_errstr(c));
		exit(1);
	}
}


void nb_qpathinfo(const char *fname)
{
	cli_qpathinfo1(c, fname, NULL, NULL, NULL, NULL, NULL);
}

void nb_qfileinfo(int fnum)
{
	int i;
	i = find_handle(fnum);
	cli_qfileinfo_basic(c, ftable[i].fd, NULL, NULL, NULL, NULL, NULL,
			    NULL, NULL);
}

void nb_qfsinfo(int level)
{
	int bsize, total, avail;
	/* this is not the right call - we need cli_qfsinfo() */
	cli_dskattr(c, &bsize, &total, &avail);
}

static NTSTATUS find_fn(const char *mnt, struct file_info *finfo, const char *name,
		    void *state)
{
	/* noop */
	return NT_STATUS_OK;
}

void nb_findfirst(const char *mask)
{
	cli_list(c, mask, 0, find_fn, NULL);
}

void nb_flush(int fnum)
{
	int i;
	i = find_handle(fnum);
	/* hmmm, we don't have cli_flush() yet */
}

static int total_deleted;

static NTSTATUS delete_fn(const char *mnt, struct file_info *finfo,
		      const char *name, void *state)
{
	NTSTATUS status;
	char *s, *n;
	if (finfo->name[0] == '.') {
		return NT_STATUS_OK;
	}

	n = SMB_STRDUP(name);
	n[strlen(n)-1] = 0;
	if (asprintf(&s, "%s%s", n, finfo->name) == -1) {
		printf("asprintf failed\n");
		return NT_STATUS_NO_MEMORY;
	}
	if (finfo->mode & FILE_ATTRIBUTE_DIRECTORY) {
		char *s2;
		if (asprintf(&s2, "%s\\*", s) == -1) {
			printf("asprintf failed\n");
			return NT_STATUS_NO_MEMORY;
		}
		status = cli_list(c, s2, FILE_ATTRIBUTE_DIRECTORY, delete_fn, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			free(n);
			free(s2);
			return status;
		}
		nb_rmdir(s);
	} else {
		total_deleted++;
		nb_unlink(s);
	}
	free(s);
	free(n);
	return NT_STATUS_OK;
}

void nb_deltree(const char *dname)
{
	char *mask;
	if (asprintf(&mask, "%s\\*", dname) == -1) {
		printf("asprintf failed\n");
		return;
	}

	total_deleted = 0;
	cli_list(c, mask, FILE_ATTRIBUTE_DIRECTORY, delete_fn, NULL);
	free(mask);
	cli_rmdir(c, dname);

	if (total_deleted) printf("WARNING: Cleaned up %d files\n", total_deleted);
}


void nb_cleanup(void)
{
	cli_rmdir(c, "clients");
	children[nbio_id].done = 1;
}
