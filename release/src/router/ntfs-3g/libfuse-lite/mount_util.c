/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU LGPLv2.
    See the file COPYING.LIB.
*/

#include "config.h"
#include "mount_util.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/param.h>

static int mtab_needs_update(const char *mnt)
{
	int res;
	struct stat stbuf;

	/* If mtab is within new mount, don't touch it */
	if (strncmp(mnt, _PATH_MOUNTED, strlen(mnt)) == 0 &&
	    _PATH_MOUNTED[strlen(mnt)] == '/')
		return 0;

	/*
	 * Skip mtab update if /etc/mtab:
	 *
	 *  - doesn't exist,
	 *  - is a symlink,
	 *  - is on a read-only filesystem.
	 */
	res = lstat(_PATH_MOUNTED, &stbuf);
	if (res == -1) {
		if (errno == ENOENT)
			return 0;
	} else {
		if (S_ISLNK(stbuf.st_mode))
			return 0;

		res = access(_PATH_MOUNTED, W_OK);
		if (res == -1 && errno == EROFS)
			return 0;
	}

	return 1;
}

int fuse_mnt_add_mount(const char *progname, const char *fsname,
                       const char *mnt, const char *type, const char *opts)
{
    int res;

    if (!mtab_needs_update(mnt))
        return 0;

    res = fork();
    if (res == -1) {
        fprintf(stderr, "%s: fork: %s\n", progname, strerror(errno));
        return 0;
    }
    if (res == 0) {
        char templ[] = "/tmp/fusermountXXXXXX";
        char *tmp;

        setuid(geteuid());

        /* 
         * hide in a directory, where mount isn't able to resolve
         * fsname as a valid path
         */
        tmp = mkdtemp(templ);
        if (!tmp) {
            fprintf(stderr, "%s: failed to create temporary directory\n",
                    progname);
            exit(1);
        }
        if (chdir(tmp)) {
            fprintf(stderr, "%s: failed to chdir to %s: %s\n",
                    progname, tmp, strerror(errno));
            exit(1);
        }
        rmdir(tmp);
        execl("/bin/mount", "/bin/mount", "-i", "-f", "-t", type, "-o", opts,
              fsname, mnt, NULL);
        fprintf(stderr, "%s: failed to execute /bin/mount: %s\n", progname,
                strerror(errno));
        exit(1);
    }
    return 0;
}

int fuse_mnt_umount(const char *progname, const char *mnt, int lazy)
{
    int res;
    int status;

    if (!mtab_needs_update(mnt)) {
        res = umount2(mnt, lazy ? 2 : 0);
        if (res == -1)
            fprintf(stderr, "%s: failed to unmount %s: %s\n", progname,
                    mnt, strerror(errno));
        return res;
    }

    res = fork();
    if (res == -1) {
        fprintf(stderr, "%s: fork: %s\n", progname, strerror(errno));
        return -1;
    }
    if (res == 0) {
        setuid(geteuid());
        execl("/bin/umount", "/bin/umount", "-i", mnt, lazy ? "-l" : NULL,
              NULL);
        fprintf(stderr, "%s: failed to execute /bin/umount: %s\n", progname,
                strerror(errno));
        exit(1);
    }
    res = waitpid(res, &status, 0);
    if (res == -1) {
        fprintf(stderr, "%s: waitpid: %s\n", progname, strerror(errno));
        return -1;
    }
    if (status != 0)
        return -1;

    return 0;
}

char *fuse_mnt_resolve_path(const char *progname, const char *orig)
{
    char buf[PATH_MAX];
    char *copy;
    char *dst;
    char *end;
    char *lastcomp;
    const char *toresolv;

    if (!orig[0]) {
        fprintf(stderr, "%s: invalid mountpoint '%s'\n", progname, orig);
        return NULL;
    }

    copy = strdup(orig);
    if (copy == NULL) {
        fprintf(stderr, "%s: failed to allocate memory\n", progname);
        return NULL;
    }

    toresolv = copy;
    lastcomp = NULL;
    for (end = copy + strlen(copy) - 1; end > copy && *end == '/'; end --);
    if (end[0] != '/') {
        char *tmp;
        end[1] = '\0';
        tmp = strrchr(copy, '/');
        if (tmp == NULL) {
            lastcomp = copy;
            toresolv = ".";
        } else {
            lastcomp = tmp + 1;
            if (tmp == copy)
                toresolv = "/";
        }
        if (strcmp(lastcomp, ".") == 0 || strcmp(lastcomp, "..") == 0) {
            lastcomp = NULL;
            toresolv = copy;
        }
        else if (tmp)
            tmp[0] = '\0';
    }
    if (realpath(toresolv, buf) == NULL) {
        fprintf(stderr, "%s: bad mount point %s: %s\n", progname, orig,
                strerror(errno));
        free(copy);
        return NULL;
    }
    if (lastcomp == NULL)
        dst = strdup(buf);
    else {
        dst = (char *) malloc(strlen(buf) + 1 + strlen(lastcomp) + 1);
        if (dst) {
            unsigned buflen = strlen(buf);
            if (buflen && buf[buflen-1] == '/')
                sprintf(dst, "%s%s", buf, lastcomp);
            else
                sprintf(dst, "%s/%s", buf, lastcomp);
        }
    }
    free(copy);
    if (dst == NULL)
        fprintf(stderr, "%s: failed to allocate memory\n", progname);
    return dst;
}

int fuse_mnt_check_fuseblk(void)
{
    char buf[256];
    FILE *f = fopen("/proc/filesystems", "r");
    if (!f)
        return 1;

    while (fgets(buf, sizeof(buf), f))
        if (strstr(buf, "fuseblk\n")) {
            fclose(f);
            return 1;
        }

    fclose(f);
    return 0;
}
