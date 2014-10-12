/*
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - pvfs_sys wrappers

   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett 2010

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
#include "vfs_posix.h"
#include "../lib/util/unix_privs.h"

/*
  these wrapper functions must only be called when the appropriate ACL
  has already been checked. The wrappers will override a EACCES result
  by gaining root privileges if the 'pvfs:perm override' is set on the
  share (it is enabled by default)

  Careful use of O_NOFOLLOW and O_DIRECTORY is used to prevent
  security attacks via symlinks
 */


struct pvfs_sys_ctx {
	struct pvfs_state *pvfs;
	void *privs;
	const char *old_wd;
	struct stat st_orig;
};


/*
  we create PVFS_NOFOLLOW and PVFS_DIRECTORY as aliases for O_NOFOLLOW
  and O_DIRECTORY on systems that have them. On systems that don't
  have O_NOFOLLOW we are less safe, but the root override code is off
  by default.
 */
#ifdef O_NOFOLLOW
#define PVFS_NOFOLLOW O_NOFOLLOW
#else
#define PVFS_NOFOLLOW 0
#endif
#ifdef O_DIRECTORY
#define PVFS_DIRECTORY O_DIRECTORY
#else
#define PVFS_DIRECTORY 0
#endif

/*
  return to original directory when context is destroyed
 */
static int pvfs_sys_pushdir_destructor(struct pvfs_sys_ctx *ctx)
{
	struct stat st;

	if (ctx->old_wd == NULL) {
		return 0;
	}

	if (chdir(ctx->old_wd) != 0) {
		smb_panic("Failed to restore working directory");
	}
	if (stat(".", &st) != 0) {
		smb_panic("Failed to stat working directory");
	}
	if (st.st_ino != ctx->st_orig.st_ino ||
	    st.st_dev != ctx->st_orig.st_dev) {
		smb_panic("Working directory changed during call");
	}

	return 0;
}


/*
  chdir() to the directory part of a pathname, but disallow any
  component with a symlink

  Note that we can't use O_NOFOLLOW on the whole path as that only
  prevents links in the final component of the path
 */
static int pvfs_sys_chdir_nosymlink(struct pvfs_sys_ctx *ctx, const char *pathname)
{
	char *p, *path;
	size_t base_len = strlen(ctx->pvfs->base_directory);

	/* don't check for symlinks in the base directory of the share */
	if (strncmp(ctx->pvfs->base_directory, pathname, base_len) == 0 &&
	    pathname[base_len] == '/') {
		if (chdir(ctx->pvfs->base_directory) != 0) {
			return -1;
		}
		pathname += base_len + 1;
	}

	path = talloc_strdup(ctx, pathname);
	if (path == NULL) {
		return -1;
	}
	while ((p = strchr(path, '/'))) {
		int fd;
		struct stat st1, st2;
		*p = 0;
		fd = open(path, PVFS_NOFOLLOW | PVFS_DIRECTORY | O_RDONLY);
		if (fd == -1) {
			return -1;
		}
		if (chdir(path) != 0) {
			close(fd);
			return -1;
		}
		if (stat(".", &st1) != 0 ||
		    fstat(fd, &st2) != 0) {
			close(fd);
			return -1;
		}
		close(fd);
		if (st1.st_ino != st2.st_ino ||
		    st1.st_dev != st2.st_dev) {
			DEBUG(0,(__location__ ": Inode changed during chdir in '%s' - symlink attack?",
				 pathname));
			return -1;
		}
		path = p + 1;
	}

	return 0;
}


/*
  become root, and change directory to the directory component of a
  path. Return a talloc context which when freed will move us back
  to the original directory, and return us to the original uid

  change the pathname argument to contain just the base component of
  the path

  return NULL on error, which could include an attempt to subvert
  security using symlink tricks
 */
static struct pvfs_sys_ctx *pvfs_sys_pushdir(struct pvfs_state *pvfs,
					     const char **pathname)
{
	struct pvfs_sys_ctx *ctx;
	char *cwd, *p, *dirname;
	int ret;

	ctx = talloc_zero(pvfs, struct pvfs_sys_ctx);
	if (ctx == NULL) {
		return NULL;
	}
	ctx->pvfs = pvfs;
	ctx->privs = root_privileges();
	if (ctx->privs == NULL) {
		talloc_free(ctx);
		return NULL;
	}

	talloc_steal(ctx, ctx->privs);

	if (!pathname) {
		/* no pathname needed */
		return ctx;
	}

	p = strrchr(*pathname, '/');
	if (p == NULL) {
		/* we don't need to change directory */
		return ctx;
	}

	/* we keep the old st around, so we can tell that
	   we have come back to the right directory */
	if (stat(".", &ctx->st_orig) != 0) {
		talloc_free(ctx);
		return NULL;
	}

	cwd = get_current_dir_name();
	if (cwd == NULL) {
		talloc_free(ctx);
		return NULL;
	}
	ctx->old_wd = talloc_strdup(ctx, cwd);
	if (ctx->old_wd == NULL) {
		free(cwd);
		talloc_free(ctx);
		return NULL;
	}

	dirname = talloc_strndup(ctx, *pathname, (p - *pathname));
	if (dirname == NULL) {
		talloc_free(ctx);
		return NULL;
	}

	ret = pvfs_sys_chdir_nosymlink(ctx, *pathname);
	if (ret == -1) {
		talloc_free(ctx);
		return NULL;
	}

	talloc_set_destructor(ctx, pvfs_sys_pushdir_destructor);

	/* return the basename as the filename that should be operated on */
	(*pathname) = talloc_strdup(ctx, p+1);
	if (! *pathname) {
		talloc_free(ctx);
		return NULL;
	}

	return ctx;
}


