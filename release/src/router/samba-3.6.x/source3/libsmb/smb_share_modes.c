/*
   Samba share mode database library external interface library.
   Used by non-Samba products needing access to the Samba share mode db.

   Copyright (C) Jeremy Allison 2005 - 2006

   sharemodes_procid functions (C) Copyright (C) Volker Lendecke 2005

     ** NOTE! The following LGPL license applies to this module only.
     ** This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "smb_share_modes.h"
#include <tdb.h>

/* Database context handle. */
struct smbdb_ctx {
	TDB_CONTEXT *smb_tdb;
};

/* Remove the paranoid malloc checker. */
#ifdef malloc
#undef malloc
#endif

int smb_create_share_mode_entry_ex(struct smbdb_ctx *db_ctx, uint64_t dev,
				uint64_t ino, uint64_t extid,
				const struct smb_share_mode_entry *new_entry,
				const char *sharepath, const char *filename);

static bool sharemodes_procid_equal(const struct server_id *p1, const struct server_id *p2)
{
	return (p1->pid == p2->pid);
}

static pid_t sharemodes_procid_to_pid(const struct server_id *proc)
{
	return proc->pid;
}

/*
 * open/close sharemode database.
 */

struct smbdb_ctx *smb_share_mode_db_open(const char *db_path)
{
	struct smbdb_ctx *smb_db = (struct smbdb_ctx *)malloc(sizeof(struct smbdb_ctx));

	if (!smb_db) {
		return NULL;
	}

	memset(smb_db, '\0', sizeof(struct smbdb_ctx));

	smb_db->smb_tdb = tdb_open(db_path,
				0, TDB_DEFAULT|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
				O_RDWR|O_CREAT,
				0644);

	if (!smb_db->smb_tdb) {
		free(smb_db);
		return NULL;
	}

	/* Should check that this is the correct version.... */
	return smb_db;
}

/* key and data records in the tdb locking database */
struct locking_key {
        SMB_DEV_T dev;
        SMB_INO_T inode;
	uint64_t extid;
};

int smb_share_mode_db_close(struct smbdb_ctx *db_ctx)
{
	int ret = tdb_close(db_ctx->smb_tdb);
	free(db_ctx);
	return ret;
}

static TDB_DATA get_locking_key(struct locking_key *lk, uint64_t dev,
				uint64_t ino, uint64_t extid)
{
	TDB_DATA ld;

	memset(lk, '\0', sizeof(*lk));
	lk->dev = (SMB_DEV_T)dev;
	lk->inode = (SMB_INO_T)ino;
	lk->extid = extid;
	ld.dptr = (uint8 *)lk;
	ld.dsize = sizeof(*lk);
	return ld;
}

/*
 * lock/unlock entry in sharemode database.
 */

int smb_lock_share_mode_entry(struct smbdb_ctx *db_ctx,
				uint64_t dev,
				uint64_t ino,
				uint64_t extid)
{
	struct locking_key lk;
	return tdb_chainlock(db_ctx->smb_tdb, get_locking_key(&lk, dev, ino,
							      extid));
}

int smb_unlock_share_mode_entry(struct smbdb_ctx *db_ctx,
                                uint64_t dev,
                                uint64_t ino,
                                uint64_t extid)
{
	struct locking_key lk;
	return tdb_chainunlock(db_ctx->smb_tdb,
			       get_locking_key(&lk, dev, ino, extid));
}

/*
 * Check if an external smb_share_mode_entry and an internal share_mode entry match.
 */

static int share_mode_entry_equal(const struct smb_share_mode_entry *e_entry,
				const struct share_mode_entry *entry)
{
	return (sharemodes_procid_equal(&e_entry->pid, &entry->pid) &&
		e_entry->file_id == (uint32_t)entry->share_file_id &&
		e_entry->open_time.tv_sec == entry->time.tv_sec &&
		e_entry->open_time.tv_usec == entry->time.tv_usec &&
		e_entry->share_access == (uint32_t)entry->share_access &&
		e_entry->access_mask == (uint32_t)entry->access_mask &&
		e_entry->dev == entry->id.devid && 
		e_entry->ino == entry->id.inode &&
		e_entry->extid == entry->id.extid);
}

/*
 * Create an internal Samba share_mode entry from an external smb_share_mode_entry.
 */

static void create_share_mode_entry(struct share_mode_entry *out,
				const struct smb_share_mode_entry *in,
				uint32_t name_hash)
{
	memset(out, '\0', sizeof(struct share_mode_entry));

	out->pid = in->pid;
	out->share_file_id = (unsigned long)in->file_id;
	out->time.tv_sec = in->open_time.tv_sec;
	out->time.tv_usec = in->open_time.tv_usec;
	out->share_access = in->share_access;
	out->access_mask = in->access_mask;
	out->id.devid = in->dev;
	out->id.inode = in->ino;
	out->id.extid = in->extid;
	out->uid = (uint32)geteuid();
	out->flags = 0;
	out->name_hash = name_hash;
}

