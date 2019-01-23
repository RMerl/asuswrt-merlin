/*
 * Unix SMB/CIFS implementation.
 *
 * A general tdb content validation mechanism
 *
 * Copyright (C) Michael Adam      2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "system/filesys.h"
#include "util_tdb.h"
#include "tdb_validate.h"

/*
 * internal validation function, executed by the child.
 */
static int tdb_validate_child(struct tdb_context *tdb,
			      tdb_validate_data_func validate_fn)
{
	int ret = 1;
	int num_entries = 0;
	struct tdb_validation_status v_status;

	v_status.tdb_error = False;
	v_status.bad_freelist = False;
	v_status.bad_entry = False;
	v_status.unknown_key = False;
	v_status.success = True;

	if (!tdb) {
		v_status.tdb_error = True;
		v_status.success = False;
		goto out;
	}

	/*
	 * we can simplify this by passing a check function,
	 * but I don't want to change all the callers...
	 */
	ret = tdb_check(tdb, NULL, NULL);
	if (ret == -1) {
		v_status.tdb_error = True;
		v_status.success = False;
		goto out;
	}

	/* Check if the tdb's freelist is good. */
	if (tdb_validate_freelist(tdb, &num_entries) == -1) {
		v_status.bad_freelist = True;
		v_status.success = False;
		goto out;
	}

	DEBUG(10,("tdb_validate_child: tdb %s freelist has %d entries\n",
		  tdb_name(tdb), num_entries));

	/* Now traverse the tdb to validate it. */
	num_entries = tdb_traverse(tdb, validate_fn, (void *)&v_status);
	if (!v_status.success) {
		goto out;
	} else if (num_entries == -1) {
		v_status.tdb_error = True;
		v_status.success = False;
		goto out;
	}

	DEBUG(10,("tdb_validate_child: tdb %s is good with %d entries\n",
		  tdb_name(tdb), num_entries));
	ret = 0; /* Cache is good. */

out:
	DEBUG(10,   ("tdb_validate_child: summary of validation status:\n"));
	DEBUGADD(10,(" * tdb error: %s\n", v_status.tdb_error ? "yes" : "no"));
	DEBUGADD(10,(" * bad freelist: %s\n",v_status.bad_freelist?"yes":"no"));
	DEBUGADD(10,(" * bad entry: %s\n", v_status.bad_entry ? "yes" : "no"));
	DEBUGADD(10,(" * unknown key: %s\n", v_status.unknown_key?"yes":"no"));
	DEBUGADD(10,(" => overall success: %s\n", v_status.success?"yes":"no"));

	return ret;
}

/*
 * tdb validation function.
 * returns 0 if tdb is ok, != 0 if it isn't.
 * this function expects an opened tdb.
 */
int tdb_validate(struct tdb_context *tdb, tdb_validate_data_func validate_fn)
{
	pid_t child_pid = -1;
	int child_status = 0;
	int wait_pid = 0;
	int ret = 1;

	if (tdb == NULL) {
		DEBUG(1, ("Error: tdb_validate called with tdb == NULL\n"));
		return ret;
	}

	DEBUG(5, ("tdb_validate called for tdb '%s'\n", tdb_name(tdb)));

	/* fork and let the child do the validation.
	 * benefit: no need to twist signal handlers and panic functions.
	 * just let the child panic. we catch the signal. */

	DEBUG(10, ("tdb_validate: forking to let child do validation.\n"));
	child_pid = sys_fork();
	if (child_pid == 0) {
		/* child code */
		DEBUG(10, ("tdb_validate (validation child): created\n"));
		DEBUG(10, ("tdb_validate (validation child): "
			   "calling tdb_validate_child\n"));
		exit(tdb_validate_child(tdb, validate_fn));
	}
	else if (child_pid < 0) {
		DEBUG(1, ("tdb_validate: fork for validation failed.\n"));
		goto done;
	}

	/* parent */

	DEBUG(10, ("tdb_validate: fork succeeded, child PID = %u\n",
		(unsigned int)child_pid));

	DEBUG(10, ("tdb_validate: waiting for child to finish...\n"));
	while  ((wait_pid = sys_waitpid(child_pid, &child_status, 0)) < 0) {
		if (errno == EINTR) {
			DEBUG(10, ("tdb_validate: got signal during waitpid, "
				   "retrying\n"));
			errno = 0;
			continue;
		}
		DEBUG(1, ("tdb_validate: waitpid failed with error '%s'.\n",
			  strerror(errno)));
		goto done;
	}
	if (wait_pid != child_pid) {
		DEBUG(1, ("tdb_validate: waitpid returned pid %d, "
			  "but %u was expected\n", wait_pid, (unsigned int)child_pid));
		goto done;
	}

	DEBUG(10, ("tdb_validate: validating child returned.\n"));
	if (WIFEXITED(child_status)) {
		DEBUG(10, ("tdb_validate: child exited, code %d.\n",
			   WEXITSTATUS(child_status)));
		ret = WEXITSTATUS(child_status);
	}
	if (WIFSIGNALED(child_status)) {
		DEBUG(10, ("tdb_validate: child terminated by signal %d\n",
			   WTERMSIG(child_status)));
#ifdef WCOREDUMP
		if (WCOREDUMP(child_status)) {
			DEBUGADD(10, ("core dumped\n"));
		}
#endif
		ret = WTERMSIG(child_status);
	}
	if (WIFSTOPPED(child_status)) {
		DEBUG(10, ("tdb_validate: child was stopped by signal %d\n",
			   WSTOPSIG(child_status)));
		ret = WSTOPSIG(child_status);
	}

done:
	DEBUG(5, ("tdb_validate returning code '%d' for tdb '%s'\n", ret,
		  tdb_name(tdb)));

	return ret;
}

