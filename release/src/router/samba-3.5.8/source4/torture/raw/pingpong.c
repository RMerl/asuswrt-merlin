/* 
   Unix SMB/CIFS implementation.

   ping pong test

   Copyright (C) Ronnie Sahlberg 2007

   Significantly based on and borrowed from lockbench.c by
   Copyright (C) Andrew Tridgell 2006
   
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

/*
   filename is specified by
	 --option=torture:filename=...

   number of locks is specified by
	 --option=torture:num_locks=...

   locktimeout is specified in ms by
	 --option=torture:locktimeout=...

       default is 100 seconds
       if set to 0 pingpong will instead loop trying the lock operation
       over and over until it completes.

   reading from the file can be enabled with
	 --option=torture:read=true

   writing to the file can be enabled with
	 --option=torture:write=true

*/
#include "includes.h"
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"

static void lock_byte(struct smbcli_state *cli, int fd, int offset, int lock_timeout)
{
	union smb_lock io;
	struct smb_lock_entry lock;
	NTSTATUS status;

try_again:
	ZERO_STRUCT(lock);
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;

	lock.count = 1;
	lock.offset = offset;
	lock.pid = cli->tree->session->pid;
	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = lock_timeout;
	io.lockx.in.locks = &lock;
	io.lockx.in.file.fnum = fd;

	status = smb_raw_lock(cli->tree, &io);

	/* If we dont use timeouts and we got file lock conflict
	   just try the lock again.
	*/
	if (lock_timeout==0) {
		if ( (NT_STATUS_EQUAL(NT_STATUS_FILE_LOCK_CONFLICT, status))
		   ||(NT_STATUS_EQUAL(NT_STATUS_LOCK_NOT_GRANTED, status)) ) {
			goto try_again;
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Lock failed\n"));
		exit(1);
	}
}

static void unlock_byte(struct smbcli_state *cli, int fd, int offset)
{
	union smb_lock io;
	struct smb_lock_entry lock;
	NTSTATUS status;

	ZERO_STRUCT(lock);
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;

	lock.count = 1;
	lock.offset = offset;
	lock.pid = cli->tree->session->pid;
	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 100000;
	io.lockx.in.locks = &lock;
	io.lockx.in.file.fnum = fd;

	status = smb_raw_lock(cli->tree, &io);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unlock failed\n"));
		exit(1);
	}
}

static void write_byte(struct smbcli_state *cli, int fd, uint8_t c, int offset)
{
	union smb_write io;
	NTSTATUS status;

	io.generic.level = RAW_WRITE_WRITEX;
	io.writex.in.file.fnum = fd;
	io.writex.in.offset = offset;
	io.writex.in.wmode = 0;
	io.writex.in.remaining = 0;
	io.writex.in.count = 1;
	io.writex.in.data = &c;

	status = smb_raw_write(cli->tree, &io);
	if (!NT_STATUS_IS_OK(status)) {
		printf("write failed\n");
		exit(1);
	}
}	

static void read_byte(struct smbcli_state *cli, int fd, uint8_t *c, int offset)
{
	union smb_read io;
	NTSTATUS status;

	io.generic.level = RAW_READ_READX;
	io.readx.in.file.fnum = fd;
	io.readx.in.mincnt = 1;
	io.readx.in.maxcnt = 1;
	io.readx.in.offset = offset;
	io.readx.in.remaining = 0;
	io.readx.in.read_for_execute = false;
	io.readx.out.data = c;

	status = smb_raw_read(cli->tree, &io);
	if (!NT_STATUS_IS_OK(status)) {
		printf("read failed\n");
		exit(1);
	}
}	


static struct timeval tp1, tp2;

static void start_timer(void)
{
	gettimeofday(&tp1, NULL);
}

static double end_timer(void)
{
	gettimeofday(&tp2, NULL);
	return (tp2.tv_sec + (tp2.tv_usec*1.0e-6)) - 
		(tp1.tv_sec + (tp1.tv_usec*1.0e-6));
}

/* 
   ping pong
*/
bool torture_ping_pong(struct torture_context *torture)
{
	const char *fn;
	int num_locks;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	static bool do_reads;
	static bool do_writes;
	int lock_timeout;
	int fd;
	struct smbcli_state *cli;
	int i;
	uint8_t incr=0, last_incr=0;
	uint8_t *val;
	int count, loops;

	fn = torture_setting_string(torture, "filename", NULL);
	if (fn == NULL) {
		DEBUG(0,("You must specify the filename using --option=torture:filename=...\n"));
		return false;
	}

	num_locks = torture_setting_int(torture, "num_locks", -1);
	if (num_locks == -1) {
		DEBUG(0,("You must specify num_locks using --option=torture:num_locks=...\n"));
		return false;
	}

	do_reads     = torture_setting_bool(torture, "read", false);
	do_writes    = torture_setting_bool(torture, "write", false);
	lock_timeout =  torture_setting_int(torture, "lock_timeout", 100000);

	if (!torture_open_connection(&cli, torture, 0)) {
		DEBUG(0,("Could not open connection\n"));
		return false;
	}

	fd = smbcli_open(cli->tree, fn, O_RDWR|O_CREAT, DENY_NONE);
	if (fd == -1) {
		printf("Failed to open %s\n", fn);
		exit(1);
	}

	write_byte(cli, fd, 0, num_locks);
	lock_byte(cli, fd, 0, lock_timeout);


	start_timer();
	val = talloc_zero_array(mem_ctx, uint8_t, num_locks);
	i = 0;
	count = 0;
	loops = 0;
	while (1) {
		lock_byte(cli, fd, (i+1)%num_locks, lock_timeout);

		if (do_reads) {
			uint8_t c;
			read_byte(cli, fd, &c, i);
			incr   = c-val[i];
			val[i] = c;			
		}

		if (do_writes) {
			uint8_t c = val[i] + 1;
			write_byte(cli, fd, c, i);
		}

		unlock_byte(cli, fd, i);

		i = (i+1)%num_locks;
		count++;
		if (loops>num_locks && incr!=last_incr) {
			last_incr = incr;
			printf("data increment = %u\n", incr);
			fflush(stdout);
		}
		if (end_timer() > 1.0) {
			printf("%8u locks/sec\r", 
			       (unsigned)(2*count/end_timer()));
			fflush(stdout);
			start_timer();
			count=0;
		}
		loops++;
	}

	talloc_free(mem_ctx);
	return true;
}

