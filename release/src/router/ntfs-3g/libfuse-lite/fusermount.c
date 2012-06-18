/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/
/* This program does the mounting and unmounting of FUSE filesystems */

#include <config.h>

#include "mount_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <mntent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/fsuid.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <grp.h>

#define FUSE_DEV_NEW "/dev/fuse"

#ifndef MS_DIRSYNC
#define MS_DIRSYNC 128
#endif

static const char *progname = "ntfs-3g-mount";

static int mount_max = 1000;

int drop_privs(void);
int restore_privs(void);

static const char *get_user_name(void)
{
    struct passwd *pw = getpwuid(getuid());
    if (pw != NULL && pw->pw_name != NULL)
        return pw->pw_name;
    else {
        fprintf(stderr, "%s: could not determine username\n", progname);
        return NULL;
    }
}

int drop_privs(void)
{
	if (!getegid()) {

		gid_t new_gid = getgid();

		if (setresgid(-1, new_gid, getegid()) < 0) {
			perror("priv drop: setresgid failed");
			return -1;
		}
		if (getegid() != new_gid){
			perror("dropping group privilege failed");
			return -1;
		}
	}
	
	if (!geteuid()) {

		uid_t new_uid = getuid();

		if (setresuid(-1, new_uid, geteuid()) < 0) {
			perror("priv drop: setresuid failed");
			return -1;
		}
		if (geteuid() != new_uid){
			perror("dropping user privilege failed");
			return -1;
		}
	}
	
	return 0;
}

int restore_privs(void)
{
	if (geteuid()) {
		
		uid_t ruid, euid, suid;

		if (getresuid(&ruid, &euid, &suid) < 0) {
			perror("priv restore: getresuid failed");
			return -1;
		}
		if (setresuid(-1, suid, -1) < 0) {
			perror("priv restore: setresuid failed");
			return -1;
		}
		if (geteuid() != suid) {
			perror("restoring privilege failed");
			return -1;
		}
	}

	if (getegid()) {

		gid_t rgid, egid, sgid;

		if (getresgid(&rgid, &egid, &sgid) < 0) {
			perror("priv restore: getresgid failed");
			return -1;
		}
		if (setresgid(-1, sgid, -1) < 0) {
			perror("priv restore: setresgid failed");
			return -1;
		}
		if (getegid() != sgid){
			perror("restoring group privilege failed");
			return -1;
		}
	}
	
	return 0;
}

#ifndef IGNORE_MTAB
static int add_mount(const char *source, const char *mnt, const char *type,
                     const char *opts)
{
    return fuse_mnt_add_mount(progname, source, mnt, type, opts);
}

static int count_fuse_fs(void)
{
    struct mntent *entp;
    int count = 0;
    const char *mtab = _PATH_MOUNTED;
    FILE *fp = setmntent(mtab, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s: failed to open %s: %s\n", progname, mtab,
                strerror(errno));
        return -1;
    }
    while ((entp = getmntent(fp)) != NULL) {
        if (strcmp(entp->mnt_type, "fuse") == 0 ||
            strncmp(entp->mnt_type, "fuse.", 5) == 0)
            count ++;
    }
    endmntent(fp);
    return count;
}


#else /* IGNORE_MTAB */
static int count_fuse_fs()
{
    return 0;
}

static int add_mount(const char *source, const char *mnt, const char *type,
                     const char *opts)
{
    (void) source;
    (void) mnt;
    (void) type;
    (void) opts;
    return 0;
}
#endif /* IGNORE_MTAB */

static int begins_with(const char *s, const char *beg)
{
    if (strncmp(s, beg, strlen(beg)) == 0)
        return 1;
    else
        return 0;
}

struct mount_flags {
    const char *opt;
    unsigned long flag;
    int on;
    int safe;
};

static struct mount_flags mount_flags[] = {
    {"rw",      MS_RDONLY,      0, 1},
    {"ro",      MS_RDONLY,      1, 1},
    {"suid",    MS_NOSUID,      0, 0},
    {"nosuid",  MS_NOSUID,      1, 1},
    {"dev",     MS_NODEV,       0, 0},
    {"nodev",   MS_NODEV,       1, 1},
    {"exec",    MS_NOEXEC,      0, 1},
    {"noexec",  MS_NOEXEC,      1, 1},
    {"async",   MS_SYNCHRONOUS, 0, 1},
    {"sync",    MS_SYNCHRONOUS, 1, 1},
    {"atime",   MS_NOATIME,     0, 1},
    {"noatime", MS_NOATIME,     1, 1},
    {"dirsync", MS_DIRSYNC,     1, 1},
    {NULL,      0,              0, 0}
};

