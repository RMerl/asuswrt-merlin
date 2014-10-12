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

#ifndef __TDB_VALIDATE_H__
#define __TDB_VALIDATE_H__

#include "lib/replace/replace.h"
#include <tdb.h>

/**
 * Flag field for keeping track of the status of a validation.
 */
struct tdb_validation_status {
	bool tdb_error;
	bool bad_freelist;
	bool bad_entry;
	bool unknown_key;
	bool success;
};

/**
 * Callback function type for the validation mechanism.
 */
typedef int (*tdb_validate_data_func)(TDB_CONTEXT *the_tdb, TDB_DATA kbuf,
				      TDB_DATA dbuf, void *state);

/**
 * tdb validation function.
 * returns 0 if tdb is ok, != 0 if it isn't.
 * this function expects an opened tdb.
 */
int tdb_validate(struct tdb_context *tdb,
		 tdb_validate_data_func validate_fn);

/**
 * tdb validation function.
 * returns 0 if tdb is ok, != 0 if it isn't.
 * This is a wrapper around the actual validation function that
 * opens and closes the tdb.
 */
int tdb_validate_open(const char *tdb_path,
		      tdb_validate_data_func validate_fn);

/**
 * validation function with backup handling:
 *
 *  - calls tdb_validate
 *  - if the tdb is ok, create a backup "name.bak", possibly moving
 *    existing backup to name.bak.old,
 *    return 0 (success) even if the backup fails
 *  - if the tdb is corrupt:
 *    - move the tdb to "name.corrupt"
 *    - check if there is valid backup.
 *      if so, restore the backup.
 *      if restore is successful, return 0 (success),
 *    - otherwise return -1 (failure)
 */
int tdb_validate_and_backup(const char *tdb_path,
			    tdb_validate_data_func validate_fn);

#endif /* __TDB_VALIDATE_H__ */