/*
 * tdb validation function.
 * returns 0 if tdb is ok, != 0 if it isn't.
 * this is a wrapper around the actual validation function that opens and closes
 * the tdb.
 */
int tdb_validate_open(const char *tdb_path, tdb_validate_data_func validate_fn)
{
	TDB_CONTEXT *tdb = NULL;
	int ret = 1;

	DEBUG(5, ("tdb_validate_open called for tdb '%s'\n", tdb_path));

	tdb = tdb_open_log(tdb_path, 0, TDB_DEFAULT, O_RDWR, 0);
	if (!tdb) {
		DEBUG(1, ("Error opening tdb %s\n", tdb_path));
		return ret;
	}

	ret = tdb_validate(tdb, validate_fn);
	tdb_close(tdb);
	return ret;
}

/*
 * tdb backup function and helpers for tdb_validate wrapper with backup
 * handling.
 */

/* this structure eliminates the need for a global overall status for
 * the traverse-copy */
struct tdb_copy_data {
	struct tdb_context *dst;
	bool success;
};

static int traverse_copy_fn(struct tdb_context *tdb, TDB_DATA key,
			    TDB_DATA dbuf, void *private_data)
{
	struct tdb_copy_data *data = (struct tdb_copy_data *)private_data;

	if (tdb_store(data->dst, key, dbuf, TDB_INSERT) != 0) {
		DEBUG(4, ("Failed to insert into %s: %s\n", tdb_name(data->dst),
			  strerror(errno)));
		data->success = False;
		return 1;
	}
	return 0;
}

static int tdb_copy(struct tdb_context *src, struct tdb_context *dst)
{
	struct tdb_copy_data data;
	int count;

	data.dst = dst;
	data.success = True;

	count = tdb_traverse(src, traverse_copy_fn, (void *)(&data));
	if ((count < 0) || (data.success == False)) {
		return -1;
	}
	return count;
}

static int tdb_verify_basic(struct tdb_context *tdb)
{
	return tdb_traverse(tdb, NULL, NULL);
}

/* this backup function is essentially taken from lib/tdb/tools/tdbbackup.tdb
 */
