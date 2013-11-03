/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/directory.h>
#include <atalk/volume.h>
#include <atalk/logger.h>
#include <atalk/unix.h>
#include <atalk/acl.h>
#include <atalk/compat.h>
#include <atalk/errchk.h>
#include <atalk/ea.h>

/* ------------------------- */
int dir_rx_set(mode_t mode)
{
    return (mode & (S_IXUSR | S_IRUSR)) == (S_IXUSR | S_IRUSR);
}

/* --------------------- */
int setfilmode(const struct vol *vol, const char *name, mode_t mode, struct stat *st)
{
    struct stat sb;
    mode_t mask = S_IRWXU | S_IRWXG | S_IRWXO;  /* rwx for owner group and other, by default */

    if (!st) {
        if (lstat(name, &sb) != 0)
            return -1;
        st = &sb;
    }

    mode |= st->st_mode & ~mask; /* keep other bits from previous mode */

    if (ochmod((char *)name, mode & ~vol->v_umask, st, vol_syml_opt(vol) | O_NETATALK_ACL) < 0 && errno != EPERM ) {
        return -1;
    }
    return 0;
}

/*
 * @brief system rmdir with afp error code.
 *
 * Supports *at semantics (cf openat) if HAVE_ATFUNCS. Pass dirfd=-1 to ignore this.
 */
int netatalk_rmdir_all_errors(int dirfd, const char *name)
{
    int err;

#ifdef HAVE_ATFUNCS
    if (dirfd == -1)
        dirfd = AT_FDCWD;
    err = unlinkat(dirfd, name, AT_REMOVEDIR);
#else
    err = rmdir(name);
#endif

    if (err < 0) {
        switch ( errno ) {
        case ENOENT :
            return AFPERR_NOOBJ;
        case ENOTEMPTY :
        case EEXIST:
            return AFPERR_DIRNEMPT;
        case EPERM:
        case EACCES :
            return AFPERR_ACCESS;
        case EROFS:
            return AFPERR_VLOCK;
        default :
            return AFPERR_PARAM;
        }
    }
    return AFP_OK;
}

/*
 * @brief System rmdir with afp error code, but ENOENT is not an error.
 *
 * Supports *at semantics (cf openat) if HAVE_ATFUNCS. Pass dirfd=-1 to ignore this.
 */
int netatalk_rmdir(int dirfd, const char *name)
{
    int ret = netatalk_rmdir_all_errors(dirfd, name);
    if (ret == AFPERR_NOOBJ)
        return AFP_OK;
    return ret;
}

/* -------------------
   system unlink with afp error code.
   ENOENT is not an error.
*/
int netatalk_unlink(const char *name)
{
    if (unlink(name) < 0) {
        switch (errno) {
        case ENOENT :
            break;
        case EROFS:
            return AFPERR_VLOCK;
        case EPERM:
        case EACCES :
            return AFPERR_ACCESS;
        default :
            return AFPERR_PARAM;
        }
    }
    return AFP_OK;
}

/**************************************************************************
 * *at semnatics support functions (like openat, renameat standard funcs)
 **************************************************************************/

/* Copy all file data from one file fd to another */
int copy_file_fd(int sfd, int dfd)
{
    EC_INIT;
    ssize_t cc;
    size_t  buflen;
    char   filebuf[NETATALK_DIOSZ_STACK];

    while ((cc = read(sfd, filebuf, sizeof(filebuf)))) {
        if (cc < 0) {
            if (errno == EINTR)
                continue;
            LOG(log_error, logtype_afpd, "copy_file_fd: %s", strerror(errno));
            EC_FAIL;
        }

        buflen = cc;
        while (buflen > 0) {
            if ((cc = write(dfd, filebuf, buflen)) < 0) {
                if (errno == EINTR)
                    continue;
                LOG(log_error, logtype_afpd, "copy_file_fd: %s", strerror(errno));
                EC_FAIL;
            }
            buflen -= cc;
        }
    }

EC_CLEANUP:
    EC_EXIT;
}

/* 
 * Supports *at semantics if HAVE_ATFUNCS, pass dirfd=-1 to ignore this
 */
int copy_file(int dirfd, const char *src, const char *dst, mode_t mode)
{
    int    ret = 0;
    int    sfd = -1;
    int    dfd = -1;

#ifdef HAVE_ATFUNCS
    if (dirfd == -1)
        dirfd = AT_FDCWD;
    sfd = openat(dirfd, src, O_RDONLY);
#else
    sfd = open(src, O_RDONLY);
#endif
    if (sfd < 0) {
        LOG(log_info, logtype_afpd, "copy_file('%s'/'%s'): open '%s' error: %s",
            src, dst, src, strerror(errno));
        return -1;
    }

    if ((dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode)) < 0) {
        LOG(log_info, logtype_afpd, "copy_file('%s'/'%s'): open '%s' error: %s",
            src, dst, dst, strerror(errno));
        ret = -1;
        goto exit;
    }

    ret = copy_file_fd(sfd, dfd);

