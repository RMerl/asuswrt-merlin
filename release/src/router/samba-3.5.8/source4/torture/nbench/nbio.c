/*
  TODO: add splitting of writes for servers with signing
*/


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
#include "system/time.h"
#include "system/filesys.h"
#include "../lib/util/dlinklist.h"
#include "libcli/libcli.h"
#include "libcli/raw/libcliraw.h"
#include "torture/torture.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "torture/nbench/proto.h"

extern int nbench_line_count;
static int nbio_id = -1;
static int nprocs;
static bool bypass_io;
static struct timeval tv_start, tv_end;
static int warmup, timelimit;
static int in_cleanup;

struct lock_info {
	struct lock_info *next, *prev;
	off_t offset;
	int size;
};

struct createx_params {
	char *fname;
	uint_t create_options;
	uint_t create_disposition;
	int handle;
};

struct ftable {
	struct ftable *next, *prev;
	int fd;     /* the fd that we got back from the server */
	int handle; /* the handle in the load file */
	struct createx_params cp;
	struct lock_info *locks;
};

static struct ftable *ftable;

static struct {
	double bytes, warmup_bytes;
	int line;
	int done;
	bool connected;
	double max_latency;
	struct timeval starttime;
} *children;

static bool nb_do_createx(struct ftable *f,
			  const char *fname,
			  uint_t create_options,
			  uint_t create_disposition,
			  int handle,
			  NTSTATUS status,
			  bool retry);

static bool nb_do_lockx(bool relock, int handle, off_t offset, int size, NTSTATUS status);

static void nb_set_createx_params(struct ftable *f,
				  const char *fname,
				  uint_t create_options,
				  uint_t create_disposition,
				  int handle)
{
	struct createx_params *cp = &f->cp;

	if (fname != NULL) {
		cp->fname = talloc_strdup(f, fname);
		if (cp->fname == NULL) {
			perror("nb_set_createx_params: strdup");
			nb_exit(1);
		}
	} else {
		cp->fname = NULL;
	}

	cp->create_options = create_options;
	cp->create_disposition = create_disposition;
	cp->handle = handle;
}

static bool nb_reestablish_locks(struct ftable *f)
{
	struct lock_info *linfo = f->locks;

	while (linfo != NULL) {
		DEBUG(1,("nb_reestablish_locks: lock for file %d at %lu\n",
			 f->handle, (unsigned long) linfo->offset));

		if (!nb_do_lockx(true, f->handle, linfo->offset, linfo->size, NT_STATUS_OK)) {
			printf("nb_reestablish_locks: failed to get lock for file %s at %lu\n",
			       f->cp.fname, (unsigned long) linfo->offset);
			return false;
		}

		linfo = linfo->next;
	}

	return true;
}

static bool nb_reopen_all_files(void)
{
	struct ftable *f = ftable;

	while (f != NULL) {
		DEBUG(1,("-- nb_reopen_all_files: opening %s (handle %d)\n",
			 f->cp.fname, f->cp.handle));

		if (!nb_do_createx(f,
				   f->cp.fname,
				   f->cp.create_options,
				   f->cp.create_disposition,
				   f->cp.handle,
				   NT_STATUS_OK,
				   true))
		{
			printf("-- nb_reopen_all_files: failed to open file %s\n", f->cp.fname);
			return false;
		}

		if (!nb_reestablish_locks(f)) {
			printf("--nb_reopen_all_files: failed to reestablish locks\n");
			return false;
		}

		f = f->next;
	}

	return true;
}

bool nb_reconnect(struct smbcli_state **cli, struct torture_context *tctx, int client)
{
	children[client].connected = false;

	if (*cli != NULL) {
		talloc_free(*cli);
	}

	if (!torture_open_connection(cli, tctx, client)) {
		printf("nb_reconnect: failed to connect\n");
		*cli = NULL;
		return false;
	}

	nb_setup(*cli, client);

	if (!nb_reopen_all_files()) {
		printf("nb_reconnect: failed to reopen files in client %d\n", client);
		return false;
	}

	return true;
}

