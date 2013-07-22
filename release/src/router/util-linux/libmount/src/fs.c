/*
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: fs
 * @title: Filesystem
 * @short_description: represents one entry from fstab, mtab, or mountinfo file
 *
 */
#include <ctype.h>
#include <blkid.h>
#include <stddef.h>

#include "mountP.h"

/**
 * mnt_new_fs:
 *
 * Returns: newly allocated struct libmnt_fs.
 */
struct libmnt_fs *mnt_new_fs(void)
{
	struct libmnt_fs *fs = calloc(1, sizeof(*fs));
	if (!fs)
		return NULL;

	/*DBG(FS, mnt_debug_h(fs, "alloc"));*/
	INIT_LIST_HEAD(&fs->ents);
	return fs;
}

/**
 * mnt_free_fs:
 * @fs: fs pointer
 *
 * Deallocates the fs.
 */
void mnt_free_fs(struct libmnt_fs *fs)
{
	if (!fs)
		return;
	list_del(&fs->ents);

	/*DBG(FS, mnt_debug_h(fs, "free"));*/

	free(fs->source);
	free(fs->bindsrc);
	free(fs->tagname);
	free(fs->tagval);
	free(fs->root);
	free(fs->target);
	free(fs->fstype);
	free(fs->optstr);
	free(fs->vfs_optstr);
	free(fs->fs_optstr);
	free(fs->user_optstr);
	free(fs->attrs);

	free(fs);
}

/**
 * mnt_reset_fs:
 * @fs: fs pointer
 *
 * Resets (zeroize) @fs.
 */
void mnt_reset_fs(struct libmnt_fs *fs)
{
	if (fs)
		memset(fs, 0, sizeof(*fs));
}

static inline int update_str(char **dest, const char *src)
{
	size_t sz;
	char *x;

	assert(dest);

	if (!src) {
		free(*dest);
		*dest = NULL;
		return 0;	/* source (old) is empty */
	}

	sz = strlen(src) + 1;
	x = realloc(*dest, sz);
	if (!x)
		return -ENOMEM;
	*dest = x;
	memcpy(*dest, src, sz);
	return 0;
}

static inline int cpy_str_at_offset(void *new, const void *old, size_t offset)
{
	char **o = (char **) (old + offset);
	char **n = (char **) (new + offset);

	if (*n)
		return 0;	/* already set, not overwrite */

	return update_str(n, *o);
}

/**
 * mnt_copy_fs:
 * @dest: destination FS
 * @src: source FS
 *
 * If @dest is NULL, then a new FS is allocated, if any @dest field is already
 * set then the field is NOT overwrited.
 *
 * This function does not copy userdata (se mnt_fs_set_userdata()). A new copy is
 * not linked with any existing mnt_tab.
 *
 * Returns: @dest or NULL in case of error
 */
struct libmnt_fs *mnt_copy_fs(struct libmnt_fs *dest,
			      const struct libmnt_fs *src)
{
	const struct libmnt_fs *org = dest;

	if (!dest) {
		dest = mnt_new_fs();
		if (!dest)
			return NULL;
	}

	/*DBG(FS, mnt_debug_h(dest, "copy from %p", src));*/

	dest->id         = src->id;
	dest->parent     = src->parent;
	dest->devno      = src->devno;

	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, source)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, tagname)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, tagval)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, root)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, target)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, fstype)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, vfs_optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, fs_optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, user_optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, attrs)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, bindsrc)))
		goto err;

	dest->freq       = src->freq;
	dest->passno     = src->passno;
	dest->flags      = src->flags;

	return dest;
err:
	if (!org)
		mnt_free_fs(dest);
	return NULL;
}

/*
 * This function copies all @fs description except information that does not
 * belong to /etc/mtab (e.g. VFS and userspace mount options with MNT_NOMTAB
 * mask).
 *
 * Returns: copy of @fs.
 */
struct libmnt_fs *mnt_copy_mtab_fs(const struct libmnt_fs *fs)
{
	struct libmnt_fs *n = mnt_new_fs();

	if (!n)
		return NULL;

	if (cpy_str_at_offset(n, fs, offsetof(struct libmnt_fs, source)))
		goto err;
	if (cpy_str_at_offset(n, fs, offsetof(struct libmnt_fs, target)))
		goto err;
	if (cpy_str_at_offset(n, fs, offsetof(struct libmnt_fs, fstype)))
		goto err;