/*
 * Return the current share mode list for an open file.
 * This uses similar (but simplified) logic to locking/locking.c
 */

int smb_get_share_mode_entries(struct smbdb_ctx *db_ctx,
				uint64_t dev,
				uint64_t ino,
				uint64_t extid,
				struct smb_share_mode_entry **pp_list,
				unsigned char *p_delete_on_close)
{
	struct locking_key lk;
	TDB_DATA db_data;
	struct smb_share_mode_entry *list = NULL;
	int num_share_modes = 0;
	struct locking_data *ld = NULL; /* internal samba db state. */
	struct share_mode_entry *shares = NULL;
	size_t i;
	int list_num;

	*pp_list = NULL;
	*p_delete_on_close = 0;

	db_data = tdb_fetch(db_ctx->smb_tdb, get_locking_key(&lk, dev, ino,
							     extid));
	if (!db_data.dptr) {
		return 0;
	}

	ld = (struct locking_data *)db_data.dptr;
	num_share_modes = ld->u.s.num_share_mode_entries;

	if (!num_share_modes) {
		free(db_data.dptr);
		return 0;
	}

	list = (struct smb_share_mode_entry *)malloc(sizeof(struct smb_share_mode_entry)*num_share_modes);
	if (!list) {
		free(db_data.dptr);
		return -1;
	}

	memset(list, '\0', num_share_modes * sizeof(struct smb_share_mode_entry));

	shares = (struct share_mode_entry *)(db_data.dptr + sizeof(struct locking_data));

	list_num = 0;
	for (i = 0; i < num_share_modes; i++) {
		struct share_mode_entry *share = &shares[i];
		struct smb_share_mode_entry *sme = &list[list_num];
		struct server_id pid = share->pid;

		/* Check this process really exists. */
		if (kill(sharemodes_procid_to_pid(&pid), 0) == -1 && (errno == ESRCH)) {
			continue; /* No longer exists. */
		}

		/* Ignore deferred open entries. */
		if (share->op_type == DEFERRED_OPEN_ENTRY) {
			continue;
		}

		/* Copy into the external list. */
		sme->dev = share->id.devid;
		sme->ino = share->id.inode;
		sme->extid = share->id.extid;
		sme->share_access = (uint32_t)share->share_access;
		sme->access_mask = (uint32_t)share->access_mask;
		sme->open_time.tv_sec = share->time.tv_sec;
		sme->open_time.tv_usec = share->time.tv_usec;
        	sme->file_id = (uint32_t)share->share_file_id;
		sme->pid = share->pid;
		list_num++;
	}

	if (list_num == 0) {
		free(db_data.dptr);
		free(list);
		return 0;
	}

	*p_delete_on_close = ld->u.s.num_delete_token_entries != 0;
	*pp_list = list;
	free(db_data.dptr);
	return list_num;
}

static uint32_t smb_name_hash(const char *sharepath, const char *filename, int *err)
{
	TDB_DATA key;
	char *fullpath = NULL;
	size_t sharepath_size = strlen(sharepath);
	size_t filename_size = strlen(filename);
	uint32_t name_hash;

	*err = 0;
	fullpath = (char *)malloc(sharepath_size + filename_size + 2);
	if (fullpath == NULL) {
		*err = 1;
		return 0;
	}
	memcpy(fullpath, sharepath, sharepath_size);
	fullpath[sharepath_size] = '/';
	memcpy(&fullpath[sharepath_size + 1], filename, filename_size + 1);

	key.dptr = (uint8_t *)fullpath;
	key.dsize = strlen(fullpath) + 1;
	name_hash = tdb_jenkins_hash(&key);
	free(fullpath);
	return name_hash;
}

/* 
 * Create an entry in the Samba share mode db.
 */