void nbio_target_rate(double rate)
{
	static double last_bytes;
	static struct timeval last_time;
	double tdelay;

	if (last_bytes == 0) {
		last_bytes = children[nbio_id].bytes;
		last_time = timeval_current();
		return;
	}

	tdelay = (children[nbio_id].bytes - last_bytes)/(1.0e6*rate) - timeval_elapsed(&last_time);
	if (tdelay > 0) {
		msleep(tdelay*1000);
	} else {
		children[nbio_id].max_latency = MAX(children[nbio_id].max_latency, -tdelay);
	}

	last_time = timeval_current();
	last_bytes = children[nbio_id].bytes;
}

void nbio_time_reset(void)
{
	children[nbio_id].starttime = timeval_current();	
}

void nbio_time_delay(double targett)
{
	double elapsed = timeval_elapsed(&children[nbio_id].starttime);
	if (targett > elapsed) {
		msleep(1000*(targett - elapsed));
	} else if (elapsed - targett > children[nbio_id].max_latency) {
		children[nbio_id].max_latency = MAX(elapsed - targett, children[nbio_id].max_latency);
	}
}

double nbio_result(void)
{
	int i;
	double total = 0;
	for (i=0;i<nprocs;i++) {
		total += children[i].bytes - children[i].warmup_bytes;
	}
	return 1.0e-6 * total / timeval_elapsed2(&tv_start, &tv_end);
}

double nbio_latency(void)
{
	int i;
	double max_latency = 0;
	for (i=0;i<nprocs;i++) {
		if (children[i].max_latency > max_latency) {
			max_latency = children[i].max_latency;
			children[i].max_latency = 0;
		}
	}
	return max_latency;
}

bool nb_tick(void)
{
	return children[nbio_id].done;
}


void nb_alarm(int sig)
{
	int i;
	int lines=0;
	double t;
	int in_warmup = 0;
	int num_connected = 0;

	if (nbio_id != -1) return;

	for (i=0;i<nprocs;i++) {
		if (children[i].connected) {
			num_connected++;
		}
		if (children[i].bytes == 0) {
			in_warmup = 1;
		}
		lines += children[i].line;
	}

	t = timeval_elapsed(&tv_start);

	if (!in_warmup && warmup>0 && t > warmup) {
		tv_start = timeval_current();
		warmup = 0;
		for (i=0;i<nprocs;i++) {
			children[i].warmup_bytes = children[i].bytes;
		}
		goto next;
	}
	if (t < warmup) {
		in_warmup = 1;
	} else if (!in_warmup && !in_cleanup && t > timelimit) {
		for (i=0;i<nprocs;i++) {
			children[i].done = 1;
		}
		tv_end = timeval_current();
		in_cleanup = 1;
	}
	if (t < 1) {
		goto next;
	}
	if (!in_cleanup) {
		tv_end = timeval_current();
	}

	if (in_warmup) {
		printf("%4d  %8d  %.2f MB/sec  warmup %.0f sec   \n", 
		       num_connected, lines/nprocs, 
		       nbio_result(), t);
	} else if (in_cleanup) {
		printf("%4d  %8d  %.2f MB/sec  cleanup %.0f sec   \n", 
		       num_connected, lines/nprocs, 
		       nbio_result(), t);
	} else {
		printf("%4d  %8d  %.2f MB/sec  execute %.0f sec  latency %.2f msec \n", 
		       num_connected, lines/nprocs, 
		       nbio_result(), t, nbio_latency() * 1.0e3);
	}

	fflush(stdout);
next:
	signal(SIGALRM, nb_alarm);
	alarm(1);	
}

void nbio_shmem(int n, int t_timelimit, int t_warmup)
{
	nprocs = n;
	children = shm_setup(sizeof(*children) * nprocs);
	if (!children) {
		printf("Failed to setup shared memory!\n");
		nb_exit(1);
	}
	memset(children, 0, sizeof(*children) * nprocs);
	timelimit = t_timelimit;
	warmup = t_warmup;
	in_cleanup = 0;
	tv_start = timeval_current();
}

static struct lock_info* find_lock(struct lock_info *linfo, off_t offset, int size)
{
	while (linfo != NULL) {
		if (linfo->offset == offset &&
		    linfo->size == size)
		{
			return linfo;
		}

		linfo = linfo->next;
	}

	return NULL;
}

