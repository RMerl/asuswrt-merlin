/*
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: utils
 * @title: Utils
 * @short_description: misc utils.
 */
#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include "strutils.h"
#include "pathnames.h"
#include "mountP.h"
#include "mangle.h"
#include "canonicalize.h"
#include "env.h"

int endswith(const char *s, const char *sx)
{
	ssize_t off;

	assert(s);
	assert(sx);

	off = strlen(s);
	if (!off)
		return 0;
	off -= strlen(sx);
	if (off < 0)
		return 0;

        return !strcmp(s + off, sx);
}

int startswith(const char *s, const char *sx)
{
	size_t off;

	assert(s);
	assert(sx);

	off = strlen(sx);
	if (!off)
		return 0;

        return !strncmp(s, sx, off);
}

/* returns basename and keeps dirname in the @path, if @path is "/" (root)
 * then returns empty string */
static char *stripoff_last_component(char *path)
{
	char *p = path ? strrchr(path, '/') : NULL;

	if (!p)
		return NULL;
	*p = '\0';
	return ++p;
}

/* Note that the @target has to be absolute path (so at least "/")
 */
int mnt_chdir_to_parent(const char *target, char **filename)
{
	char *path, *last = NULL;
	char cwd[PATH_MAX];
	int rc = -EINVAL;

	if (!target || *target != '/')
		return -EINVAL;

	path = strdup(target);
	if (!path)
		return -ENOMEM;

	if (*(path + 1) != '\0') {
		last = stripoff_last_component(path);
		if (!last)
			goto err;
	}
	if (!*path)
		*path = '/';	/* root */

	if (chdir(path) == -1) {
		DBG(UTILS, mnt_debug("failed to chdir to %s: %m", path));
		rc = -errno;
		goto err;
	}
	if (!getcwd(cwd, sizeof(cwd))) {
		DBG(UTILS, mnt_debug("failed to obtain current directory: %m"));
		rc = -errno;
		goto err;
	}
	if (strcmp(cwd, path) != 0) {
		DBG(UTILS, mnt_debug("path moved (%s -> %s)", path, cwd));
		goto err;
	}

	DBG(CXT, mnt_debug("current directory moved to %s", path));

	*filename = path;

	if (!last || !*last)
		memcpy(*filename, ".", 2);
	else
		memcpy(*filename, last, strlen(last) + 1);
	return 0;
err:
	free(path);
	return rc;
}

/**
 * mnt_mangle:
 * @str: string
 *
 * Encode @str to be compatible with fstab/mtab
 *
 * Returns: new allocated string or NULL in case of error.
 */
char *mnt_mangle(const char *str)
{
	return mangle(str);
}

/**
 * mnt_unmangle:
 * @str: string
 *
 * Decode @str from fstab/mtab
 *
 * Returns: new allocated string or NULL in case of error.
 */
char *mnt_unmangle(const char *str)
{
	return unmangle(str, NULL);
}

/**
 * mnt_fstype_is_pseudofs:
 * @type: filesystem name
 *
 * Returns: 1 for filesystems like proc, sysfs, ... or 0.
 */
int mnt_fstype_is_pseudofs(const char *type)
{
	if (!type)
		return 0;
	if (strcmp(type, "none")  == 0 ||
	    strcmp(type, "proc")  == 0 ||
	    strcmp(type, "tmpfs") == 0 ||
	    strcmp(type, "sysfs") == 0 ||
	    strcmp(type, "autofs") == 0 ||
	    strcmp(type, "devpts") == 0||
	    strcmp(type, "cgroup") == 0 ||
	    strcmp(type, "devtmpfs") == 0 ||
	    strcmp(type, "devfs") == 0 ||
	    strcmp(type, "dlmfs") == 0 ||
	    strcmp(type, "cpuset") == 0 ||
	    strcmp(type, "securityfs") == 0 ||
	    strcmp(type, "hugetlbfs") == 0 ||
	    strcmp(type, "rpc_pipefs") == 0 ||
	    strcmp(type, "fusectl") == 0 ||
	    strcmp(type, "mqueue") == 0 ||
	    strcmp(type, "binfmt_misc") == 0 ||
	    strcmp(type, "fuse.gvfs-fuse-daemon") == 0 ||
	    strcmp(type, "debugfs") == 0 ||
	    strcmp(type, "spufs") == 0)
		return 1;
	return 0;
}

/**
 * mnt_fstype_is_netfs:
 * @type: filesystem name
 *
 * Returns: 1 for filesystems like cifs, nfs, ... or 0.
 */