static int find_mount_flag(const char *s, unsigned len, int *on, int *flag)
{
    int i;

    for (i = 0; mount_flags[i].opt != NULL; i++) {
        const char *opt = mount_flags[i].opt;
        if (strlen(opt) == len && strncmp(opt, s, len) == 0) {
            *on = mount_flags[i].on;
            *flag = mount_flags[i].flag;
            if (!mount_flags[i].safe && getuid() != 0) {
                *flag = 0;
                fprintf(stderr, "%s: unsafe option '%s' ignored\n",
                        progname, opt);
            }
            return 1;
        }
    }
    return 0;
}

static int add_option(char **optsp, const char *opt, unsigned expand)
{
    char *newopts;
    if (*optsp == NULL)
        newopts = strdup(opt);
    else {
        unsigned oldsize = strlen(*optsp);
        unsigned newsize = oldsize + 1 + strlen(opt) + expand + 1;
        newopts = (char *) realloc(*optsp, newsize);
        if (newopts)
            sprintf(newopts + oldsize, ",%s", opt);
    }
    if (newopts == NULL) {
        fprintf(stderr, "%s: failed to allocate memory\n", progname);
        return -1;
    }
    *optsp = newopts;
    return 0;
}

static int get_mnt_opts(int flags, char *opts, char **mnt_optsp)
{
    int i;
    int l;

    if (!(flags & MS_RDONLY) && add_option(mnt_optsp, "rw", 0) == -1)
        return -1;

    for (i = 0; mount_flags[i].opt != NULL; i++) {
        if (mount_flags[i].on && (flags & mount_flags[i].flag) &&
            add_option(mnt_optsp, mount_flags[i].opt, 0) == -1)
            return -1;
    }

    if (add_option(mnt_optsp, opts, 0) == -1)
        return -1;
    /* remove comma from end of opts*/
    l = strlen(*mnt_optsp);
    if ((*mnt_optsp)[l-1] == ',')
        (*mnt_optsp)[l-1] = '\0';
    if (getuid() != 0) {
        const char *user = get_user_name();
        if (user == NULL)
            return -1;

        if (add_option(mnt_optsp, "user=", strlen(user)) == -1)
            return -1;
        strcat(*mnt_optsp, user);
    }
    return 0;
}

static int opt_eq(const char *s, unsigned len, const char *opt)
{
    if(strlen(opt) == len && strncmp(s, opt, len) == 0)
        return 1;
    else
        return 0;
}

static int get_string_opt(const char *s, unsigned len, const char *opt,
                          char **val)
{
    unsigned opt_len = strlen(opt);

    if (*val)
        free(*val);
    *val = (char *) malloc(len - opt_len + 1);
    if (!*val) {
        fprintf(stderr, "%s: failed to allocate memory\n", progname);
        return 0;
    }

    memcpy(*val, s + opt_len, len - opt_len);
    (*val)[len - opt_len] = '\0';
    return 1;
}