	if (fs->vfs_optstr) {
		char *p = NULL;
		mnt_optstr_get_options(fs->vfs_optstr, &p,
				mnt_get_builtin_optmap(MNT_LINUX_MAP),
				MNT_NOMTAB);
		n->vfs_optstr = p;
	}

	if (fs->user_optstr) {
		char *p = NULL;
		mnt_optstr_get_options(fs->user_optstr, &p,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP),
				MNT_NOMTAB);
		n->user_optstr = p;
	}

	if (cpy_str_at_offset(n, fs, offsetof(struct libmnt_fs, fs_optstr)))
		goto err;

	/* we cannot copy original optstr, the new optstr has to be without
	 * non-mtab options -- so, let's generate a new string */
	n->optstr = mnt_fs_strdup_options(n);

	n->freq       = fs->freq;
	n->passno     = fs->passno;
	n->flags      = fs->flags;

	return n;
err:
	mnt_free_fs(n);
	return NULL;

}

/**
 * mnt_fs_get_userdata:
 * @fs: struct libmnt_file instance
 *
 * Returns: private data set by mnt_fs_set_userdata() or NULL.
 */
void *mnt_fs_get_userdata(struct libmnt_fs *fs)
{
	return fs ? fs->userdata : NULL;
}

/**
 * mnt_fs_set_userdata:
 * @fs: struct libmnt_file instance
 * @data: user data
 *
 * The "userdata" are library independent data.
 *
 * Returns: 0 or negative number in case of error (if @fs is NULL).
 */
int mnt_fs_set_userdata(struct libmnt_fs *fs, void *data)
{
	if (!fs)
		return -EINVAL;
	fs->userdata = data;
	return 0;
}

/**
 * mnt_fs_get_srcpath:
 * @fs: struct libmnt_file (fstab/mtab/mountinfo) fs
 *
 * The mount "source path" is:
 * - a directory for 'bind' mounts (in fstab or mtab only)
 * - a device name for standard mounts
 *
 * See also mnt_fs_get_tag() and mnt_fs_get_source().
 *
 * Returns: mount source path or NULL in case of error or when the path
 * is not defined.
 */
const char *mnt_fs_get_srcpath(struct libmnt_fs *fs)
{
	assert(fs);
	if (!fs)
		return NULL;

	/* fstab-like fs */
	if (fs->tagname)
		return NULL;	/* the source contains a "NAME=value" */
	return fs->source;
}

/**
 * mnt_fs_get_source:
 * @fs: struct libmnt_file (fstab/mtab/mountinfo) fs
 *
 * Returns: mount source. Note that the source could be unparsed TAG
 * (LABEL/UUID). See also mnt_fs_get_srcpath() and mnt_fs_get_tag().
 */
const char *mnt_fs_get_source(struct libmnt_fs *fs)
{
	return fs ? fs->source : NULL;
}

/*
 * Used by parser ONLY (@source has to be allocated on error)
 */
int __mnt_fs_set_source_ptr(struct libmnt_fs *fs, char *source)
{
	char *t = NULL, *v = NULL;

	assert(fs);

	if (source && !strcmp(source, "none")) {
		free(source);
		source = NULL;

	} else if (source && strchr(source, '=')) {
		if (blkid_parse_tag_string(source, &t, &v) != 0)
			return -1;
	}

	if (fs->source != source)
		free(fs->source);

	free(fs->tagname);
	free(fs->tagval);

	fs->source = source;
	fs->tagname = t;
	fs->tagval = v;
	return 0;
}

