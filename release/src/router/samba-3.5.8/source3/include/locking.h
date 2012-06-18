/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup, plus a whole lot more.
   
   Copyright (C) Jeremy Allison   2006
   
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

#ifndef _LOCKING_H
#define _LOCKING_H

/* passed to br lock code - the UNLOCK_LOCK should never be stored into the tdb
   and is used in calculating POSIX unlock ranges only. We differentiate between
   PENDING read and write locks to allow posix lock downgrades to trigger a lock
   re-evaluation. */

enum brl_type {READ_LOCK, WRITE_LOCK, PENDING_READ_LOCK, PENDING_WRITE_LOCK, UNLOCK_LOCK};
enum brl_flavour {WINDOWS_LOCK = 0, POSIX_LOCK = 1};

#define IS_PENDING_LOCK(type) ((type) == PENDING_READ_LOCK || (type) == PENDING_WRITE_LOCK)

/* This contains elements that differentiate locks. The smbpid is a
   client supplied pid, and is essentially the locking context for
   this client */

struct lock_context {
	uint32 smbpid;
	uint16 tid;
	struct server_id pid;
};

struct files_struct;

struct file_id {
	/* we don't use SMB_DEV_T and SMB_INO_T as we want a fixed size here,
	   and we may be using file system specific code to fill in something
	   other than a dev_t for the device */
	uint64_t devid;
	uint64_t inode;
	uint64_t extid; /* Support systems that use an extended id (e.g. snapshots). */
};

struct byte_range_lock {
	struct files_struct *fsp;
	unsigned int num_locks;
	bool modified;
	bool read_only;
	struct file_id key;
	struct lock_struct *lock_data;
	struct db_record *record;
};

/* Internal structure in brlock.tdb. 
   The data in brlock records is an unsorted linear array of these
   records.  It is unnecessary to store the count as tdb provides the
   size of the record */

struct lock_struct {
	struct lock_context context;
	br_off start;
	br_off size;
	uint16 fnum;
	enum brl_type lock_type;
	enum brl_flavour lock_flav;
};

/****************************************************************************
 This is the structure to queue to implement blocking locks.
*****************************************************************************/

struct blocking_lock_record {
	struct blocking_lock_record *next;
	struct blocking_lock_record *prev;
	struct files_struct *fsp;
	struct timeval expire_time;
	int lock_num;
	uint64_t offset;
	uint64_t count;
	uint32_t lock_pid;
	uint32_t blocking_pid; /* PID that blocks us. */
	enum brl_flavour lock_flav;
	enum brl_type lock_type;
	struct smb_request *req;
	void *blr_private; /* Implementation specific. */
};

#endif /* _LOCKING_H_ */