static int do_mount(const char *mnt, char **typep, mode_t rootmode,
                    int fd, const char *opts, const char *dev, char **sourcep,
                    char **mnt_optsp)
{
    int res;
    int flags = MS_NOSUID | MS_NODEV;
    char *optbuf;
    char *mnt_opts = NULL;
    const char *s;
    char *d;
    char *fsname = NULL;
    char *source = NULL;
    char *type = NULL;
    int blkdev = 0;

    optbuf = (char *) malloc(strlen(opts) + 128);
    if (!optbuf) {
        fprintf(stderr, "%s: failed to allocate memory\n", progname);
        return -1;
    }

    for (s = opts, d = optbuf; *s;) {
        unsigned len;
        const char *fsname_str = "fsname=";
        for (len = 0; s[len] && s[len] != ','; len++);
        if (begins_with(s, fsname_str)) {
            if (!get_string_opt(s, len, fsname_str, &fsname))
                goto err;
        } else if (opt_eq(s, len, "blkdev")) {
            blkdev = 1;
        } else if (!begins_with(s, "fd=") &&
                   !begins_with(s, "rootmode=") &&
                   !begins_with(s, "user_id=") &&
                   !begins_with(s, "group_id=")) {
            int on;
            int flag;
            int skip_option = 0;
            if (opt_eq(s, len, "large_read")) {
                struct utsname utsname;
                unsigned kmaj, kmin;
                res = uname(&utsname);
                if (res == 0 &&
                    sscanf(utsname.release, "%u.%u", &kmaj, &kmin) == 2 &&
                    (kmaj > 2 || (kmaj == 2 && kmin > 4))) {
                    fprintf(stderr, "%s: note: 'large_read' mount option is "
			    "deprecated for %i.%i kernels\n", progname, kmaj, kmin);
                    skip_option = 1;
                }
            }
            if (!skip_option) {
                if (find_mount_flag(s, len, &on, &flag)) {
                    if (on)
                        flags |= flag;
                    else
                        flags  &= ~flag;
                } else {
                    memcpy(d, s, len);
                    d += len;
                    *d++ = ',';
                }
            }
        }
        s += len;
        if (*s)
            s++;
    }
    *d = '\0';
    res = get_mnt_opts(flags, optbuf, &mnt_opts);
    if (res == -1)
        goto err;

    sprintf(d, "fd=%i,rootmode=%o,user_id=%i,group_id=%i",
            fd, rootmode, getuid(), getgid());

    source = malloc((fsname ? strlen(fsname) : 0) + strlen(dev) + 32);

    type = malloc(32);
    if (!type || !source) {
        fprintf(stderr, "%s: failed to allocate memory\n", progname);
        goto err;
    }

    strcpy(type, blkdev ? "fuseblk" : "fuse");

    if (fsname)
        strcpy(source, fsname);
    else
        strcpy(source, dev);

    if (restore_privs())
	goto err;
    
    res = mount(source, mnt, type, flags, optbuf);
    if (res == -1 && errno == EINVAL) {
        /* It could be an old version not supporting group_id */
        sprintf(d, "fd=%i,rootmode=%o,user_id=%i", fd, rootmode, getuid());
        res = mount(source, mnt, type, flags, optbuf);
    }
    
    if (drop_privs())
	goto err;
    
    if (res == -1) {
        int errno_save = errno;
        if (blkdev && errno == ENODEV && !fuse_mnt_check_fuseblk())
            fprintf(stderr, "%s: 'fuseblk' support missing\n", progname);
	else {
            fprintf(stderr, "%s: mount failed: %s\n", progname, strerror(errno_save));
	    if (errno_save == EPERM)
		    fprintf(stderr, "User doesn't have privilege to mount. "
			    "For more information\nplease see: "
			    "http://ntfs-3g.org/support.html#unprivileged\n");
	}
	goto err;
    } else {
        *sourcep = source;
        *typep = type;
        *mnt_optsp = mnt_opts;
    }
out:    
    free(fsname);
    free(optbuf);
    return res;
err:
    free(source);
    free(type);
    free(mnt_opts);
    res = -1;
    goto out;
}

static int check_perm(const char **mntp, struct stat *stbuf, int *currdir_fd,
                      int *mountpoint_fd)
{
    int res;
    const char *mnt = *mntp;
    const char *origmnt = mnt;

    res = stat(mnt, stbuf);
    if (res == -1) {
        fprintf(stderr, "%s: failed to access mountpoint %s: %s\n",
                progname, mnt, strerror(errno));
        return -1;
    }

    /* No permission checking is done for root */
    if (getuid() == 0)
        return 0;

    if (S_ISDIR(stbuf->st_mode)) {
        *currdir_fd = open(".", O_RDONLY);
        if (*currdir_fd == -1) {
            fprintf(stderr, "%s: failed to open current directory: %s\n",
                    progname, strerror(errno));
            return -1;
        }
        res = chdir(mnt);
        if (res == -1) {
            fprintf(stderr, "%s: failed to chdir to mountpoint: %s\n",
                    progname, strerror(errno));
            return -1;
        }
        mnt = *mntp = ".";
        res = lstat(mnt, stbuf);
        if (res == -1) {
            fprintf(stderr, "%s: failed to access mountpoint %s: %s\n",
                    progname, origmnt, strerror(errno));
            return -1;
        }

        if ((stbuf->st_mode & S_ISVTX) && stbuf->st_uid != getuid()) {
            fprintf(stderr, "%s: mountpoint %s not owned by user\n",
                    progname, origmnt);
            return -1;
        }

        res = access(mnt, W_OK);
        if (res == -1) {
            fprintf(stderr, "%s: user has no write access to mountpoint %s\n",
                    progname, origmnt);
            return -1;
        }
    } else if (S_ISREG(stbuf->st_mode)) {
        static char procfile[256];
        *mountpoint_fd = open(mnt, O_WRONLY);
        if (*mountpoint_fd == -1) {
            fprintf(stderr, "%s: failed to open %s: %s\n", progname, mnt,
                    strerror(errno));
            return -1;
        }
        res = fstat(*mountpoint_fd, stbuf);
        if (res == -1) {
            fprintf(stderr, "%s: failed to access mountpoint %s: %s\n",
                    progname, mnt, strerror(errno));
            return -1;
        }
        if (!S_ISREG(stbuf->st_mode)) {
            fprintf(stderr, "%s: mountpoint %s is no longer a regular file\n",
                    progname, mnt);
            return -1;
        }

        sprintf(procfile, "/proc/self/fd/%i", *mountpoint_fd);
        *mntp = procfile;
    } else {
        fprintf(stderr,
                "%s: mountpoint %s is not a directory or a regular file\n",
                progname, mnt);
        return -1;
    }


    return 0;
}