/**
 * mnt_fs_set_source:
 * @fs: fstab/mtab/mountinfo entry
 * @source: new source
 *
 * This function creates a private copy (strdup()) of @source.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_source(struct libmnt_fs *fs, const char *source)
{
	char *p = NULL;
	int rc;

	if (!fs)
		return -EINVAL;
	if (source) {
		p = strdup(source);
		if (!p)
			return -ENOMEM;
	}

	rc = __mnt_fs_set_source_ptr(fs, p);
	if (rc)
		free(p);
	return rc;
}

/**
 * mnt_fs_get_tag:
 * @fs: fs
 * @name: returns pointer to NAME string
 * @value: returns pointer to VALUE string
 *
 * "TAG" is NAME=VALUE (e.g. LABEL=foo)
 *
 * The TAG is the first column in the fstab file. The TAG or "srcpath" has to
 * be always set for all entries.
 *
 * See also mnt_fs_get_source().
 *
 * <informalexample>
 *   <programlisting>
 *	char *src;
 *	struct libmnt_fs *fs = mnt_table_find_target(tb, "/home", MNT_ITER_FORWARD);
 *
 *	if (!fs)
 *		goto err;
 *
 *	src = mnt_fs_get_srcpath(fs);
 *	if (!src) {
 *		char *tag, *val;
 *		if (mnt_fs_get_tag(fs, &tag, &val) == 0)
 *			printf("%s: %s\n", tag, val);	// LABEL or UUID
 *	} else
 *		printf("device: %s\n", src);		// device or bind path
 *   </programlisting>
 * </informalexample>
 *
 * Returns: 0 on success or negative number in case that a TAG is not defined.
 */
int mnt_fs_get_tag(struct libmnt_fs *fs, const char **name, const char **value)
{
	if (fs == NULL || !fs->tagname)
		return -EINVAL;
	if (name)
		*name = fs->tagname;
	if (value)
		*value = fs->tagval;
	return 0;
}

/**
 * mnt_fs_get_target:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to mountpoint path or NULL
 */
const char *mnt_fs_get_target(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->target : NULL;
}

/**
 * mnt_fs_set_target:
 * @fs: fstab/mtab/mountinfo entry
 * @target: mountpoint
 *
 * This function creates a private copy (strdup()) of @target.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_target(struct libmnt_fs *fs, const char *target)
{
	char *p = NULL;

	assert(fs);

	if (!fs)
		return -EINVAL;
	if (target) {
		p = strdup(target);
		if (!p)
			return -ENOMEM;
	}
	free(fs->target);
	fs->target = p;

	return 0;
}

int __mnt_fs_get_flags(struct libmnt_fs *fs)
{
	return fs ? fs->flags : 0;
}

int __mnt_fs_set_flags(struct libmnt_fs *fs, int flags)
{
	if (fs) {
		fs->flags = flags;
		return 0;
	}
	return -EINVAL;
}

/**
 * mnt_fs_is_kernel:
 * @fs: filesystem
 *
 * Returns: 1 if the filesystem description is read from kernel e.g. /proc/mounts.
 */
int mnt_fs_is_kernel(struct libmnt_fs *fs)
{
	return __mnt_fs_get_flags(fs) & MNT_FS_KERNEL;
}

/**
 * mnt_fs_get_fstype:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to filesystem type.
 */
const char *mnt_fs_get_fstype(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->fstype : NULL;
}

/* Used by struct libmnt_file parser only */
int __mnt_fs_set_fstype_ptr(struct libmnt_fs *fs, char *fstype)
{
	assert(fs);

	if (fstype != fs->fstype)
		free(fs->fstype);

	fs->fstype = fstype;
	fs->flags &= ~MNT_FS_PSEUDO;
	fs->flags &= ~MNT_FS_NET;
	fs->flags &= ~MNT_FS_SWAP;

	/* save info about pseudo filesystems */
	if (fs->fstype) {
		if (mnt_fstype_is_pseudofs(fs->fstype))
			fs->flags |= MNT_FS_PSEUDO;
		else if (mnt_fstype_is_netfs(fs->fstype))
			fs->flags |= MNT_FS_NET;
		else if (!strcmp(fs->fstype, "swap"))
			fs->flags |= MNT_FS_SWAP;
	}
	return 0;
}

/**
 * mnt_fs_set_fstype:
 * @fs: fstab/mtab/mountinfo entry
 * @fstype: filesystem type
 *
 * This function creates a private copy (strdup()) of @fstype.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_fstype(struct libmnt_fs *fs, const char *fstype)
{
	char *p = NULL;

	if (!fs)
		return -EINVAL;
	if (fstype) {
		p = strdup(fstype);
		if (!p)
			return -ENOMEM;
	}
	return  __mnt_fs_set_fstype_ptr(fs, p);
}

/*
 * Merges @vfs and @fs options strings into a new string.
 * This function cares about 'ro/rw' options. The 'ro' is
 * always used if @vfs or @fs is read-only.
 * For example:
 *
 *    mnt_merge_optstr("rw,noexec", "ro,journal=update")
 *
 *           returns: "ro,noexec,journal=update"
 *
 *    mnt_merge_optstr("rw,noexec", "rw,journal=update")
 *
 *           returns: "rw,noexec,journal=update"
 */