int mnt_fstype_is_netfs(const char *type)
{
	if (!type)
		return 0;
	if (strcmp(type, "cifs")   == 0 ||
	    strcmp(type, "smbfs")  == 0 ||
	    strncmp(type,"nfs", 3) == 0 ||
	    strcmp(type, "afs")    == 0 ||
	    strcmp(type, "ncpfs")  == 0 ||
	    strncmp(type,"9p", 2)  == 0)
		return 1;
	return 0;
}

/**
 * mnt_match_fstype:
 * @type: filesystem type
 * @pattern: filesystem name or comma delimited list of names
 *
 * The @pattern list of filesystem can be prefixed with a global
 * "no" prefix to invert matching of the whole list. The "no" could
 * also be used for individual items in the @pattern list. So,
 * "nofoo,bar" has the same meaning as "nofoo,nobar".
 *
 * "bar"  : "nofoo,bar"		-> False   (global "no" prefix)
 *
 * "bar"  : "foo,bar"		-> True
 *
 * "bar" : "foo,nobar"		-> False
 *
 * Returns: 1 if type is matching, else 0. This function also returns
 *          0 if @pattern is NULL and @type is non-NULL.
 */
int mnt_match_fstype(const char *type, const char *pattern)
{
	int no = 0;		/* negated types list */
	int len;
	const char *p;

	if (!pattern && !type)
		return 1;
	if (!pattern)
		return 0;

	if (!strncmp(pattern, "no", 2)) {
		no = 1;
		pattern += 2;
	}

	/* Does type occur in types, separated by commas? */
	len = strlen(type);
	p = pattern;
	while(1) {
		if (!strncmp(p, "no", 2) && !strncmp(p+2, type, len) &&
		    (p[len+2] == 0 || p[len+2] == ','))
			return 0;
		if (strncmp(p, type, len) == 0 && (p[len] == 0 || p[len] == ','))
			return !no;
		p = strchr(p,',');
		if (!p)
			break;
		p++;
	}
	return no;
}


/* Returns 1 if needle found or noneedle not found in haystack
 * Otherwise returns 0
 */
static int check_option(const char *haystack, size_t len,
			const char *needle, size_t needle_len)
{
	const char *p;
	int no = 0;

	if (needle_len >= 2 && !strncmp(needle, "no", 2)) {
		no = 1;
		needle += 2;
		needle_len -= 2;
	}

	for (p = haystack; p && p < haystack + len; p++) {
		char *sep = strchr(p, ',');
		size_t plen = sep ? (size_t) (sep - p) :
				    len - (p - haystack);

		if (plen == needle_len) {
			if (!strncmp(p, needle, plen))
				return !no;	/* foo or nofoo was found */
		}
		p += plen;
	}

	return no;  /* foo or nofoo was not found */
}

/**
 * mnt_match_options:
 * @optstr: options string
 * @pattern: comma delimited list of options
 *
 * The "no" could used for individual items in the @options list. The "no"
 * prefix does not have a global meaning.
 *
 * Unlike fs type matching, nonetdev,user and nonetdev,nouser have
 * DIFFERENT meanings; each option is matched explicitly as specified.
 *
 * "xxx,yyy,zzz" : "nozzz"	-> False
 *
 * "xxx,yyy,zzz" : "xxx,noeee"	-> True
 *
 * Returns: 1 if pattern is matching, else 0. This function also returns 0
 *          if @pattern is NULL and @optstr is non-NULL.
 */
int mnt_match_options(const char *optstr, const char *pattern)
{
	const char *p;
	size_t len, optstr_len = 0;

	if (!pattern && !optstr)
		return 1;
	if (!pattern)
		return 0;

	len = strlen(pattern);
	if (optstr)
		optstr_len = strlen(optstr);

	for (p = pattern; p < pattern + len; p++) {
		char *sep = strchr(p, ',');
		size_t plen = sep ? (size_t) (sep - p) :
				    len - (p - pattern);

		if (!plen)
			continue; /* if two ',' appear in a row */

		if (!check_option(optstr, optstr_len, p, plen))
			return 0; /* any match failure means failure */

		p += plen;
	}

	/* no match failures in list means success */
	return 1;
}

void mnt_free_filesystems(char **filesystems)
{
	char **p;

	if (!filesystems)
		return;
	for (p = filesystems; *p; p++)
		free(*p);
	free(filesystems);
}