static int tdb_backup(TALLOC_CTX *ctx, const char *src_path,
		      const char *dst_path, int hash_size)
{
	struct tdb_context *src_tdb = NULL;
	struct tdb_context *dst_tdb = NULL;
	char *tmp_path = NULL;
	struct stat st;
	int count1, count2;
	int saved_errno = 0;
	int ret = -1;

	if (stat(src_path, &st) != 0) {
		DEBUG(3, ("Could not stat '%s': %s\n", src_path,
			  strerror(errno)));
		goto done;
	}

	/* open old tdb RDWR - so we can lock it */
	src_tdb = tdb_open_log(src_path, 0, TDB_DEFAULT, O_RDWR, 0);
	if (src_tdb == NULL) {
		DEBUG(3, ("Failed to open tdb '%s'\n", src_path));
		goto done;
	}

	if (tdb_lockall(src_tdb) != 0) {
		DEBUG(3, ("Failed to lock tdb '%s'\n", src_path));
		goto done;
	}

	tmp_path = talloc_asprintf(ctx, "%s%s", dst_path, ".tmp");
	if (!tmp_path) {
		DEBUG(3, ("talloc fail\n"));
		goto done;
	}

	unlink(tmp_path);
	dst_tdb = tdb_open_log(tmp_path,
			       hash_size ? hash_size : tdb_hash_size(src_tdb),
			       TDB_DEFAULT, O_RDWR | O_CREAT | O_EXCL,
			       st.st_mode & 0777);
	if (dst_tdb == NULL) {
		DEBUG(3, ("Error creating tdb '%s': %s\n", tmp_path,
			  strerror(errno)));
		saved_errno = errno;
		unlink(tmp_path);
		goto done;
	}

	count1 = tdb_copy(src_tdb, dst_tdb);
	if (count1 < 0) {
		DEBUG(3, ("Failed to copy tdb '%s': %s\n", src_path,
			  strerror(errno)));
		tdb_close(dst_tdb);
		goto done;
	}

	/* reopen ro and do basic verification */
	tdb_close(dst_tdb);
	dst_tdb = tdb_open_log(tmp_path, 0, TDB_DEFAULT, O_RDONLY, 0);
	if (!dst_tdb) {
		DEBUG(3, ("Failed to reopen tdb '%s': %s\n", tmp_path,
			  strerror(errno)));
		goto done;
	}
	count2 = tdb_verify_basic(dst_tdb);
	if (count2 != count1) {
		DEBUG(3, ("Failed to verify result of copying tdb '%s'.\n",
			  src_path));
		tdb_close(dst_tdb);
		goto done;
	}

	DEBUG(10, ("tdb_backup: successfully copied %d entries\n", count1));

	/* make sure the new tdb has reached stable storage
	 * then rename it to its destination */
	fsync(tdb_fd(dst_tdb));
	tdb_close(dst_tdb);
	unlink(dst_path);
	if (rename(tmp_path, dst_path) != 0) {
		DEBUG(3, ("Failed to rename '%s' to '%s': %s\n",
			  tmp_path, dst_path, strerror(errno)));
		goto done;
	}

	/* success */
	ret = 0;

done:
	if (src_tdb != NULL) {
		tdb_close(src_tdb);
	}
	if (tmp_path != NULL) {
		unlink(tmp_path);
		TALLOC_FREE(tmp_path);
	}
	if (saved_errno != 0) {
		errno = saved_errno;
	}
	return ret;
}

static int rename_file_with_suffix(TALLOC_CTX *ctx, const char *path,
				   const char *suffix)
{
	int ret = -1;
	char *dst_path;

	dst_path = talloc_asprintf(ctx, "%s%s", path, suffix);
	if (dst_path == NULL) {
		DEBUG(3, ("error out of memory\n"));
		return ret;
	}

	ret = (rename(path, dst_path) != 0);

	if (ret == 0) {
		DEBUG(5, ("moved '%s' to '%s'\n", path, dst_path));
	} else if (errno == ENOENT) {
		DEBUG(3, ("file '%s' does not exist - so not moved\n", path));
		ret = 0;
	} else {
		DEBUG(3, ("error renaming %s to %s: %s\n", path, dst_path,
			  strerror(errno)));
	}

	TALLOC_FREE(dst_path);
	return ret;
}

/*
 * do a backup of a tdb, moving the destination out of the way first
 */
static int tdb_backup_with_rotate(TALLOC_CTX *ctx, const char *src_path,
				  const char *dst_path, int hash_size,
				  const char *rotate_suffix,
				  bool retry_norotate_if_nospc,
				  bool rename_as_last_resort_if_nospc)
{
	int ret;

	rename_file_with_suffix(ctx, dst_path, rotate_suffix);

	ret = tdb_backup(ctx, src_path, dst_path, hash_size);

	if (ret != 0) {
		DEBUG(10, ("backup of %s failed: %s\n", src_path, strerror(errno)));
	}
	if ((ret != 0) && (errno == ENOSPC) && retry_norotate_if_nospc)
	{
		char *rotate_path = talloc_asprintf(ctx, "%s%s", dst_path,
						    rotate_suffix);
		if (rotate_path == NULL) {
			DEBUG(10, ("talloc fail\n"));
			return -1;
		}
		DEBUG(10, ("backup of %s failed due to lack of space\n",
			   src_path));
		DEBUGADD(10, ("trying to free some space by removing rotated "
			      "dst %s\n", rotate_path));
		if (unlink(rotate_path) == -1) {
			DEBUG(10, ("unlink of %s failed: %s\n", rotate_path,
				   strerror(errno)));
		} else {
			ret = tdb_backup(ctx, src_path, dst_path, hash_size);
		}
		TALLOC_FREE(rotate_path);
	}

	if ((ret != 0) && (errno == ENOSPC) && rename_as_last_resort_if_nospc)
	{
		DEBUG(10, ("backup of %s failed due to lack of space\n", 
			   src_path));
		DEBUGADD(10, ("using 'rename' as a last resort\n"));
		ret = rename(src_path, dst_path);
	}

	return ret;
}