static char *merge_optstr(const char *vfs, const char *fs)
{
	char *res, *p;
	size_t sz;
	int ro = 0, rw = 0;

	if (!vfs && !fs)
		return NULL;
	if (!vfs || !fs)
		return strdup(fs ? fs : vfs);
	if (!strcmp(vfs, fs))
		return strdup(vfs);		/* e.g. "aaa" and "aaa" */

	/* leave space for leading "r[ow],", "," and trailing zero */
	sz = strlen(vfs) + strlen(fs) + 5;
	res = malloc(sz);
	if (!res)
		return NULL;
	p = res + 3;			/* make a room for rw/ro flag */

	snprintf(p, sz - 3, "%s,%s", vfs, fs);

	/* remove 'rw' flags */
	rw += !mnt_optstr_remove_option(&p, "rw");	/* from vfs */
	rw += !mnt_optstr_remove_option(&p, "rw");	/* from fs */

	/* remove 'ro' flags if necessary */
	if (rw != 2) {
		ro += !mnt_optstr_remove_option(&p, "ro");
		if (ro + rw < 2)
			ro += !mnt_optstr_remove_option(&p, "ro");
	}

	if (!strlen(p))
		memcpy(res, ro ? "ro" : "rw", 3);
	else
		memcpy(res, ro ? "ro," : "rw,", 3);
	return res;
}

/**
 * mnt_fs_strdup_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Merges all mount options (VFS, FS and userspace) to the one options string
 * and returns the result. This function does not modigy @fs.
 *
 * Returns: pointer to string (can be freed by free(3)) or NULL in case of error.
 */
char *mnt_fs_strdup_options(struct libmnt_fs *fs)
{
	char *res;

	assert(fs);

	errno = 0;

	if (fs->optstr)
		return strdup(fs->optstr);

	res = merge_optstr(fs->vfs_optstr, fs->fs_optstr);
	if (!res && errno)
		return NULL;
	if (fs->user_optstr) {
		if (mnt_optstr_append_option(&res, fs->user_optstr, NULL)) {
			free(res);
			res = NULL;
		}
	}
	return res;
}

/**
 * mnt_fs_get_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to string or NULL in case of error.
 */
const char *mnt_fs_get_options(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->optstr : NULL;
}


/**
 * mnt_fs_set_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 * @optstr: options string
 *
 * Splits @optstr to VFS, FS and userspace mount options and update relevat
 * parts of @fs.
 *
 * Returns: 0 on success, or negative number icase of error.
 */
int mnt_fs_set_options(struct libmnt_fs *fs, const char *optstr)
{
	char *v = NULL, *f = NULL, *u = NULL, *n = NULL;

	assert(fs);

	if (!fs)
		return -EINVAL;
	if (optstr) {
		int rc = mnt_split_optstr(optstr, &u, &v, &f, 0, 0);
		if (rc)
			return rc;
		n = strdup(optstr);
		if (!n)
			return -ENOMEM;
	}

	free(fs->fs_optstr);
	free(fs->vfs_optstr);
	free(fs->user_optstr);
	free(fs->optstr);

	fs->fs_optstr = f;
	fs->vfs_optstr = v;
	fs->user_optstr = u;
	fs->optstr = n;

	return 0;
}