static struct ftable *find_ftable(int handle)
{
	struct ftable *f;

	for (f=ftable;f;f=f->next) {
		if (f->handle == handle) return f;
	}
	return NULL;
}

static int find_handle(int handle, struct ftable **f_ret)
{
	struct ftable *f;

	if (f_ret != NULL)
		*f_ret = NULL;

	children[nbio_id].line = nbench_line_count;

	f = find_ftable(handle);
	if (f) {
		if (f_ret != NULL)
			*f_ret = f;
		return f->fd;
	}
	printf("(%d) ERROR: handle %d was not found\n", 
	       nbench_line_count, handle);
	nb_exit(1);

	return -1;		/* Not reached */
}



static struct smbcli_state *c;

/*
  a handler function for oplock break requests
*/
static bool oplock_handler(struct smbcli_transport *transport, uint16_t tid, 
			   uint16_t fnum, uint8_t level, void *private_data)
{
	struct smbcli_tree *tree = (struct smbcli_tree *)private_data;
	return smbcli_oplock_ack(tree, fnum, OPLOCK_BREAK_TO_NONE);
}

void nb_setup(struct smbcli_state *cli, int id)
{
	nbio_id = id;
	c = cli;
	if (bypass_io)
		printf("skipping I/O\n");

	if (cli) {
		smbcli_oplock_handler(cli->transport, oplock_handler, cli->tree);
	}

	children[id].connected = true;
}


static bool check_status(const char *op, NTSTATUS status, NTSTATUS ret)
{
	if ((NT_STATUS_EQUAL(ret, NT_STATUS_END_OF_FILE) ||
	     NT_STATUS_EQUAL(ret, NT_STATUS_NET_WRITE_FAULT) ||
	     NT_STATUS_EQUAL(ret, NT_STATUS_CONNECTION_RESET)) 
		&& !NT_STATUS_EQUAL (status, ret))
	{
		return false;
	}

	if (!NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(ret)) {
		printf("[%d] Error: %s should have failed with %s\n", 
		       nbench_line_count, op, nt_errstr(status));
		nb_exit(1);
	}

	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(ret)) {
		printf("[%d] Error: %s should have succeeded - %s\n", 
		       nbench_line_count, op, nt_errstr(ret));
		nb_exit(1);
	}

	if (!NT_STATUS_EQUAL(status, ret)) {
		printf("[%d] Warning: got status %s but expected %s\n",
		       nbench_line_count, nt_errstr(ret), nt_errstr(status));
	}

	return true;
}


bool nb_unlink(const char *fname, int attr, NTSTATUS status, bool retry)
{
	union smb_unlink io;
	NTSTATUS ret;

	io.unlink.in.pattern = fname;

	io.unlink.in.attrib = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	if (strchr(fname, '*') == 0) {
		io.unlink.in.attrib |= FILE_ATTRIBUTE_DIRECTORY;
	}

	ret = smb_raw_unlink(c->tree, &io);

	if (!retry)
		return check_status("Unlink", status, ret);

	return true;
}

static bool nb_do_createx(struct ftable *f,
			  const char *fname,
			  uint_t create_options,
			  uint_t create_disposition,
			  int handle,
			  NTSTATUS status,
			  bool retry)
{
	union smb_open io;	
	uint32_t desired_access;
	NTSTATUS ret;
	TALLOC_CTX *mem_ctx;
	uint_t flags = 0;

	mem_ctx = talloc_init("raw_open");

	if (create_options & NTCREATEX_OPTIONS_DIRECTORY) {
		desired_access = SEC_FILE_READ_DATA;
	} else {
		desired_access = 
			SEC_FILE_READ_DATA | 
			SEC_FILE_WRITE_DATA |
			SEC_FILE_READ_ATTRIBUTE |
			SEC_FILE_WRITE_ATTRIBUTE;
		flags = NTCREATEX_FLAGS_EXTENDED |
			NTCREATEX_FLAGS_REQUEST_OPLOCK | 
			NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	}

	io.ntcreatex.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = flags;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.access_mask = desired_access;
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = create_disposition;
	io.ntcreatex.in.create_options = create_options;
	io.ntcreatex.in.impersonation = 0;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	if (retry) {
		/* Reopening after a disconnect. */
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	} else
	if (f != NULL &&
	    f->cp.create_disposition == NTCREATEX_DISP_CREATE &&
	    NT_STATUS_IS_OK(status))
	{
		/* Reopening after nb_createx() error. */
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	}

	ret = smb_raw_open(c->tree, mem_ctx, &io);

	talloc_free(mem_ctx);

	if (!check_status("NTCreateX", status, ret))
		return false;

	if (!NT_STATUS_IS_OK(ret))
		return true;

	if (f == NULL) {
		f = talloc (NULL, struct ftable);
		f->locks = NULL;
		nb_set_createx_params(f, fname, create_options, create_disposition, handle);
		DLIST_ADD_END(ftable, f, struct ftable *);
	}

	f->handle = handle;
	f->fd = io.ntcreatex.out.file.fnum;

	return true;
}