static int try_open(const char *dev, char **devp)
{
    int fd;
     
    if (restore_privs())
	return -1;
    fd = open(dev, O_RDWR);
    if (drop_privs())
	return -1;
    if (fd != -1) {
        *devp = strdup(dev);
        if (*devp == NULL) {
            fprintf(stderr, "%s: failed to allocate memory\n", progname);
            close(fd);
            fd = -1;
        }
    } else if (errno == ENODEV ||
               errno == ENOENT) /* check for ENOENT too, for the udev case */
        return -2;
    else {
        fprintf(stderr, "%s: failed to open %s: %s\n", progname, dev,
                strerror(errno));
    }
    return fd;
}

static int open_fuse_device(char **devp)
{
    int fd;

    fd = try_open(FUSE_DEV_NEW, devp);
    if (fd >= -1)
        return fd;

    fprintf(stderr, "%s: fuse device is missing, try 'modprobe fuse' as root\n",
            progname);

    return -1;
}


static int mount_fuse(const char *mnt, const char *opts)
{
    int res;
    int fd;
    char *dev;
    struct stat stbuf;
    char *type = NULL;
    char *source = NULL;
    char *mnt_opts = NULL;
    const char *real_mnt = mnt;
    int currdir_fd = -1;
    int mountpoint_fd = -1;

    fd = open_fuse_device(&dev);
    if (fd == -1)
        return -1;

    if (getuid() != 0 && mount_max != -1) {
        if (count_fuse_fs() >= mount_max) {
            fprintf(stderr, "%s: too many mounted FUSE filesystems (%d+)\n",
		    progname, mount_max);
	    goto err;
        }
    }

    res = check_perm(&real_mnt, &stbuf, &currdir_fd, &mountpoint_fd);
    if (res != -1)
         res = do_mount(real_mnt, &type, stbuf.st_mode & S_IFMT, fd, opts, dev,
			&source, &mnt_opts);

    if (currdir_fd != -1) {
	 __attribute__((unused))int ignored_fchdir_status =
	        fchdir(currdir_fd);
        close(currdir_fd);
    }
    if (mountpoint_fd != -1)
        close(mountpoint_fd);

    if (res == -1)
	goto err;

    if (restore_privs())
	goto err;

    if (geteuid() == 0) {
	
	if (setgroups(0, NULL) == -1) {
	    perror("priv drop: setgroups failed");
	    goto err;
	}
	    
        res = add_mount(source, mnt, type, mnt_opts);
        if (res == -1) {
            umount2(mnt, 2); /* lazy umount */
	    drop_privs();
	    goto err;
        }
    }

    if (drop_privs())
	goto err;
out:    
    free(source);
    free(type);
    free(mnt_opts);
    free(dev);

    return fd;
err:
    close(fd);
    fd = -1;
    goto out;
}

int fusermount(int unmount, int quiet, int lazy, const char *opts,
	       const char *origmnt)
{
    int res = -1;
    char *mnt;
    mode_t old_umask;

    mnt = fuse_mnt_resolve_path(progname, origmnt);
    if (mnt == NULL)
	return -1;

    old_umask = umask(033);
    
    if (unmount) {
	    
	if (restore_privs())
	    goto out;
	
        if (geteuid() == 0)
            res = fuse_mnt_umount(progname, mnt, lazy);
        else {
            res = umount2(mnt, lazy ? 2 : 0);
            if (res == -1 && !quiet)
                fprintf(stderr, "%s: failed to unmount %s: %s\n", progname,
                        mnt, strerror(errno));
        }
	
	if (drop_privs())
	    res = -1;
	
    } else
	    res = mount_fuse(mnt, opts);
out:    
    umask(old_umask);
    free(mnt);
    return res;	    
}