exit:
    if (sfd != -1)
        close(sfd);

    if (dfd != -1) {
        int err;

        err = close(dfd);
        if (!ret && err) {
            /* don't bother to report an error if there's already one */
            LOG(log_error, logtype_afpd, "copy_file('%s'/'%s'): close '%s' error: %s",
                src, dst, dst, strerror(errno));
            ret = -1;
        }
    }

    return ret;
}

/*!
 * Copy an EA from one file to another
 *
 * Supports *at semantics if HAVE_ATFUNCS, pass dirfd=-1 to ignore this
 */
int copy_ea(const char *ea, int dirfd, const char *src, const char *dst, mode_t mode)
{
    EC_INIT;
    int    sfd = -1;
    int    dfd = -1;
    size_t easize;
    char   *eabuf = NULL;

#ifdef HAVE_ATFUNCS
    if (dirfd == -1)
        dirfd = AT_FDCWD;
    EC_NEG1_LOG( sfd = openat(dirfd, src, O_RDONLY) );
#else
    EC_NEG1_LOG( sfd = open(src, O_RDONLY) );
#endif
    EC_NEG1_LOG( dfd = open(dst, O_WRONLY, mode) );

    if ((easize = sys_fgetxattr(sfd, ea, NULL, 0)) > 0) {
        EC_NULL_LOG( eabuf = malloc(easize));
        EC_NEG1_LOG( easize = sys_fgetxattr(sfd, ea, eabuf, easize) );
        EC_NEG1_LOG( easize = sys_fsetxattr(dfd, ea, eabuf, easize, 0) );
    }

EC_CLEANUP:
    if (sfd != -1)
        close(sfd);
    if (dfd != -1)
        close(dfd);
    free(eabuf);
    EC_EXIT;
}

/* 
 * at wrapper for netatalk_unlink
 */
int netatalk_unlinkat(int dirfd, const char *name)
{
#ifdef HAVE_ATFUNCS
    if (dirfd == -1)
        dirfd = AT_FDCWD;

    if (unlinkat(dirfd, name, 0) < 0) {
        switch (errno) {
        case ENOENT :
            break;
        case EROFS:
            return AFPERR_VLOCK;
        case EPERM:
        case EACCES :
            return AFPERR_ACCESS;
        default :
            return AFPERR_PARAM;
        }
    }
    return AFP_OK;
#else
    return netatalk_unlink(name);
#endif

    /* DEADC0DE */
    return 0;
}

/*
 * @brief This is equivalent of unix rename()
 *
 * unix_rename mulitplexes rename and renameat. If we dont HAVE_ATFUNCS, sfd and dfd
 * are ignored.
 *
 * @param sfd        (r) if we HAVE_ATFUNCS, -1 gives AT_FDCWD
 * @param oldpath    (r) guess what
 * @param dfd        (r) same as sfd
 * @param newpath    (r) guess what
 */
int unix_rename(int sfd, const char *oldpath, int dfd, const char *newpath)
{
#ifdef HAVE_ATFUNCS
    if (sfd == -1)
        sfd = AT_FDCWD;
    if (dfd == -1)
        dfd = AT_FDCWD;

    if (renameat(sfd, oldpath, dfd, newpath) < 0)
        return -1;        
#else
    if (rename(oldpath, newpath) < 0)
        return -1;
#endif  /* HAVE_ATFUNCS */

    return 0;
}

/* 
 * @brief stat/fsstatat multiplexer
 *
 * statat mulitplexes stat and fstatat. If we dont HAVE_ATFUNCS, dirfd is ignored.
 *
 * @param dirfd   (r) Only used if HAVE_ATFUNCS, ignored else, -1 gives AT_FDCWD
 * @param path    (r) pathname
 * @param st      (rw) pointer to struct stat
 */
int statat(int dirfd, const char *path, struct stat *st)
{
#ifdef HAVE_ATFUNCS
    if (dirfd == -1)
        dirfd = AT_FDCWD;
    return (fstatat(dirfd, path, st, 0));
#else
    return (stat(path, st));
#endif            

    /* DEADC0DE */
    return -1;
}

/* 
 * @brief opendir wrapper for *at semantics support
 *
 * opendirat chdirs to dirfd if dirfd != -1 before calling opendir on path.
 *
 * @param dirfd   (r) if != -1, chdir(dirfd) before opendir(path)
 * @param path    (r) pathname
 */
DIR *opendirat(int dirfd, const char *path)
{
    DIR *ret;
    int cwd = -1;

    if (dirfd != -1) {
        if (((cwd = open(".", O_RDONLY)) == -1) || (fchdir(dirfd) != 0)) {
            ret = NULL;
            goto exit;
        }
    }

    ret = opendir(path);

    if (dirfd != -1 && fchdir(cwd) != 0) {
        LOG(log_error, logtype_afpd, "opendirat: cant chdir back. exit!");
        exit(EXITERR_SYS);
    }

exit:
    if (cwd != -1)
        close(cwd);

    return ret;
}