bool nb_createx(const char *fname, 
	       uint_t create_options, uint_t create_disposition, int handle,
	       NTSTATUS status)
{
	return nb_do_createx(NULL, fname, create_options, create_disposition, handle, status, false);
}

bool nb_writex(int handle, off_t offset, int size, int ret_size, NTSTATUS status)
{
	union smb_write io;
	int i;
	NTSTATUS ret;
	uint8_t *buf;

	i = find_handle(handle, NULL);

	if (bypass_io)
		return true;

	buf = malloc(size);
	memset(buf, 0xab, size);

	io.writex.level = RAW_WRITE_WRITEX;
	io.writex.in.file.fnum = i;
	io.writex.in.wmode = 0;
	io.writex.in.remaining = 0;
	io.writex.in.offset = offset;
	io.writex.in.count = size;
	io.writex.in.data = buf;

	ret = smb_raw_write(c->tree, &io);

	free(buf);

	if (!check_status("WriteX", status, ret))
		return false;

	if (NT_STATUS_IS_OK(ret) && io.writex.out.nwritten != ret_size) {
		printf("[%d] Warning: WriteX got count %d expected %d\n", 
		       nbench_line_count,
		       io.writex.out.nwritten, ret_size);
	}	

	children[nbio_id].bytes += ret_size;

	return true;
}

bool nb_write(int handle, off_t offset, int size, int ret_size, NTSTATUS status)
{
	union smb_write io;
	int i;
	NTSTATUS ret;
	uint8_t *buf;

	i = find_handle(handle, NULL);

	if (bypass_io)
		return true;

	buf = malloc(size);

	memset(buf, 0x12, size);

	io.write.level = RAW_WRITE_WRITE;
	io.write.in.file.fnum = i;
	io.write.in.remaining = 0;
	io.write.in.offset = offset;
	io.write.in.count = size;
	io.write.in.data = buf;

	ret = smb_raw_write(c->tree, &io);

	free(buf);

	if (!check_status("Write", status, ret))
		return false;

	if (NT_STATUS_IS_OK(ret) && io.write.out.nwritten != ret_size) {
		printf("[%d] Warning: Write got count %d expected %d\n", 
		       nbench_line_count,
		       io.write.out.nwritten, ret_size);
	}	

	children[nbio_id].bytes += ret_size;

	return true;
}

static bool nb_do_lockx(bool relock, int handle, off_t offset, int size, NTSTATUS status)
{
	union smb_lock io;
	int i;
	NTSTATUS ret;
	struct smb_lock_entry lck;
	struct ftable *f;

	i = find_handle(handle, &f);

	lck.pid = getpid();
	lck.offset = offset;
	lck.count = size;

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = i;
	io.lockx.in.mode = 0;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lck;

	ret = smb_raw_lock(c->tree, &io);

	if (!check_status("LockX", status, ret))
		return false;

	if (f != NULL &&
	    !relock)
	{
		struct lock_info *linfo;
		linfo = talloc (f, struct lock_info);
		linfo->offset = offset;
		linfo->size = size;
		DLIST_ADD_END(f->locks, linfo, struct lock_info *);
	}

	return true;
}

bool nb_lockx(int handle, off_t offset, int size, NTSTATUS status)
{
	return nb_do_lockx(false, handle, offset, size, status);
}