/**
 * mnt_fs_append_options:
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: mount options
 *
 * Parses (splits) @optstr and appends results to VFS, FS and userspace lists
 * of options.
 *
 * If @optstr is NULL then @fs is not modified and 0 is returned.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_append_options(struct libmnt_fs *fs, const char *optstr)
{
	char *v = NULL, *f = NULL, *u = NULL;
	int rc;

	assert(fs);

	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;

	rc = mnt_split_optstr((char *) optstr, &u, &v, &f, 0, 0);
	if (!rc && v)
		rc = mnt_optstr_append_option(&fs->vfs_optstr, v, NULL);
	if (!rc && f)
		rc = mnt_optstr_append_option(&fs->fs_optstr, f, NULL);
	if (!rc && u)
		rc = mnt_optstr_append_option(&fs->user_optstr, u, NULL);
	if (!rc)
		rc = mnt_optstr_append_option(&fs->optstr, optstr, NULL);

	free(v);
	free(f);
	free(u);

	return rc;
}

/**
 * mnt_fs_prepend_options:
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: mount options
 *
 * Parses (splits) @optstr and prepands results to VFS, FS and userspace lists
 * of options.
 *
 * If @optstr is NULL then @fs is not modified and 0 is returned.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_prepend_options(struct libmnt_fs *fs, const char *optstr)
{
	char *v = NULL, *f = NULL, *u = NULL;
	int rc;

	assert(fs);

	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;

	rc = mnt_split_optstr((char *) optstr, &u, &v, &f, 0, 0);
	if (!rc && v)
		rc = mnt_optstr_prepend_option(&fs->vfs_optstr, v, NULL);
	if (!rc && f)
		rc = mnt_optstr_prepend_option(&fs->fs_optstr, f, NULL);
	if (!rc && u)
		rc = mnt_optstr_prepend_option(&fs->user_optstr, u, NULL);
	if (!rc)
		rc = mnt_optstr_prepend_option(&fs->optstr, optstr, NULL);

	free(v);
	free(f);
	free(u);

	return rc;
}

/*
 * mnt_fs_get_fs_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to superblock (fs-depend) mount option string or NULL.
 */
const char *mnt_fs_get_fs_options(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->fs_optstr : NULL;
}

/**
 * mnt_fs_get_vfs_options:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: pointer to fs-independent (VFS) mount option string or NULL.
 */
const char *mnt_fs_get_vfs_options(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->vfs_optstr : NULL;
}

/**
 * mnt_fs_get_user_options:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: pointer to userspace mount option string or NULL.
 */
const char *mnt_fs_get_user_options(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->user_optstr : NULL;
}

/**
 * mnt_fs_get_attributes:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: pointer to attributes string or NULL.
 */
const char *mnt_fs_get_attributes(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->attrs : NULL;
}

/**
 * mnt_fs_set_attributes:
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: options string
 *
 * Sets mount attributes. The attributes are mount(2) and mount(8) independent
 * options, these options are not send to kernel and are not interpreted by
 * libmount. The attributes are stored in /run/mount/utab only.
 *
 * The atrtributes are managed by libmount in userspace only. It's possible
 * that information stored in userspace will not be available for libmount
 * after CLONE_FS unshare. Be carefull, and don't use attributes if possible.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_attributes(struct libmnt_fs *fs, const char *optstr)
{
	char *p = NULL;

	if (!fs)
		return -EINVAL;
	if (optstr) {
		p = strdup(optstr);
		if (!p)
			return -ENOMEM;
	}
	free(fs->attrs);
	fs->attrs = p;

	return 0;
}

/**
 * mnt_fs_append_attributes
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: options string
 *
 * Appends mount attributes. (See mnt_fs_set_attributes()).
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_append_attributes(struct libmnt_fs *fs, const char *optstr)
{
	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;
	return mnt_optstr_append_option(&fs->attrs, optstr, NULL);
}

/**
 * mnt_fs_prepend_attributes
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: options string
 *
 * Prepends mount attributes. (See mnt_fs_set_attributes()).
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_prepend_attributes(struct libmnt_fs *fs, const char *optstr)
{
	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;
	return mnt_optstr_prepend_option(&fs->attrs, optstr, NULL);
}


/**
 * mnt_fs_get_freq:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: dump frequency in days.
 */
int mnt_fs_get_freq(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->freq : 0;
}

/**
 * mnt_fs_set_freq:
 * @fs: fstab/mtab entry pointer
 * @freq: dump frequency in days
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_freq(struct libmnt_fs *fs, int freq)
{
	assert(fs);
	if (!fs)
		return -EINVAL;
	fs->freq = freq;
	return 0;
}

/**
 * mnt_fs_get_passno:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: "pass number on parallel fsck".
 */
int mnt_fs_get_passno(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->passno: 0;
}