static int add_filesystem(char ***filesystems, char *name)
{
	int n = 0;

	assert(filesystems);
	assert(name);

	if (*filesystems) {
		char **p;
		for (n = 0, p = *filesystems; *p; p++, n++) {
			if (strcmp(*p, name) == 0)
				return 0;
		}
	}

	#define MYCHUNK	16

	if (n == 0 || !((n + 1) % MYCHUNK)) {
		size_t items = ((n + 1 + MYCHUNK) / MYCHUNK) * MYCHUNK;
		char **x = realloc(*filesystems, items * sizeof(char *));

		if (!x)
			goto err;
		*filesystems = x;
	}
	name = strdup(name);
	if (!name)
		goto err;
	(*filesystems)[n] = name;
	(*filesystems)[n + 1] = NULL;
	return 0;
err:
	mnt_free_filesystems(*filesystems);
	return -ENOMEM;
}

static int get_filesystems(const char *filename, char ***filesystems, const char *pattern)
{
	FILE *f;
	char line[128];

	f = fopen(filename, "r");
	if (!f)
		return 0;

	while (fgets(line, sizeof(line), f)) {
		char name[sizeof(line)];
		int rc;

		if (*line == '#' || strncmp(line, "nodev", 5) == 0)
			continue;
		if (sscanf(line, " %128[^\n ]\n", name) != 1)
			continue;
		if (pattern && !mnt_match_fstype(name, pattern))
			continue;
		rc = add_filesystem(filesystems, name);
		if (rc)
			return rc;
	}
	return 0;
}

int mnt_get_filesystems(char ***filesystems, const char *pattern)
{
	int rc;

	if (!filesystems)
		return -EINVAL;
	*filesystems = NULL;

	rc = get_filesystems(_PATH_FILESYSTEMS, filesystems, pattern);
	if (rc)
		return rc;
	return get_filesystems(_PATH_PROC_FILESYSTEMS, filesystems, pattern);
}

/*
 * Returns allocated string with username or NULL.
 */
char *mnt_get_username(const uid_t uid)
{
        struct passwd pwd;
	struct passwd *res;
#ifdef _SC_GETPW_R_SIZE_MAX
	size_t sz = sysconf(_SC_GETPW_R_SIZE_MAX);
#else
	size_t sz = 0;
#endif
	char *buf, *username = NULL;

	if (sz <= 0)
		sz = 16384;        /* Should be more than enough */

	buf = malloc(sz);
	if (!buf)
		return NULL;

	if (!getpwuid_r(uid, &pwd, buf, sz, &res) && res)
		username = strdup(pwd.pw_name);

	free(buf);
	return username;
}

int mnt_get_uid(const char *username, uid_t *uid)
{
	int rc = -1;
        struct passwd pwd;
	struct passwd *pw;
	size_t sz = sysconf(_SC_GETPW_R_SIZE_MAX);
	char *buf;

	if (!username || !uid)
		return -EINVAL;
	if (sz <= 0)
		sz = 16384;        /* Should be more than enough */

	buf = malloc(sz);
	if (!buf)
		return -ENOMEM;

	if (!getpwnam_r(username, &pwd, buf, sz, &pw) && pw) {
		*uid= pw->pw_uid;
		rc = 0;
	} else {
		DBG(UTILS, mnt_debug(
			"cannot convert '%s' username to UID", username));
	}

	free(buf);
	return rc;
}

int mnt_get_gid(const char *groupname, gid_t *gid)
{
	int rc = -1;
        struct group grp;
	struct group *gr;
	size_t sz = sysconf(_SC_GETGR_R_SIZE_MAX);
	char *buf;

	if (!groupname || !gid)
		return -EINVAL;
	if (sz <= 0)
		sz = 16384;        /* Should be more than enough */

	buf = malloc(sz);
	if (!buf)
		return -ENOMEM;

	if (!getgrnam_r(groupname, &grp, buf, sz, &gr) && gr) {
		*gid= gr->gr_gid;
		rc = 0;
	} else {
		DBG(UTILS, mnt_debug(
			"cannot convert '%s' groupname to GID", groupname));
	}

	free(buf);
	return rc;
}

int mnt_in_group(gid_t gid)
{
	int rc = 0, n, i;
	gid_t *grps = NULL;

	if (getgid() == gid)
		return 1;

	n = getgroups(0, NULL);
	if (n <= 0)
		goto done;

	grps = malloc(n * sizeof(*grps));
	if (!grps)
		goto done;

	if (getgroups(n, grps) == n) {
		for (i = 0; i < n; i++) {
			if (grps[i] == gid) {
				rc = 1;
				break;
			}
		}
	}
done:
	free(grps);
	return rc;
}

static int try_write(const char *filename)
{
	int fd;

	if (!filename)
		return -EINVAL;

	fd = open(filename, O_RDWR|O_CREAT, S_IWUSR| \
					    S_IRUSR|S_IRGRP|S_IROTH);
	if (fd >= 0) {
		close(fd);
		return 0;
	}
	return -errno;
}

