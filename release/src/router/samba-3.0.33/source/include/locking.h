/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup, plus a whole lot more.
   
   Copyright (C) Jeremy Allison   2006
   
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
	struct process_id pid;
};

/* The key used in the brlock database. */

struct lock_key {
	SMB_DEV_T device;
	SMB_INO_T inode;
};

struct files_struct;

struct byte_range_lock {
	struct files_struct *fsp;
	unsigned int num_locks;
	BOOL modified;
	BOOL read_only;
	struct lock_key key;
	void *lock_data;
};

#define BRLOCK_FN_CAST() \
	void (*)(SMB_DEV_T dev, SMB_INO_T ino, struct process_id pid, \
				 enum brl_type lock_type, \
				 enum brl_flavour lock_flav, \
				 br_off start, br_off size)

#define BRLOCK_FN(fn) \
	void (*fn)(SMB_DEV_T dev, SMB_INO_T ino, struct process_id pid, \
				 enum brl_type lock_type, \
				 enum brl_flavour lock_flav, \
				 br_off start, br_off size)

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

#endif /* _LOCKING_H_ */