/**
 * mnt_fs_set_passno:
 * @fs: fstab/mtab entry pointer
 * @passno: pass number
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_passno(struct libmnt_fs *fs, int passno)
{
	assert(fs);
	if (!fs)
		return -EINVAL;
	fs->passno = passno;
	return 0;
}

/**
 * mnt_fs_get_root:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: root of the mount within the filesystem or NULL
 */
const char *mnt_fs_get_root(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->root : NULL;
}

/**
 * mnt_fs_set_root:
 * @fs: mountinfo entry
 * @root: path
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_root(struct libmnt_fs *fs, const char *root)
{
	char *p = NULL;

	assert(fs);
	if (!fs)
		return -EINVAL;
	if (root) {
		p = strdup(root);
		if (!p)
			return -ENOMEM;
	}
	free(fs->root);
	fs->root = p;
	return 0;
}

/**
 * mnt_fs_get_bindsrc:
 * @fs: /run/mount/utab entry
 *
 * Returns: full path that was used for mount(2) on MS_BIND
 */
const char *mnt_fs_get_bindsrc(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->bindsrc : NULL;
}

/**
 * mnt_fs_set_bindsrc:
 * @fs: filesystem
 * @src: path
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_bindsrc(struct libmnt_fs *fs, const char *src)
{
	char *p = NULL;

	assert(fs);
	if (!fs)
		return -EINVAL;
	if (src) {
		p = strdup(src);
		if (!p)
			return -ENOMEM;
	}
	free(fs->bindsrc);
	fs->bindsrc = p;
	return 0;
}

/**
 * mnt_fs_get_id:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: mount ID (unique identifier of the mount) or negative number in case of error.
 */
int mnt_fs_get_id(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->id : -EINVAL;
}

/**
 * mnt_fs_get_parent_id:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: parent mount ID or negative number in case of error.
 */
int mnt_fs_get_parent_id(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->parent : -EINVAL;
}

/**
 * mnt_fs_get_devno:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: value of st_dev for files on filesystem or 0 in case of error.
 */
dev_t mnt_fs_get_devno(struct libmnt_fs *fs)
{
	assert(fs);
	return fs ? fs->devno : 0;
}

/**
 * mnt_fs_get_option:
 * @fs: fstab/mtab/mountinfo entry pointer
 * @name: option name
 * @value: returns pointer to the begin of the value (e.g. name=VALUE) or NULL
 * @valsz: returns size of options value or 0
 *
 * Returns: 0 on success, 1 when not found the @name or negative number in case of error.
 */
int mnt_fs_get_option(struct libmnt_fs *fs, const char *name,
		char **value, size_t *valsz)
{
	char rc = 1;

	if (fs->fs_optstr)
		rc = mnt_optstr_get_option(fs->fs_optstr, name, value, valsz);
	if (rc == 1 && fs->vfs_optstr)
		rc = mnt_optstr_get_option(fs->vfs_optstr, name, value, valsz);
	if (rc == 1 && fs->user_optstr)
		rc = mnt_optstr_get_option(fs->user_optstr, name, value, valsz);
	return rc;
}

/**
 * mnt_fs_get_attribute:
 * @fs: fstab/mtab/mountinfo entry pointer
 * @name: option name
 * @value: returns pointer to the begin of the value (e.g. name=VALUE) or NULL
 * @valsz: returns size of options value or 0
 *
 * Returns: 0 on success, 1 when not found the @name or negative number in case of error.
 */
int mnt_fs_get_attribute(struct libmnt_fs *fs, const char *name,
		char **value, size_t *valsz)
{
	char rc = 1;

	if (fs->attrs)
		rc = mnt_optstr_get_option(fs->attrs, name, value, valsz);
	return rc;
}

/**
 * mnt_fs_match_target:
 * @fs: filesystem
 * @target: mountpoint path
 * @cache: tags/paths cache or NULL
 *
 * Possible are three attempts:
 *	1) compare @target with @fs->target
 *	2) realpath(@target) with @fs->target
 *	3) realpath(@target) with realpath(@fs->target).
 *
 * The 2nd and 3rd attempts are not performed when @cache is NULL.
 *
 * Returns: 1 if @fs target is equal to @target else 0.
 */