/**
 * mnt_has_regular_mtab:
 * @mtab: returns path to mtab
 * @writable: returns 1 if the file is writable
 *
 * If the file does not exist and @writable argument is not NULL then it will
 * try to create the file
 *
 * Returns: 1 if /etc/mtab is a regular file, and 0 in case of error (check
 *          errno for more details).
 */
int mnt_has_regular_mtab(const char **mtab, int *writable)
{
	struct stat st;
	int rc;
	const char *filename = mtab && *mtab ? *mtab : mnt_get_mtab_path();

	if (writable)
		*writable = 0;
	if (mtab && !*mtab)
		*mtab = filename;

	DBG(UTILS, mnt_debug("mtab: %s", filename));

	rc = lstat(filename, &st);

	if (rc == 0) {
		/* file exist */
		if (S_ISREG(st.st_mode)) {
			if (writable)
				*writable = !try_write(filename);
			return 1;
		}
		goto done;
	}

	/* try to create the file */
	if (writable) {
		*writable = !try_write(filename);
		if (*writable)
			return 1;
	}

done:
	DBG(UTILS, mnt_debug("%s: irregular/non-writable", filename));
	return 0;
}

/*
 * Don't export this to libmount API -- utab is private library stuff.
 *
 * If the file does not exist and @writable argument is not NULL then it will
 * try to create the directory (e.g. /run/mount) and the file.
 *
 * Returns: 1 if utab is a regular file, and 0 in case of
 *          error (check errno for more details).
 */
int mnt_has_regular_utab(const char **utab, int *writable)
{
	struct stat st;
	int rc;
	const char *filename = utab && *utab ? *utab : mnt_get_utab_path();

	if (writable)
		*writable = 0;
	if (utab && !*utab)
		*utab = filename;

	DBG(UTILS, mnt_debug("utab: %s", filename));

	rc = lstat(filename, &st);

	if (rc == 0) {
		/* file exist */
		if (S_ISREG(st.st_mode)) {
			if (writable)
				*writable = !try_write(filename);
			return 1;
		}
		goto done;	/* it's not regular file */
	}

	if (writable) {
		char *dirname = strdup(filename);

		if (!dirname)
			goto done;

		stripoff_last_component(dirname);	/* remove filename */

		rc = mkdir(dirname, S_IWUSR|
				    S_IRUSR|S_IRGRP|S_IROTH|
				    S_IXUSR|S_IXGRP|S_IXOTH);
		free(dirname);
		if (rc && errno != EEXIST)
			goto done;			/* probably EACCES */

		*writable = !try_write(filename);
		if (*writable)
			return 1;
	}
done:
	DBG(UTILS, mnt_debug("%s: irregular/non-writable file", filename));
	return 0;
}

/**
 * mnt_get_fstab_path:
 *
 * Returns: path to /etc/fstab or $LIBMOUNT_FSTAB.
 */
const char *mnt_get_fstab_path(void)
{
	const char *p = safe_getenv("LIBMOUNT_FSTAB");
	return p ? : _PATH_MNTTAB;
}

/**
 * mnt_get_mtab_path:
 *
 * This function returns *default* location of the mtab file. The result does
 * not have to be writable. See also mnt_has_regular_mtab().
 *
 * Returns: path to /etc/mtab or $LIBMOUNT_MTAB.
 */
const char *mnt_get_mtab_path(void)
{
	const char *p = safe_getenv("LIBMOUNT_MTAB");
	return p ? : _PATH_MOUNTED;
}

/*
 * Don't export this to libmount API -- utab is private library stuff.
 *
 * Returns: path to /run/mount/utab (or /dev/.mount/utab) or $LIBMOUNT_UTAB.
 */
const char *mnt_get_utab_path(void)
{
	struct stat st;
	const char *p = safe_getenv("LIBMOUNT_UTAB");

	if (p)
		return p;

	if (stat(MNT_RUNTIME_TOPDIR, &st) == 0)
		return MNT_PATH_UTAB;

	return MNT_PATH_UTAB_OLD;
}


/* returns file descriptor or -errno, @name returns uniques filename
 */
int mnt_open_uniq_filename(const char *filename, char **name)
{
	int rc, fd;
	char *n;

	assert(filename);

	if (name)
		*name = NULL;

	rc = asprintf(&n, "%s.XXXXXX", filename);
	if (rc <= 0)
		return -errno;

	fd = mkstemp(n);
	if (fd >= 0 && name)
		*name = n;
	else
		free(n);

	return fd < 0 ? -errno : fd;
}