/*
 * validation function with backup handling:
 *
 *  - calls tdb_validate
 *  - if the tdb is ok, create a backup "name.bak", possibly moving
 *    existing backup to name.bak.old,
 *    return 0 (success) even if the backup fails
 *  - if the tdb is corrupt:
 *    - move the tdb to "name.corrupt"
 *    - check if there is valid backup.
 *	if so, restore the backup.
 *	if restore is successful, return 0 (success),
 *    - otherwise return -1 (failure)
 */
int tdb_validate_and_backup(const char *tdb_path,
			    tdb_validate_data_func validate_fn)
{
	int ret = -1;
	const char *backup_suffix = ".bak";
	const char *corrupt_suffix = ".corrupt";
	const char *rotate_suffix = ".old";
	char *tdb_path_backup;
	struct stat st;
	TALLOC_CTX *ctx = NULL;

	ctx = talloc_new(NULL);
	if (ctx == NULL) {
		DEBUG(0, ("tdb_validate_and_backup: out of memory\n"));
		goto done;
	}

	tdb_path_backup = talloc_asprintf(ctx, "%s%s", tdb_path, backup_suffix);
	if (!tdb_path_backup) {
		DEBUG(0, ("tdb_validate_and_backup: out of memory\n"));
		goto done;
	}

	ret = tdb_validate_open(tdb_path, validate_fn);

	if (ret == 0) {
		DEBUG(1, ("tdb '%s' is valid\n", tdb_path));
		ret = tdb_backup_with_rotate(ctx, tdb_path, tdb_path_backup, 0,
					     rotate_suffix, True, False);
		if (ret != 0) {
			DEBUG(1, ("Error creating backup of tdb '%s'\n",
				  tdb_path));
			/* the actual validation was successful: */
			ret = 0;
		} else {
			DEBUG(1, ("Created backup '%s' of tdb '%s'\n",
				  tdb_path_backup, tdb_path));
		}
	} else {
		DEBUG(1, ("tdb '%s' is invalid\n", tdb_path));

		ret =stat(tdb_path_backup, &st);
		if (ret != 0) {
			DEBUG(5, ("Could not stat '%s': %s\n", tdb_path_backup,
				  strerror(errno)));
			DEBUG(1, ("No backup found.\n"));
		} else {
			DEBUG(1, ("backup '%s' found.\n", tdb_path_backup));
			ret = tdb_validate_open(tdb_path_backup, validate_fn);
			if (ret != 0) {
				DEBUG(1, ("Backup '%s' is invalid.\n",
					  tdb_path_backup));
			}
		}

		if (ret != 0) {
			int renamed = rename_file_with_suffix(ctx, tdb_path,
							      corrupt_suffix);
			if (renamed != 0) {
				DEBUG(1, ("Error moving tdb to '%s%s'\n",
					  tdb_path, corrupt_suffix));
			} else {
				DEBUG(1, ("Corrupt tdb stored as '%s%s'\n",
					  tdb_path, corrupt_suffix));
			}
			goto done;
		}

		DEBUG(1, ("valid backup '%s' found\n", tdb_path_backup));
		ret = tdb_backup_with_rotate(ctx, tdb_path_backup, tdb_path, 0,
					     corrupt_suffix, True, True);
		if (ret != 0) {
			DEBUG(1, ("Error restoring backup from '%s'\n",
				  tdb_path_backup));
		} else {
			DEBUG(1, ("Restored tdb backup from '%s'\n",
				  tdb_path_backup));
		}
	}

done:
	TALLOC_FREE(ctx);
	return ret;
}