int mnt_fs_match_target(struct libmnt_fs *fs, const char *target, struct libmnt_cache *cache)
{
	int rc = 0;

	if (!fs || !target || !fs->target)
		return 0;

	/* 1) native paths */
	rc = !strcmp(target, fs->target);

	if (!rc && cache) {
		/* 2) - canonicalized and non-canonicalized */
		char *cn = mnt_resolve_path(target, cache);
		rc = (cn && strcmp(cn, fs->target) == 0);

		/* 3) - canonicalized and canonicalized */
		if (!rc && cn) {
			char *tcn = mnt_resolve_path(fs->target, cache);
			rc = (tcn && strcmp(cn, tcn) == 0);
		}
	}

	return rc;
}

/**
 * mnt_fs_match_source:
 * @fs: filesystem
 * @source: tag or path (device or so) or NULL
 * @cache: tags/paths cache or NULL
 *
 * Possible are four attempts:
 *	1) compare @source with @fs->source
 *	2) compare realpath(@source) with @fs->source
 *	3) compare realpath(@source) with realpath(@fs->source)
 *	4) compare realpath(@source) with evaluated tag from @fs->source
 *
 * The 2nd, 3rd and 4th attempts are not performed when @cache is NULL. The
 * 2nd and 3rd attempts are not performed if @fs->source is tag.
 *
 * Note that valid source path is NULL; the libmount uses NULL instead of
 * "none".  The "none" is used in /proc/{mounts,self/mountninfo} for pseudo
 * filesystems.
 *
 * Returns: 1 if @fs source is equal to @source else 0.
 */
int mnt_fs_match_source(struct libmnt_fs *fs, const char *source, struct libmnt_cache *cache)
{
	char *cn;
	const char *src, *t, *v;

	if (!fs)
		return 0;

	/* undefined source -- "none" in /proc */
	if (source == NULL && fs->source == NULL)
		return 1;

	if (source == NULL || fs->source == NULL)
		return 0;

	/* 1) native paths/tags */
	if (!strcmp(source, fs->source))
		return 1;

	if (!cache)
		return 0;
	if (fs->flags & (MNT_FS_NET | MNT_FS_PSEUDO))
		return 0;

	cn = mnt_resolve_spec(source, cache);
	if (!cn)
		return 0;

	/* 2) canonicalized and native */
	src = mnt_fs_get_srcpath(fs);
	if (src && !strcmp(cn, src))
		return 1;

	/* 3) canonicalized and canonicalized */
	if (src) {
		src = mnt_resolve_path(src, cache);
		if (src && !strcmp(cn, src))
			return 1;
	}
	if (src || mnt_fs_get_tag(fs, &t, &v))
		/* src path does not match and tag is not defined */
		return 0;

	/* read @source's tags to the cache */
	if (mnt_cache_read_tags(cache, cn) < 0) {
		if (errno == EACCES) {
			/* we don't have permissions to read TAGs from
			 * @source, but can translate @fs tag to devname.
			 *
			 * (because libblkid uses udev symlinks and this is
			 * accessible for non-root uses)
			 */
			char *x = mnt_resolve_tag(t, v, cache);
			if (x && !strcmp(x, cn))
				return 1;
		}
		return 0;
	}

	/* 4) has the @source a tag that matches with tag from @fs ? */
	if (mnt_cache_device_has_tag(cache, cn, t, v))
		return 1;

	return 0;
}

/**
 * mnt_fs_match_fstype:
 * @fs: filesystem
 * @types: filesystem name or comma delimited list of filesystems
 *
 * For more details see mnt_match_fstype().
 *
 * Returns: 1 if @fs type is matching to @types else 0. The function returns
 * 0 when types is NULL.
 */
int mnt_fs_match_fstype(struct libmnt_fs *fs, const char *types)
{
	return mnt_match_fstype(fs->fstype, types);
}

/**
 * mnt_fs_match_options:
 * @fs: filesystem
 * @options: comma delimited list of options (and nooptions)
 *
 * For more details see mnt_match_options().
 *
 * Returns: 1 if @fs type is matching to @options else 0. The function returns
 * 0 when types is NULL.
 */
int mnt_fs_match_options(struct libmnt_fs *fs, const char *options)
{
	return mnt_match_options(mnt_fs_get_options(fs), options);
}