/*
  chown a file that we created with a root privileges override
 */
static int pvfs_sys_fchown(struct pvfs_state *pvfs, struct pvfs_sys_ctx *ctx, int fd)
{
	return fchown(fd, root_privileges_original_uid(ctx->privs), -1);
}

/*
  chown a directory that we created with a root privileges override
 */
static int pvfs_sys_chown(struct pvfs_state *pvfs, struct pvfs_sys_ctx *ctx, const char *name)
{
	/* to avoid symlink hacks, we need to use fchown() on a directory fd */
	int ret, fd;
	fd = open(name, PVFS_DIRECTORY | PVFS_NOFOLLOW | O_RDONLY);
	if (fd == -1) {
		return -1;
	}
	ret = pvfs_sys_fchown(pvfs, ctx, fd);
	close(fd);
	return ret;
}


/*
  wrap open for system override
*/
int pvfs_sys_open(struct pvfs_state *pvfs, const char *filename, int flags, mode_t mode)
{
	int fd, ret;
	struct pvfs_sys_ctx *ctx;
	int saved_errno, orig_errno;
	int retries = 5;

	orig_errno = errno;

	fd = open(filename, flags, mode);
	if (fd != -1 ||
	    !(pvfs->flags & PVFS_FLAG_PERM_OVERRIDE) ||
	    errno != EACCES) {
		return fd;
	}

	saved_errno = errno;
	ctx = pvfs_sys_pushdir(pvfs, &filename);
	if (ctx == NULL) {
		errno = saved_errno;
		return -1;
	}

	/* don't allow permission overrides to follow links */
	flags |= PVFS_NOFOLLOW;

	/*
	   if O_CREAT was specified and O_EXCL was not specified
	   then initially do the open without O_CREAT, as in that case
	   we know that we did not create the file, so we don't have
	   to fchown it
	 */
	if ((flags & O_CREAT) && !(flags & O_EXCL)) {
	try_again:
		fd = open(filename, flags & ~O_CREAT, mode);
		/* if this open succeeded, or if it failed
		   with anything other than ENOENT, then we return the
		   open result, with the original errno */
		if (fd == -1 && errno != ENOENT) {
			talloc_free(ctx);
			errno = saved_errno;
			return -1;
		}
		if (fd != -1) {
			/* the file already existed and we opened it */
			talloc_free(ctx);
			errno = orig_errno;
			return fd;
		}

		fd = open(filename, flags | O_EXCL, mode);
		if (fd == -1 && errno != EEXIST) {
			talloc_free(ctx);
			errno = saved_errno;
			return -1;
		}
		if (fd != -1) {
			/* we created the file, we need to set the
			   right ownership on it */
			ret = pvfs_sys_fchown(pvfs, ctx, fd);
			if (ret == -1) {
				close(fd);
				unlink(filename);
				talloc_free(ctx);
				errno = saved_errno;
				return -1;
			}
			talloc_free(ctx);
			errno = orig_errno;
			return fd;
		}

		/* the file got created between the two times
		   we tried to open it! Try again */
		if (retries-- > 0) {
			goto try_again;
		}

		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	fd = open(filename, flags, mode);
	if (fd == -1) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	/* if we have created a file then fchown it */
	if (flags & O_CREAT) {
		ret = pvfs_sys_fchown(pvfs, ctx, fd);
		if (ret == -1) {
			close(fd);
			unlink(filename);
			talloc_free(ctx);
			errno = saved_errno;
			return -1;
		}
	}

	talloc_free(ctx);
	return fd;
}


/*
  wrap unlink for system override
*/
int pvfs_sys_unlink(struct pvfs_state *pvfs, const char *filename)
{
	int ret;
	struct pvfs_sys_ctx *ctx;
	int saved_errno, orig_errno;

	orig_errno = errno;

	ret = unlink(filename);
	if (ret != -1 ||
	    !(pvfs->flags & PVFS_FLAG_PERM_OVERRIDE) ||
	    errno != EACCES) {
		return ret;
	}

	saved_errno = errno;

	ctx = pvfs_sys_pushdir(pvfs, &filename);
	if (ctx == NULL) {
		errno = saved_errno;
		return -1;
	}

	ret = unlink(filename);
	if (ret == -1) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	talloc_free(ctx);
	errno = orig_errno;
	return ret;
}


static bool contains_symlink(const char *path)
{
	int fd = open(path, PVFS_NOFOLLOW | O_RDONLY);
	if (fd != -1) {
		close(fd);
		return false;
	}
	return (errno == ELOOP);
}

/*
  wrap rename for system override
*/
int pvfs_sys_rename(struct pvfs_state *pvfs, const char *name1, const char *name2)
{
	int ret;
	struct pvfs_sys_ctx *ctx;
	int saved_errno, orig_errno;

	orig_errno = errno;

	ret = rename(name1, name2);
	if (ret != -1 ||
	    !(pvfs->flags & PVFS_FLAG_PERM_OVERRIDE) ||
	    errno != EACCES) {
		return ret;
	}

	saved_errno = errno;

	ctx = pvfs_sys_pushdir(pvfs, &name1);
	if (ctx == NULL) {
		errno = saved_errno;
		return -1;
	}

	/* we need the destination as an absolute path */
	if (name2[0] != '/') {
		name2 = talloc_asprintf(ctx, "%s/%s", ctx->old_wd, name2);
		if (name2 == NULL) {
			talloc_free(ctx);
			errno = saved_errno;
			return -1;
		}
	}

	/* make sure the destination isn't a symlink beforehand */
	if (contains_symlink(name2)) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	ret = rename(name1, name2);
	if (ret == -1) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	/* make sure the destination isn't a symlink afterwards */
	if (contains_symlink(name2)) {
		DEBUG(0,(__location__ ": Possible symlink attack in rename to '%s' - unlinking\n", name2));
		unlink(name2);
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	talloc_free(ctx);
	errno = orig_errno;
	return ret;
}


/*
  wrap mkdir for system override
*/
int pvfs_sys_mkdir(struct pvfs_state *pvfs, const char *dirname, mode_t mode)
{
	int ret;
	struct pvfs_sys_ctx *ctx;
	int saved_errno, orig_errno;

	orig_errno = errno;

	ret = mkdir(dirname, mode);
	if (ret != -1 ||
	    !(pvfs->flags & PVFS_FLAG_PERM_OVERRIDE) ||
	    errno != EACCES) {
		return ret;
	}

	saved_errno = errno;
	ctx = pvfs_sys_pushdir(pvfs, &dirname);
	if (ctx == NULL) {
		errno = saved_errno;
		return -1;
	}

	ret = mkdir(dirname, mode);
	if (ret == -1) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	ret = pvfs_sys_chown(pvfs, ctx, dirname);
	if (ret == -1) {
		rmdir(dirname);
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	talloc_free(ctx);
	return ret;
}


/*
  wrap rmdir for system override
*/
int pvfs_sys_rmdir(struct pvfs_state *pvfs, const char *dirname)
{
	int ret;
	struct pvfs_sys_ctx *ctx;
	int saved_errno, orig_errno;

	orig_errno = errno;

	ret = rmdir(dirname);
	if (ret != -1 ||
	    !(pvfs->flags & PVFS_FLAG_PERM_OVERRIDE) ||
	    errno != EACCES) {
		return ret;
	}

	saved_errno = errno;

	ctx = pvfs_sys_pushdir(pvfs, &dirname);
	if (ctx == NULL) {
		errno = saved_errno;
		return -1;
	}

	ret = rmdir(dirname);
	if (ret == -1) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	talloc_free(ctx);
	errno = orig_errno;
	return ret;
}

/*
  wrap fchmod for system override
*/
int pvfs_sys_fchmod(struct pvfs_state *pvfs, int fd, mode_t mode)
{
	int ret;
	struct pvfs_sys_ctx *ctx;
	int saved_errno, orig_errno;

	orig_errno = errno;

	ret = fchmod(fd, mode);
	if (ret != -1 ||
	    !(pvfs->flags & PVFS_FLAG_PERM_OVERRIDE) ||
	    errno != EACCES) {
		return ret;
	}

	saved_errno = errno;

	ctx = pvfs_sys_pushdir(pvfs, NULL);
	if (ctx == NULL) {
		errno = saved_errno;
		return -1;
	}

	ret = fchmod(fd, mode);
	if (ret == -1) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	talloc_free(ctx);
	errno = orig_errno;
	return ret;
}


/*
  wrap chmod for system override
*/
int pvfs_sys_chmod(struct pvfs_state *pvfs, const char *filename, mode_t mode)
{
	int ret;
	struct pvfs_sys_ctx *ctx;
	int saved_errno, orig_errno;

	orig_errno = errno;

	ret = chmod(filename, mode);
	if (ret != -1 ||
	    !(pvfs->flags & PVFS_FLAG_PERM_OVERRIDE) ||
	    errno != EACCES) {
		return ret;
	}

	saved_errno = errno;

	ctx = pvfs_sys_pushdir(pvfs, &filename);
	if (ctx == NULL) {
		errno = saved_errno;
		return -1;
	}

	ret = chmod(filename, mode);
	if (ret == -1) {
		talloc_free(ctx);
		errno = saved_errno;
		return -1;
	}

	talloc_free(ctx);
	errno = orig_errno;
	return ret;
}