bool nb_unlockx(int handle, uint_t offset, int size, NTSTATUS status)
{
	union smb_lock io;
	int i;
	NTSTATUS ret;
	struct smb_lock_entry lck;
	struct ftable *f;

	i = find_handle(handle, &f);

	lck.pid = getpid();
	lck.offset = offset;
	lck.count = size;

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = i;
	io.lockx.in.mode = 0;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = &lck;

	ret = smb_raw_lock(c->tree, &io);

	if (!check_status("UnlockX", status, ret))
		return false;

	if (f != NULL) {
		struct lock_info *linfo;
		linfo = find_lock(f->locks, offset, size);
		if (linfo != NULL)
			DLIST_REMOVE(f->locks, linfo);
		else
			printf("nb_unlockx: unknown lock (%d)\n", handle);
	}

	return true;
}

bool nb_readx(int handle, off_t offset, int size, int ret_size, NTSTATUS status)
{
	union smb_read io;
	int i;
	NTSTATUS ret;
	uint8_t *buf;

	i = find_handle(handle, NULL);

	if (bypass_io)
		return true;

	buf = malloc(size);

	io.readx.level = RAW_READ_READX;
	io.readx.in.file.fnum = i;
	io.readx.in.offset    = offset;
	io.readx.in.mincnt    = size;
	io.readx.in.maxcnt    = size;
	io.readx.in.remaining = 0;
	io.readx.in.read_for_execute = false;
	io.readx.out.data     = buf;

	ret = smb_raw_read(c->tree, &io);

	free(buf);

	if (!check_status("ReadX", status, ret))
		return false;

	if (NT_STATUS_IS_OK(ret) && io.readx.out.nread != ret_size) {
		printf("[%d] ERROR: ReadX got count %d expected %d\n", 
		       nbench_line_count,
		       io.readx.out.nread, ret_size);
		nb_exit(1);
	}	

	children[nbio_id].bytes += ret_size;

	return true;
}

bool nb_close(int handle, NTSTATUS status)
{
	NTSTATUS ret;
	union smb_close io;
	int i;

	i = find_handle(handle, NULL);

	io.close.level = RAW_CLOSE_CLOSE;
	io.close.in.file.fnum = i;
	io.close.in.write_time = 0;

	ret = smb_raw_close(c->tree, &io);

	if (!check_status("Close", status, ret))
		return false;

	if (NT_STATUS_IS_OK(ret)) {
		struct ftable *f = find_ftable(handle);
		DLIST_REMOVE(ftable, f);
		talloc_free(f);
	}

	return true;
}

bool nb_rmdir(const char *dname, NTSTATUS status, bool retry)
{
	NTSTATUS ret;
	struct smb_rmdir io;

	io.in.path = dname;

	ret = smb_raw_rmdir(c->tree, &io);

	if (!retry)
		return check_status("Rmdir", status, ret);

	return true;
}

bool nb_mkdir(const char *dname, NTSTATUS status, bool retry)
{
	union smb_mkdir io;

	io.mkdir.level = RAW_MKDIR_MKDIR;
	io.mkdir.in.path = dname;

	/* NOTE! no error checking. Used for base fileset creation */
	smb_raw_mkdir(c->tree, &io);

	return true;
}

bool nb_rename(const char *o, const char *n, NTSTATUS status, bool retry)
{
	NTSTATUS ret;
	union smb_rename io;

	io.generic.level = RAW_RENAME_RENAME;
	io.rename.in.attrib = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY;
	io.rename.in.pattern1 = o;
	io.rename.in.pattern2 = n;

	ret = smb_raw_rename(c->tree, &io);

	if (!retry)
		return check_status("Rename", status, ret);

	return true;
}


bool nb_qpathinfo(const char *fname, int level, NTSTATUS status)
{
	union smb_fileinfo io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;

	mem_ctx = talloc_init("nb_qpathinfo");

	io.generic.level = level;
	io.generic.in.file.path = fname;

	ret = smb_raw_pathinfo(c->tree, mem_ctx, &io);

	talloc_free(mem_ctx);

	return check_status("Pathinfo", status, ret);
}