int smb_create_share_mode_entry_ex(struct smbdb_ctx *db_ctx,
				uint64_t dev,
				uint64_t ino,
				uint64_t extid,
				const struct smb_share_mode_entry *new_entry,
				const char *sharepath, /* Must be absolute utf8 path. */
				const char *filename) /* Must be relative utf8 path. */
{
	TDB_DATA db_data;
	struct locking_key lk;
	TDB_DATA locking_key =  get_locking_key(&lk, dev, ino, extid);
	int orig_num_share_modes = 0;
	struct locking_data *ld = NULL; /* internal samba db state. */
	struct share_mode_entry *shares = NULL;
	uint8 *new_data_p = NULL;
	size_t new_data_size = 0;
	int err = 0;
	uint32_t name_hash = smb_name_hash(sharepath, filename, &err);

	if (err) {
		return -1;
	}

	db_data = tdb_fetch(db_ctx->smb_tdb, locking_key);
	if (!db_data.dptr) {
		/* We must create the entry. */
		db_data.dptr = (uint8 *)malloc(
			sizeof(struct locking_data) +
			sizeof(struct share_mode_entry) +
			strlen(sharepath) + 1 +
			strlen(filename) + 1);
		if (!db_data.dptr) {
			return -1;
		}
		ld = (struct locking_data *)db_data.dptr;
		memset(ld, '\0', sizeof(struct locking_data));
		ld->u.s.num_share_mode_entries = 1;
		ld->u.s.num_delete_token_entries = 0;
		shares = (struct share_mode_entry *)(db_data.dptr + sizeof(struct locking_data));
		create_share_mode_entry(shares, new_entry, name_hash);

		memcpy(db_data.dptr + sizeof(struct locking_data) + sizeof(struct share_mode_entry),
			sharepath,
			strlen(sharepath) + 1);
		memcpy(db_data.dptr + sizeof(struct locking_data) + sizeof(struct share_mode_entry) +
			strlen(sharepath) + 1,
			filename,
			strlen(filename) + 1);

		db_data.dsize = sizeof(struct locking_data) + sizeof(struct share_mode_entry) +
					strlen(sharepath) + 1 +
					strlen(filename) + 1;
		if (tdb_store(db_ctx->smb_tdb, locking_key, db_data, TDB_INSERT) == -1) {
			free(db_data.dptr);
			return -1;
		}
		free(db_data.dptr);
		return 0;
	}

	/* Entry exists, we must add a new entry. */
	new_data_p = (uint8 *)malloc(
		db_data.dsize + sizeof(struct share_mode_entry));
	if (!new_data_p) {
		free(db_data.dptr);
		return -1;
	}

	ld = (struct locking_data *)db_data.dptr;
	orig_num_share_modes = ld->u.s.num_share_mode_entries;

	/* Copy the original data. */
	memcpy(new_data_p, db_data.dptr, sizeof(struct locking_data) + (orig_num_share_modes * sizeof(struct share_mode_entry)));

	/* Add in the new share mode */
	shares = (struct share_mode_entry *)(new_data_p + sizeof(struct locking_data) +
			(orig_num_share_modes * sizeof(struct share_mode_entry)));

	create_share_mode_entry(shares, new_entry, name_hash);

	ld = (struct locking_data *)new_data_p;
	ld->u.s.num_share_mode_entries++;

	/* Append the original delete_tokens and filenames. */
	memcpy(new_data_p + sizeof(struct locking_data) + (ld->u.s.num_share_mode_entries * sizeof(struct share_mode_entry)),
		db_data.dptr + sizeof(struct locking_data) + (orig_num_share_modes * sizeof(struct share_mode_entry)),
		db_data.dsize - sizeof(struct locking_data) - (orig_num_share_modes * sizeof(struct share_mode_entry)));

	new_data_size = db_data.dsize + sizeof(struct share_mode_entry);

	free(db_data.dptr);

	db_data.dptr = new_data_p;
	db_data.dsize = new_data_size;

	if (tdb_store(db_ctx->smb_tdb, locking_key, db_data, TDB_REPLACE) == -1) {
		free(db_data.dptr);
		return -1;
	}
	free(db_data.dptr);
	return 0;
}

/* 
 * Create an entry in the Samba share mode db. Original interface - doesn't
 * Distinguish between share path and filename. Fudge this by using a
 * sharepath of / and a relative filename of (filename+1).
 */

int smb_create_share_mode_entry(struct smbdb_ctx *db_ctx,
				uint64_t dev,
				uint64_t ino,
				uint64_t extid,
				const struct smb_share_mode_entry *new_entry,
				const char *filename) /* Must be absolute utf8 path. */
{
	if (*filename != '/') {
		abort();
	}
	return smb_create_share_mode_entry_ex(db_ctx, dev, ino, extid, new_entry,
						"/", &filename[1]);
}

