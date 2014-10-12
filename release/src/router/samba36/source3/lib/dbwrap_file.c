/* 
   Unix SMB/CIFS implementation.
   Database interface using a file per record
   Copyright (C) Volker Lendecke 2005
   
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

struct db_file_ctx {
	const char *dirname;

	/* We only support one locked record at a time -- everything else
	 * would lead to a potential deadlock anyway! */
	struct db_record *locked_record;
};

struct db_locked_file {
	int fd;
	uint8 hash;
	const char *name;
	const char *path;
	struct db_file_ctx *parent;
};

/* Copy from statcache.c... */

static uint32 fsh(const uint8 *p, int len)
{
        uint32 n = 0;
	int i;
        for (i=0; i<len; i++) {
                n = ((n << 5) + n) ^ (uint32)(p[i]);
        }
        return n;
}

static int db_locked_file_destr(struct db_locked_file *data)
{
	if (data->parent != NULL) {
		data->parent->locked_record = NULL;
	}

	if (close(data->fd) != 0) {
		DEBUG(3, ("close failed: %s\n", strerror(errno)));
		return -1;
	}

	return 0;
}

static NTSTATUS db_file_store(struct db_record *rec, TDB_DATA data, int flag);
static NTSTATUS db_file_delete(struct db_record *rec);