bool nb_qfileinfo(int fnum, int level, NTSTATUS status)
{
	union smb_fileinfo io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;
	int i;

	i = find_handle(fnum, NULL);

	mem_ctx = talloc_init("nb_qfileinfo");

	io.generic.level = level;
	io.generic.in.file.fnum = i;

	ret = smb_raw_fileinfo(c->tree, mem_ctx, &io);

	talloc_free(mem_ctx);

	return check_status("Fileinfo", status, ret);
}

bool nb_sfileinfo(int fnum, int level, NTSTATUS status)
{
	union smb_setfileinfo io;
	NTSTATUS ret;
	int i;

	if (level != RAW_SFILEINFO_BASIC_INFORMATION) {
		printf("[%d] Warning: setfileinfo level %d not handled\n", nbench_line_count, level);
		return true;
	}

	ZERO_STRUCT(io);

	i = find_handle(fnum, NULL);

	io.generic.level = level;
	io.generic.in.file.fnum = i;
	unix_to_nt_time(&io.basic_info.in.create_time, time(NULL));
	unix_to_nt_time(&io.basic_info.in.access_time, 0);
	unix_to_nt_time(&io.basic_info.in.write_time, 0);
	unix_to_nt_time(&io.basic_info.in.change_time, 0);
	io.basic_info.in.attrib = 0;

	ret = smb_raw_setfileinfo(c->tree, &io);

	return check_status("Setfileinfo", status, ret);
}

bool nb_qfsinfo(int level, NTSTATUS status)
{
	union smb_fsinfo io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;

	mem_ctx = talloc_init("smbcli_dskattr");

	io.generic.level = level;
	ret = smb_raw_fsinfo(c->tree, mem_ctx, &io);

	talloc_free(mem_ctx);
	
	return check_status("Fsinfo", status, ret);	
}

/* callback function used for trans2 search */
static bool findfirst_callback(void *private_data, const union smb_search_data *file)
{
	return true;
}

bool nb_findfirst(const char *mask, int level, int maxcnt, int count, NTSTATUS status)
{
	union smb_search_first io;
	TALLOC_CTX *mem_ctx;
	NTSTATUS ret;

	mem_ctx = talloc_init("smbcli_dskattr");

	io.t2ffirst.level = RAW_SEARCH_TRANS2;
	io.t2ffirst.data_level = level;
	io.t2ffirst.in.max_count = maxcnt;
	io.t2ffirst.in.search_attrib = FILE_ATTRIBUTE_DIRECTORY;
	io.t2ffirst.in.pattern = mask;
	io.t2ffirst.in.flags = FLAG_TRANS2_FIND_CLOSE;
	io.t2ffirst.in.storage_type = 0;
			
	ret = smb_raw_search_first(c->tree, mem_ctx, &io, NULL, findfirst_callback);

	talloc_free(mem_ctx);

	if (!check_status("Search", status, ret))
		return false;

	if (NT_STATUS_IS_OK(ret) && io.t2ffirst.out.count != count) {
		printf("[%d] Warning: got count %d expected %d\n", 
		       nbench_line_count,
		       io.t2ffirst.out.count, count);
	}

	return true;
}

bool nb_flush(int fnum, NTSTATUS status)
{
	union smb_flush io;
	NTSTATUS ret;
	int i;
	i = find_handle(fnum, NULL);

	io.flush.level		= RAW_FLUSH_FLUSH;
	io.flush.in.file.fnum	= i;

	ret = smb_raw_flush(c->tree, &io);

	return check_status("Flush", status, ret);
}

void nb_sleep(int usec, NTSTATUS status)
{
	usleep(usec);
}

bool nb_deltree(const char *dname, bool retry)
{
	int total_deleted;

	smb_raw_exit(c->session);

	while (ftable) {
		struct ftable *f = ftable;
		DLIST_REMOVE(ftable, f);
		talloc_free (f);
	}

	total_deleted = smbcli_deltree(c->tree, dname);

	if (total_deleted == -1) {
		printf("Failed to cleanup tree %s - exiting\n", dname);
		nb_exit(1);
	}

	smbcli_rmdir(c->tree, dname);

	return true;
}


void nb_exit(int status)
{
	children[nbio_id].connected = false;
	printf("[%d] client %d exiting with status %d\n",
	       nbench_line_count, nbio_id, status);
	exit(status);
}