/**
 * mnt_fs_print_debug
 * @fs: fstab/mtab/mountinfo entry
 * @file: file stream
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_print_debug(struct libmnt_fs *fs, FILE *file)
{
	if (!fs)
		return -EINVAL;
	fprintf(file, "------ fs: %p\n", fs);
	fprintf(file, "source: %s\n", mnt_fs_get_source(fs));
	fprintf(file, "target: %s\n", mnt_fs_get_target(fs));
	fprintf(file, "fstype: %s\n", mnt_fs_get_fstype(fs));

	if (mnt_fs_get_options(fs))
		fprintf(file, "optstr: %s\n", mnt_fs_get_options(fs));
	if (mnt_fs_get_vfs_options(fs))
		fprintf(file, "VFS-optstr: %s\n", mnt_fs_get_vfs_options(fs));
	if (mnt_fs_get_fs_options(fs))
		fprintf(file, "FS-opstr: %s\n", mnt_fs_get_fs_options(fs));
	if (mnt_fs_get_user_options(fs))
		fprintf(file, "user-optstr: %s\n", mnt_fs_get_user_options(fs));
	if (mnt_fs_get_attributes(fs))
		fprintf(file, "attributes: %s\n", mnt_fs_get_attributes(fs));

	if (mnt_fs_get_root(fs))
		fprintf(file, "root:   %s\n", mnt_fs_get_root(fs));
	if (mnt_fs_get_bindsrc(fs))
		fprintf(file, "bindsrc: %s\n", mnt_fs_get_bindsrc(fs));
	if (mnt_fs_get_freq(fs))
		fprintf(file, "freq:   %d\n", mnt_fs_get_freq(fs));
	if (mnt_fs_get_passno(fs))
		fprintf(file, "pass:   %d\n", mnt_fs_get_passno(fs));
	if (mnt_fs_get_id(fs))
		fprintf(file, "id:     %d\n", mnt_fs_get_id(fs));
	if (mnt_fs_get_parent_id(fs))
		fprintf(file, "parent: %d\n", mnt_fs_get_parent_id(fs));
	if (mnt_fs_get_devno(fs))
		fprintf(file, "devno:  %d:%d\n", major(mnt_fs_get_devno(fs)),
						 minor(mnt_fs_get_devno(fs)));
	return 0;
}

/**
 * mnt_free_mntent:
 * @mnt: mount entry
 *
 * Deallocates "mntent.h" mount entry.
 */
void mnt_free_mntent(struct mntent *mnt)
{
	if (mnt) {
		free(mnt->mnt_fsname);
		free(mnt->mnt_dir);
		free(mnt->mnt_type);
		free(mnt->mnt_opts);
		free(mnt);
	}
}

/**
 * mnt_fs_to_mntent:
 * @fs: filesystem
 * @mnt: mount description (as described in mntent.h)
 *
 * Copies information from @fs to struct mntent @mnt. If @mnt is already set
 * then the struct mntent items are reallocated and updated. See also
 * mnt_free_mntent().
 *
 * Returns: 0 on success and negative number in case of error.
 */
int mnt_fs_to_mntent(struct libmnt_fs *fs, struct mntent **mnt)
{
	int rc;
	struct mntent *m;

	if (!fs || !mnt)
		return -EINVAL;

	m = *mnt;
	if (!m) {
		m = calloc(1, sizeof(*m));
		if (!m)
			return -ENOMEM;
	}

	if ((rc = update_str(&m->mnt_fsname, mnt_fs_get_source(fs))))
		goto err;
	if ((rc = update_str(&m->mnt_dir, mnt_fs_get_target(fs))))
		goto err;
	if ((rc = update_str(&m->mnt_type, mnt_fs_get_fstype(fs))))
		goto err;

	errno = 0;
	m->mnt_opts = mnt_fs_strdup_options(fs);
	if (!m->mnt_opts && errno) {
		rc = -errno;
		goto err;
	}

	m->mnt_freq = mnt_fs_get_freq(fs);
	m->mnt_passno = mnt_fs_get_passno(fs);

	if (!m->mnt_fsname) {
		m->mnt_fsname = strdup("none");
		if (!m->mnt_fsname)
			goto err;
	}
	*mnt = m;

	return 0;
err:
	if (m != *mnt)
		mnt_free_mntent(m);
	return rc;
}