static struct db_record *db_file_fetch_locked(struct db_context *db,
					      TALLOC_CTX *mem_ctx,
					      TDB_DATA key)
{
	struct db_file_ctx *ctx = talloc_get_type_abort(db->private_data,
							struct db_file_ctx);
	struct db_record *result;
	struct db_locked_file *file;
	struct flock fl;
	SMB_STRUCT_STAT statbuf;
	ssize_t nread;
	int ret;

	SMB_ASSERT(ctx->locked_record == NULL);

 again:
	if (!(result = TALLOC_P(mem_ctx, struct db_record))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	if (!(file = TALLOC_P(result, struct db_locked_file))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	result->private_data = file;
	result->store = db_file_store;
	result->delete_rec = db_file_delete;

	result->key.dsize = key.dsize;
	result->key.dptr = (uint8 *)talloc_memdup(result, key.dptr, key.dsize);
	if (result->key.dptr == NULL) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	/* Cut to 8 bits */
	file->hash = fsh(key.dptr, key.dsize);
	file->name = hex_encode_talloc(file, (unsigned char *)key.dptr, key.dsize);
	if (file->name == NULL) {
		DEBUG(0, ("hex_encode failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	file->path = talloc_asprintf(file, "%s/%2.2X/%s", ctx->dirname,
				     file->hash, file->name);
	if (file->path == NULL) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	become_root();
	file->fd = open(file->path, O_RDWR|O_CREAT, 0644);
	unbecome_root();

	if (file->fd < 0) {
		DEBUG(3, ("Could not open/create %s: %s\n",
			  file->path, strerror(errno)));
		TALLOC_FREE(result);
		return NULL;
	}

	talloc_set_destructor(file, db_locked_file_destr);

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	fl.l_pid = 0;

	do {
		ret = fcntl(file->fd, F_SETLKW, &fl);
	} while ((ret == -1) && (errno == EINTR));

	if (ret == -1) {
		DEBUG(3, ("Could not get lock on %s: %s\n",
			  file->path, strerror(errno)));
		TALLOC_FREE(result);
		return NULL;
	}

	if (sys_fstat(file->fd, &statbuf) != 0) {
		DEBUG(3, ("Could not fstat %s: %s\n",
			  file->path, strerror(errno)));
		TALLOC_FREE(result);
		return NULL;
	}

	if (statbuf.st_nlink == 0) {
		/* Someone has deleted it under the lock, retry */
		TALLOC_FREE(result);
		goto again;
	}

	result->value.dsize = 0;
	result->value.dptr = NULL;

	if (statbuf.st_size != 0) {
		result->value.dsize = statbuf.st_size;
		result->value.dptr = TALLOC_ARRAY(result, uint8,
						  statbuf.st_size);
		if (result->value.dptr == NULL) {
			DEBUG(1, ("talloc failed\n"));
			TALLOC_FREE(result);
			return NULL;
		}

		nread = read_data(file->fd, (char *)result->value.dptr,
				  result->value.dsize);
		if (nread != result->value.dsize) {
			DEBUG(3, ("read_data failed: %s\n", strerror(errno)));
			TALLOC_FREE(result);
			return NULL;
		}
	}

	ctx->locked_record = result;
	file->parent = (struct db_file_ctx *)talloc_reference(file, ctx);

	return result;
}

static NTSTATUS db_file_store_root(int fd, TDB_DATA data)
{
	if (sys_lseek(fd, 0, SEEK_SET) != 0) {
		DEBUG(0, ("sys_lseek failed: %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	if (write_data(fd, (char *)data.dptr, data.dsize) != data.dsize) {
		DEBUG(3, ("write_data failed: %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	if (sys_ftruncate(fd, data.dsize) != 0) {
		DEBUG(3, ("sys_ftruncate failed: %s\n", strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	return NT_STATUS_OK;
}

static NTSTATUS db_file_store(struct db_record *rec, TDB_DATA data, int flag)
{
	struct db_locked_file *file =
		talloc_get_type_abort(rec->private_data,
				      struct db_locked_file);
	NTSTATUS status;

	become_root();
	status = db_file_store_root(file->fd, data);
	unbecome_root();

	return status;
}

static NTSTATUS db_file_delete(struct db_record *rec)
{
	struct db_locked_file *file =
		talloc_get_type_abort(rec->private_data,
				      struct db_locked_file);
	int res;

	become_root();
	res = unlink(file->path);
	unbecome_root();

	if (res == -1) {
		DEBUG(3, ("unlink(%s) failed: %s\n", file->path,
			  strerror(errno)));
		return map_nt_error_from_unix(errno);
	}

	return NT_STATUS_OK;
}

static int db_file_traverse(struct db_context *db,
			    int (*fn)(struct db_record *rec,
				      void *private_data),
			    void *private_data)
{
	struct db_file_ctx *ctx = talloc_get_type_abort(db->private_data,
							struct db_file_ctx);
	TALLOC_CTX *mem_ctx = talloc_init("traversal %s\n", ctx->dirname);
	
	int i;
	int count = 0;

	for (i=0; i<256; i++) {
		const char *dirname = talloc_asprintf(mem_ctx, "%s/%2.2X",
						      ctx->dirname, i);
		DIR *dir;
		struct dirent *dirent;

		if (dirname == NULL) {
			DEBUG(0, ("talloc failed\n"));
			TALLOC_FREE(mem_ctx);
			return -1;
		}

		dir = opendir(dirname);
		if (dir == NULL) {
			DEBUG(3, ("Could not open dir %s: %s\n", dirname,
				  strerror(errno)));
			TALLOC_FREE(mem_ctx);
			return -1;
		}

		while ((dirent = readdir(dir)) != NULL) {
			DATA_BLOB keyblob;
			TDB_DATA key;
			struct db_record *rec;

			if ((dirent->d_name[0] == '.') &&
			    ((dirent->d_name[1] == '\0') ||
			     ((dirent->d_name[1] == '.') &&
			      (dirent->d_name[2] == '\0')))) {
				continue;
			}

			keyblob = strhex_to_data_blob(mem_ctx, dirent->d_name);
			if (keyblob.data == NULL) {
				DEBUG(5, ("strhex_to_data_blob failed\n"));
				continue;
			}

			key.dptr = keyblob.data;
			key.dsize = keyblob.length;

			if ((ctx->locked_record != NULL) &&
			    (key.dsize == ctx->locked_record->key.dsize) &&
			    (memcmp(key.dptr, ctx->locked_record->key.dptr,
				    key.dsize) == 0)) {
				count += 1;
				if (fn(ctx->locked_record,
				       private_data) != 0) {
					TALLOC_FREE(mem_ctx);
					closedir(dir);
					return count;
				}
			}

			rec = db_file_fetch_locked(db, mem_ctx, key);
			if (rec == NULL) {
				/* Someone might have deleted it */
				continue;
			}

			if (rec->value.dptr == NULL) {
				TALLOC_FREE(rec);
				continue;
			}

			count += 1;

			if (fn(rec, private_data) != 0) {
				TALLOC_FREE(mem_ctx);
				closedir(dir);
				return count;
			}
			TALLOC_FREE(rec);
		}

		closedir(dir);
	}

	TALLOC_FREE(mem_ctx);
	return count;
}

struct db_context *db_open_file(TALLOC_CTX *mem_ctx,
				struct messaging_context *msg_ctx,
				const char *name,
				int hash_size, int tdb_flags,
				int open_flags, mode_t mode)
{
	struct db_context *result = NULL;
	struct db_file_ctx *ctx;

	if (!(result = TALLOC_ZERO_P(mem_ctx, struct db_context))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	if (!(ctx = TALLOC_P(result, struct db_file_ctx))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	result->private_data = ctx;
	result->fetch_locked = db_file_fetch_locked;
	result->traverse = db_file_traverse;
	result->traverse_read = db_file_traverse;
	result->persistent = ((tdb_flags & TDB_CLEAR_IF_FIRST) == 0);

	ctx->locked_record = NULL;
	if (!(ctx->dirname = talloc_strdup(ctx, name))) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(result);
		return NULL;
	}

	if (open_flags & O_CREAT) {
		int ret, i;

		mode |= (mode & S_IRUSR) ? S_IXUSR : 0;
		mode |= (mode & S_IRGRP) ? S_IXGRP : 0;
		mode |= (mode & S_IROTH) ? S_IXOTH : 0;

		ret = mkdir(name, mode);
		if ((ret != 0) && (errno != EEXIST)) {
			DEBUG(5, ("mkdir(%s,%o) failed: %s\n", name, mode,
				  strerror(errno)));
			TALLOC_FREE(result);
			return NULL;
		}

		for (i=0; i<256; i++) {
			char *path;
			path = talloc_asprintf(result, "%s/%2.2X", name, i);
			if (path == NULL) {
				DEBUG(0, ("asprintf failed\n"));
				TALLOC_FREE(result);
				return NULL;
			}
			ret = mkdir(path, mode);
			if ((ret != 0) && (errno != EEXIST)) {
				DEBUG(5, ("mkdir(%s,%o) failed: %s\n", path,
					  mode, strerror(errno)));
				TALLOC_FREE(result);
				return NULL;
			}
			TALLOC_FREE(path);
		}
	}

	return result;
}
