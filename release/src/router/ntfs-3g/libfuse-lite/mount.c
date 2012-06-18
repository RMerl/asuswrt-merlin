/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU LGPLv2.
    See the file COPYING.LIB.
*/

#include "config.h"
#include "fuse_i.h"
#include "fuse_opt.h"
#include "mount_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/mount.h>

#ifndef MS_DIRSYNC
#define MS_DIRSYNC 128
#endif

enum {
    KEY_KERN_FLAG,
    KEY_KERN_OPT,
    KEY_FUSERMOUNT_OPT,
    KEY_SUBTYPE_OPT,
    KEY_MTAB_OPT,
    KEY_ALLOW_ROOT,
    KEY_RO,
    KEY_HELP,
    KEY_VERSION,
};

struct mount_opts {
    int allow_other;
    int allow_root;
    int ishelp;
    int flags;
    int blkdev;
    char *fsname;
    char *mtab_opts;
    char *fusermount_opts;
    char *kernel_opts;
};

#define FUSE_MOUNT_OPT(t, p) { t, offsetof(struct mount_opts, p), 1 }

static const struct fuse_opt fuse_mount_opts[] = {
    FUSE_MOUNT_OPT("allow_other",       allow_other),
    FUSE_MOUNT_OPT("allow_root",        allow_root),
    FUSE_MOUNT_OPT("blkdev",            blkdev),
    FUSE_MOUNT_OPT("fsname=%s",         fsname),
    FUSE_OPT_KEY("allow_other",         KEY_KERN_OPT),
    FUSE_OPT_KEY("allow_root",          KEY_ALLOW_ROOT),
    FUSE_OPT_KEY("blkdev",              KEY_FUSERMOUNT_OPT),
    FUSE_OPT_KEY("fsname=",             KEY_FUSERMOUNT_OPT),
    FUSE_OPT_KEY("large_read",          KEY_KERN_OPT),
    FUSE_OPT_KEY("blksize=",            KEY_KERN_OPT),
    FUSE_OPT_KEY("default_permissions", KEY_KERN_OPT),
    FUSE_OPT_KEY("context=",            KEY_KERN_OPT),
    FUSE_OPT_KEY("max_read=",           KEY_KERN_OPT),
    FUSE_OPT_KEY("max_read=",           FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("user=",               KEY_MTAB_OPT),
    FUSE_OPT_KEY("-r",                  KEY_RO),
    FUSE_OPT_KEY("ro",                  KEY_KERN_FLAG),
    FUSE_OPT_KEY("rw",                  KEY_KERN_FLAG),
    FUSE_OPT_KEY("suid",                KEY_KERN_FLAG),
    FUSE_OPT_KEY("nosuid",              KEY_KERN_FLAG),
    FUSE_OPT_KEY("dev",                 KEY_KERN_FLAG),
    FUSE_OPT_KEY("nodev",               KEY_KERN_FLAG),
    FUSE_OPT_KEY("exec",                KEY_KERN_FLAG),
    FUSE_OPT_KEY("noexec",              KEY_KERN_FLAG),
    FUSE_OPT_KEY("async",               KEY_KERN_FLAG),
    FUSE_OPT_KEY("sync",                KEY_KERN_FLAG),
    FUSE_OPT_KEY("dirsync",             KEY_KERN_FLAG),
    FUSE_OPT_KEY("atime",               KEY_KERN_FLAG),
    FUSE_OPT_KEY("noatime",             KEY_KERN_FLAG),
    FUSE_OPT_KEY("-h",                  KEY_HELP),
    FUSE_OPT_KEY("--help",              KEY_HELP),
    FUSE_OPT_KEY("-V",                  KEY_VERSION),
    FUSE_OPT_KEY("--version",           KEY_VERSION),
    FUSE_OPT_END
};

struct mount_flags {
    const char *opt;
    unsigned long flag;
    int on;
};

static struct mount_flags mount_flags[] = {
    {"rw",      MS_RDONLY,      0},
    {"ro",      MS_RDONLY,      1},
    {"suid",    MS_NOSUID,      0},
    {"nosuid",  MS_NOSUID,      1},
    {"dev",     MS_NODEV,       0},
    {"nodev",   MS_NODEV,       1},
    {"exec",    MS_NOEXEC,      0},
    {"noexec",  MS_NOEXEC,      1},
    {"async",   MS_SYNCHRONOUS, 0},
    {"sync",    MS_SYNCHRONOUS, 1},
    {"atime",   MS_NOATIME,     0},
    {"noatime", MS_NOATIME,     1},
    {"dirsync", MS_DIRSYNC,     1},
    {NULL,      0,              0}
};

static void set_mount_flag(const char *s, int *flags)
{
    int i;

    for (i = 0; mount_flags[i].opt != NULL; i++) {
        const char *opt = mount_flags[i].opt;
        if (strcmp(opt, s) == 0) {
            if (mount_flags[i].on)
                *flags |= mount_flags[i].flag;
            else
                *flags &= ~mount_flags[i].flag;
            return;
        }
    }
    fprintf(stderr, "fuse: internal error, can't find mount flag\n");
    abort();
}

static int fuse_mount_opt_proc(void *data, const char *arg, int key,
                               struct fuse_args *outargs)
{
    struct mount_opts *mo = data;

    switch (key) {
    case KEY_ALLOW_ROOT:
        if (fuse_opt_add_opt(&mo->kernel_opts, "allow_other") == -1 ||
            fuse_opt_add_arg(outargs, "-oallow_root") == -1)
            return -1;
        return 0;

    case KEY_RO:
        arg = "ro";
        /* fall through */
    case KEY_KERN_FLAG:
        set_mount_flag(arg, &mo->flags);
        return 0;

    case KEY_KERN_OPT:
        return fuse_opt_add_opt(&mo->kernel_opts, arg);

    case KEY_FUSERMOUNT_OPT:
        return fuse_opt_add_opt(&mo->fusermount_opts, arg);

    case KEY_MTAB_OPT:
        return fuse_opt_add_opt(&mo->mtab_opts, arg);

    case KEY_HELP:
        mo->ishelp = 1;
        break;

    case KEY_VERSION:
        mo->ishelp = 1;
        break;
    }
    return 1;
}

void fuse_kern_unmount(const char *mountpoint, int fd)
{
    int res;

    if (!mountpoint)
        return;

    if (fd != -1) {
        struct pollfd pfd;

        pfd.fd = fd;
        pfd.events = 0;
        res = poll(&pfd, 1, 0);
        /* If file poll returns POLLERR on the device file descriptor,
           then the filesystem is already unmounted */
        if (res == 1 && (pfd.revents & POLLERR))
            return;
    }
    close(fd);

    fusermount(1, 0, 1, "", mountpoint);
}

static int get_mnt_flag_opts(char **mnt_optsp, int flags)
{
    int i;

    if (!(flags & MS_RDONLY) && fuse_opt_add_opt(mnt_optsp, "rw") == -1)
        return -1;

    for (i = 0; mount_flags[i].opt != NULL; i++) {
        if (mount_flags[i].on && (flags & mount_flags[i].flag) &&
            fuse_opt_add_opt(mnt_optsp, mount_flags[i].opt) == -1)
            return -1;
    }
    return 0;
}

int fuse_kern_mount(const char *mountpoint, struct fuse_args *args)
{
    struct mount_opts mo;
    int res = -1;
    char *mnt_opts = NULL;

    memset(&mo, 0, sizeof(mo));
    if (getuid())
	    mo.flags = MS_NOSUID | MS_NODEV;

    if (args &&
        fuse_opt_parse(args, &mo, fuse_mount_opts, fuse_mount_opt_proc) == -1)
        return -1;

    if (mo.allow_other && mo.allow_root) {
        fprintf(stderr, "fuse: 'allow_other' and 'allow_root' options are mutually exclusive\n");
        goto out;
    }
    res = 0;
    if (mo.ishelp)
        goto out;

    res = -1;
    if (get_mnt_flag_opts(&mnt_opts, mo.flags) == -1)
        goto out;
    if (!(mo.flags & MS_NODEV) && fuse_opt_add_opt(&mnt_opts, "dev") == -1)
        goto out;
    if (!(mo.flags & MS_NOSUID) && fuse_opt_add_opt(&mnt_opts, "suid") == -1)
        goto out;
    if (mo.kernel_opts && fuse_opt_add_opt(&mnt_opts, mo.kernel_opts) == -1)
        goto out;
    if (mo.mtab_opts &&  fuse_opt_add_opt(&mnt_opts, mo.mtab_opts) == -1)
        goto out;
    if (mo.fusermount_opts && fuse_opt_add_opt(&mnt_opts, mo.fusermount_opts) < 0)
        goto out;

    res = fusermount(0, 0, 0, mnt_opts ? mnt_opts : "", mountpoint);
    
out:
    free(mnt_opts);
    free(mo.fsname);
    free(mo.fusermount_opts);
    free(mo.kernel_opts);
    free(mo.mtab_opts);
    return res;
}