int smb_delete_share_mode_entry(struct smbdb_ctx *db_ctx,
				uint64_t dev,
				uint64_t ino,
				uint64_t extid,
				const struct smb_share_mode_entry *del_entry)
{
	TDB_DATA db_data;
	struct locking_key lk;
	TDB_DATA locking_key =  get_locking_key(&lk, dev, ino, extid);
	int orig_num_share_modes = 0;
	struct locking_data *ld = NULL; /* internal samba db state. */
	struct share_mode_entry *shares = NULL;
	uint8 *new_data_p = NULL;
	size_t remaining_size = 0;
	size_t i, num_share_modes;
	const uint8 *remaining_ptr = NULL;

	db_data = tdb_fetch(db_ctx->smb_tdb, locking_key);
	if (!db_data.dptr) {
		return -1; /* Error - missing entry ! */
	}

	ld = (struct locking_data *)db_data.dptr;
	orig_num_share_modes = ld->u.s.num_share_mode_entries;
	shares = (struct share_mode_entry *)(db_data.dptr + sizeof(struct locking_data));

	if (orig_num_share_modes == 1) {
		/* Only one entry - better be ours... */
		if (!share_mode_entry_equal(del_entry, shares)) {
			/* Error ! We can't delete someone else's entry ! */
			free(db_data.dptr);
			return -1;
		}
		/* It's ours - just remove the entire record. */
		free(db_data.dptr);
		return tdb_delete(db_ctx->smb_tdb, locking_key);
	}

	/* More than one - allocate a new record minus the one we'll delete. */
	new_data_p = (uint8 *)malloc(
		db_data.dsize - sizeof(struct share_mode_entry));
	if (!new_data_p) {
		free(db_data.dptr);
		return -1;
	}

	/* Copy the header. */
	memcpy(new_data_p, db_data.dptr, sizeof(struct locking_data));

	num_share_modes = 0;
	for (i = 0; i < orig_num_share_modes; i++) {
		struct share_mode_entry *share = &shares[i];
		struct server_id pid = share->pid;

		/* Check this process really exists. */
		if (kill(sharemodes_procid_to_pid(&pid), 0) == -1 && (errno == ESRCH)) {
			continue; /* No longer exists. */
		}

		if (share_mode_entry_equal(del_entry, share)) {
			continue; /* This is our delete taget. */
		}

		memcpy(new_data_p + sizeof(struct locking_data) +
				(num_share_modes * sizeof(struct share_mode_entry)),
			share, sizeof(struct share_mode_entry) );

		num_share_modes++;
	}

	if (num_share_modes == 0) {
		/* None left after pruning. Delete record. */
		free(db_data.dptr);
		free(new_data_p);
		return tdb_delete(db_ctx->smb_tdb, locking_key);
	}

	/* Copy any delete tokens plus the terminating filenames. */
	remaining_ptr = db_data.dptr + sizeof(struct locking_data) + (orig_num_share_modes * sizeof(struct share_mode_entry));
	remaining_size = db_data.dsize - (remaining_ptr - db_data.dptr);

	memcpy(new_data_p + sizeof(struct locking_data) + (num_share_modes * sizeof(struct share_mode_entry)),
		remaining_ptr,
		remaining_size);

	free(db_data.dptr);

	db_data.dptr = new_data_p;

	/* Re-save smaller record. */
	ld = (struct locking_data *)db_data.dptr;
	ld->u.s.num_share_mode_entries = num_share_modes;

	db_data.dsize = sizeof(struct locking_data) + (num_share_modes * sizeof(struct share_mode_entry)) + remaining_size;

	if (tdb_store(db_ctx->smb_tdb, locking_key, db_data, TDB_REPLACE) == -1) {
		free(db_data.dptr);
		return -1;
	}
	free(db_data.dptr);
	return 0;
}

int smb_change_share_mode_entry(struct smbdb_ctx *db_ctx,
				uint64_t dev,
				uint64_t ino,
				uint64_t extid,
				const struct smb_share_mode_entry *set_entry,
				const struct smb_share_mode_entry *new_entry)
{
	TDB_DATA db_data;
	struct locking_key lk;
	TDB_DATA locking_key =  get_locking_key(&lk, dev, ino, extid);
	int num_share_modes = 0;
	struct locking_data *ld = NULL; /* internal samba db state. */
	struct share_mode_entry *shares = NULL;
	size_t i;
	int found_entry = 0;

	db_data = tdb_fetch(db_ctx->smb_tdb, locking_key);
	if (!db_data.dptr) {
		return -1; /* Error - missing entry ! */
	}

	ld = (struct locking_data *)db_data.dptr;
	num_share_modes = ld->u.s.num_share_mode_entries;
	shares = (struct share_mode_entry *)(db_data.dptr + sizeof(struct locking_data));

	for (i = 0; i < num_share_modes; i++) {
		struct share_mode_entry *share = &shares[i];
		struct server_id pid = share->pid;

		/* Check this process really exists. */
		if (kill(sharemodes_procid_to_pid(&pid), 0) == -1 && (errno == ESRCH)) {
			continue; /* No longer exists. */
		}

		if (share_mode_entry_equal(set_entry, share)) {
			create_share_mode_entry(share, new_entry, share->name_hash);
			found_entry = 1;
			break;
		}
	}

	if (!found_entry) {
		free(db_data.dptr);
		return -1;
	}

	/* Save modified data. */
	if (tdb_store(db_ctx->smb_tdb, locking_key, db_data, TDB_REPLACE) == -1) {
		free(db_data.dptr);
		return -1;
	}
	free(db_data.dptr);
	return 0;
}