char *mnt_get_mountpoint(const char *path)
{
	char *mnt = strdup(path);
	struct stat st;
	dev_t dir, base;

	if (!mnt)
		return NULL;
	if (*mnt == '/' && *(mnt + 1) == '\0')
		goto done;

	if (stat(mnt, &st))
		goto err;
	base = st.st_dev;

	do {
		char *p = stripoff_last_component(mnt);

		if (!p)
			break;
		if (stat(*mnt ? mnt : "/", &st))
			goto err;
		dir = st.st_dev;
		if (dir != base) {
			*(p - 1) = '/';
			goto done;
		}
		base = dir;
	} while (mnt && *(mnt + 1) != '\0');

	memcpy(mnt, "/", 2);
done:
	DBG(UTILS, mnt_debug("%s mountpoint is %s", path, mnt));
	return mnt;
err:
	free(mnt);
	return NULL;
}

char *mnt_get_fs_root(const char *path, const char *mnt)
{
	char *m = (char *) mnt, *res;
	const char *p;
	size_t sz;

	if (!m)
		m = mnt_get_mountpoint(path);
	if (!m)
		return NULL;

	sz = strlen(m);
	p = sz > 1 ? path + sz : path;

	if (m != mnt)
		free(m);

	res = *p ? strdup(p) : strdup("/");
	DBG(UTILS, mnt_debug("%s fs-root is %s", path, res));
	return res;
}

#ifdef TEST_PROGRAM
int test_match_fstype(struct libmnt_test *ts, int argc, char *argv[])
{
	char *type = argv[1];
	char *pattern = argv[2];

	printf("%s\n", mnt_match_fstype(type, pattern) ? "MATCH" : "NOT-MATCH");
	return 0;
}

int test_match_options(struct libmnt_test *ts, int argc, char *argv[])
{
	char *optstr = argv[1];
	char *pattern = argv[2];

	printf("%s\n", mnt_match_options(optstr, pattern) ? "MATCH" : "NOT-MATCH");
	return 0;
}

int test_startswith(struct libmnt_test *ts, int argc, char *argv[])
{
	char *optstr = argv[1];
	char *pattern = argv[2];

	printf("%s\n", startswith(optstr, pattern) ? "YES" : "NOT");
	return 0;
}

int test_endswith(struct libmnt_test *ts, int argc, char *argv[])
{
	char *optstr = argv[1];
	char *pattern = argv[2];

	printf("%s\n", endswith(optstr, pattern) ? "YES" : "NOT");
	return 0;
}

int test_mountpoint(struct libmnt_test *ts, int argc, char *argv[])
{
	char *path = canonicalize_path(argv[1]),
	     *mnt = path ? mnt_get_mountpoint(path) :  NULL;

	printf("%s: %s\n", argv[1], mnt ? : "unknown");
	free(mnt);
	free(path);
	return 0;
}

int test_fsroot(struct libmnt_test *ts, int argc, char *argv[])
{
	char *path = canonicalize_path(argv[1]),
	     *mnt = path ? mnt_get_fs_root(path, NULL) : NULL;

	printf("%s: %s\n", argv[1], mnt ? : "unknown");
	free(mnt);
	free(path);
	return 0;
}

int test_filesystems(struct libmnt_test *ts, int argc, char *argv[])
{
	char **filesystems = NULL;
	int rc;

	rc = mnt_get_filesystems(&filesystems, argc ? argv[1] : NULL);
	if (!rc) {
		char **p;
		for (p = filesystems; *p; p++)
			printf("%s\n", *p);
		mnt_free_filesystems(filesystems);
	}
	return rc;
}

int test_chdir(struct libmnt_test *ts, int argc, char *argv[])
{
	int rc;
	char *path = canonicalize_path(argv[1]),
	     *last = NULL;

	if (!path)
		return -errno;

	rc = mnt_chdir_to_parent(path, &last);
	if (!rc) {
		printf("path='%s', abs='%s', last='%s'\n",
				argv[1], path, last);
	}
	free(path);
	free(last);
	return rc;
}


int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--match-fstype",  test_match_fstype,    "<type> <pattern>     FS types matching" },
	{ "--match-options", test_match_options,   "<options> <pattern>  options matching" },
	{ "--filesystems",   test_filesystems,	   "[<pattern>] list /{etc,proc}/filesystems" },
	{ "--starts-with",   test_startswith,      "<string> <prefix>" },
	{ "--ends-with",     test_endswith,        "<string> <prefix>" },
	{ "--mountpoint",    test_mountpoint,      "<path>" },
	{ "--fs-root",       test_fsroot,          "<path>" },
	{ "--cd-parent",     test_chdir,           "<path>" },
	{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
