/* 
   Unix SMB/Netbios implementation.
   Version 2.0
   SMB wrapper functions
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Derrell Lipman 2002-2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * This is a rewrite of the original wrapped.c file, using libdl to obtain
 * pointers into the C library rather than attempting to find undocumented
 * functions in the C library to call for native file access.  The problem
 * with the original implementation's paradigm is that samba manipulates
 * defines such that it gets the sizes of structures that it wants
 * (e.g. mapping 32-bit functions to 64-bit functions with their associated
 * 64-bit structure fields), but programs run under smbsh or using
 * smbwrapper.so were not necessarily compiled with the same flags.  As an
 * example of the problem, a program calling stat() passes a pointer to a
 * "struct stat" but the fields in that structure are different in samba than
 * they are in the calling program if the calling program was not compiled to
 * force stat() to be mapped to stat64().
 *
 * In this version, we provide an interface to each of the native functions,
 * not just the ones that samba is compiled to map to.  We obtain the function
 * pointers from the C library using dlsym(), and for native file operations,
 * directly call the same function that the calling application was
 * requesting.  Since the size of the calling application's structures vary
 * depending on what function was called, we use our own internal structures
 * for passing information to/from the SMB equivalent functions, and map them
 * back to the native structures before returning the result to the caller.
 *
 * This implementation was completed 25 December 2002.
 * Derrell Lipman
 */

/* We do not want auto munging of 32->64 bit names in this file (only) */
#undef _FILE_OFFSET_BITS
#undef _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <dirent.h>
#include <signal.h>
#include <stdarg.h>
#ifdef __USE_GNU
# define SMBW_USE_GNU
#endif
#define __USE_GNU             /* need this to have RTLD_NEXT defined */
#include <dlfcn.h>
#ifndef SMBW_USE_GNU
# undef __USE_GNU
#endif
#include <errno.h>
#include "libsmbclient.h"
#include "bsd-strlfunc.h"
#include "wrapper.h"

/*
 * Debug bits:
 * 0x0 = none
 * 0x1 = display symbol definitions not found in C library
 * 0x2 = show wrapper functions being called
 * 0x4 = log to file SMBW_DEBUG_FILE instead of stderr
 */
#define SMBW_DEBUG      0x0
#define SMBW_DEBUG_FILE "/tmp/smbw.log"

int      smbw_debug = 0;

#if SMBW_DEBUG & 0x2
static int      debugFd = 2;
#endif

#ifndef ENOTSUP
#define ENOTSUP EOPNOTSUPP
#endif

/*
 * None of the methods of having the initialization function called
 * automatically upon shared library startup are effective in all situations.
 * We provide the "-init" parameter to the linker which is effective most of
 * the time, but fails for applications that provide their own shared
 * libraries with _init() functions (e.g. ps).  We can't use "-z initfirst"
 * because the environment isn't yet set up at that point, so we can't find
 * our shared memory identifier (see shared.c).  We therefore must resort to
 * this tried-and-true method of keeping an "initialized" flag.  We check it
 * prior to calling the initialize() function to save a function call (a slow
 * operation on x86).
 */
#if SMBW_DEBUG & 0x2
#  define check_init(buf)                                               \
        do {                                                            \
                int saved_errno = errno;                                \
                if (! initialized) initialize();                        \
                (* smbw_libc.write)(debugFd, "["buf"]", sizeof(buf)+1); \
                errno = saved_errno;                                    \
        } while (0)
#else
#  define check_init(buf)                               \
        do {                                            \
                if (! initialized) smbw_initialize();   \
        } while (0)
#endif

static void initialize(void);

static int initialized = 0;

SMBW_libc_pointers smbw_libc;

/*
 * A public entry point used by the "-init" option to the linker.
 */
void smbw_initialize(void)
{
        initialize();
}

static void initialize(void)
{
        int saved_errno;
#if SMBW_DEBUG & 0x1
        char *error;
#endif
        
        saved_errno = errno;
        
        if (initialized) {
                return;
        }
        initialized = 1;
        
#if SMBW_DEBUG & 0x1
# define GETSYM(symname, symstring)                                      \
        if ((smbw_libc.symname = dlsym(RTLD_NEXT, symstring)) == NULL) { \
                if (smbw_libc.write != NULL &&                           \
                    (error = dlerror()) != NULL) {                       \
                        (* smbw_libc.write)(1, error, strlen(error));    \
                        (* smbw_libc.write)(1, "\n", 1);                 \
                }                                                        \
        }
#else
# define GETSYM(symname, symstring)                     \
        smbw_libc.symname = dlsym(RTLD_NEXT, symstring);
#endif
        
        /*
         * Get pointers to each of the symbols we'll need, from the C library
         *
         * Some of these symbols may not be found in the C library.  That's
         * fine.  We declare all of them here, and if the C library supports
         * them, they may be called so we have the wrappers for them.  If the
         * C library doesn't support them, then the wrapper function will
         * never be called, and the null pointer will never be dereferenced.
         */
        GETSYM(write, "write"); /* first, to allow debugging */
        GETSYM(open, "open");
        GETSYM(_open, "_open");
        GETSYM(__open, "__open");
        GETSYM(open64, "open64");
        GETSYM(_open64, "_open64");
        GETSYM(__open64, "__open64");
        GETSYM(pread, "pread");
        GETSYM(pread64, "pread64");
        GETSYM(pwrite, "pwrite");
        GETSYM(pwrite64, "pwrite64");
        GETSYM(close, "close");
        GETSYM(__close, "__close");
        GETSYM(_close, "_close");
        GETSYM(fcntl, "fcntl");
        GETSYM(__fcntl, "__fcntl");
        GETSYM(_fcntl, "_fcntl");
        GETSYM(getdents, "getdents");
        GETSYM(__getdents, "__getdents");
        GETSYM(_getdents, "_getdents");
        GETSYM(getdents64, "getdents64");
        GETSYM(lseek, "lseek");
        GETSYM(__lseek, "__lseek");
        GETSYM(_lseek, "_lseek");
        GETSYM(lseek64, "lseek64");
        GETSYM(__lseek64, "__lseek64");
        GETSYM(_lseek64, "_lseek64");
        GETSYM(read, "read");
        GETSYM(__read, "__read");
        GETSYM(_read, "_read");
        GETSYM(__write, "__write");
        GETSYM(_write, "_write");
        GETSYM(access, "access");
        GETSYM(chmod, "chmod");
        GETSYM(fchmod, "fchmod");
        GETSYM(chown, "chown");
        GETSYM(fchown, "fchown");
        GETSYM(__xstat, "__xstat");
        GETSYM(getcwd, "getcwd");
        GETSYM(mkdir, "mkdir");
        GETSYM(__fxstat, "__fxstat");
        GETSYM(__lxstat, "__lxstat");
        GETSYM(stat, "stat");
        GETSYM(lstat, "lstat");
        GETSYM(fstat, "fstat");
        GETSYM(unlink, "unlink");
        GETSYM(utime, "utime");
        GETSYM(utimes, "utimes");
        GETSYM(readlink, "readlink");
        GETSYM(rename, "rename");
        GETSYM(rmdir, "rmdir");
        GETSYM(symlink, "symlink");
        GETSYM(dup, "dup");
        GETSYM(dup2, "dup2");
        GETSYM(opendir, "opendir");
        GETSYM(readdir, "readdir");
        GETSYM(closedir, "closedir");
        GETSYM(telldir, "telldir");
        GETSYM(seekdir, "seekdir");
        GETSYM(creat, "creat");
        GETSYM(creat64, "creat64");
        GETSYM(__xstat64, "__xstat64");
        GETSYM(stat64, "stat64");
        GETSYM(__fxstat64, "__fxstat64");
        GETSYM(fstat64, "fstat64");
        GETSYM(__lxstat64, "__lxstat64");
        GETSYM(lstat64, "lstat64");
        GETSYM(_llseek, "_llseek");
        GETSYM(readdir64, "readdir64");
        GETSYM(readdir_r, "readdir_r");
        GETSYM(readdir64_r, "readdir64_r");
        GETSYM(setxattr, "setxattr");
        GETSYM(lsetxattr, "lsetxattr");
        GETSYM(fsetxattr, "fsetxattr");
        GETSYM(getxattr, "getxattr");
        GETSYM(lgetxattr, "lgetxattr");
        GETSYM(fgetxattr, "fgetxattr");
        GETSYM(removexattr, "removexattr");
        GETSYM(lremovexattr, "lremovexattr");
        GETSYM(fremovexattr, "fremovexattr");
        GETSYM(listxattr, "listxattr");
        GETSYM(llistxattr, "llistxattr");
        GETSYM(flistxattr, "flistxattr");
        GETSYM(chdir, "chdir");
        GETSYM(fchdir, "fchdir");
        GETSYM(fork, "fork");
        GETSYM(select, "select");
        GETSYM(_select, "_select");
        GETSYM(__select, "__select");
        
#if SMBW_DEBUG & 4
        {
                if ((debugFd =
                     open(SMBW_DEBUG_FILE, O_WRONLY | O_CREAT | O_APPEND)) < 0)
                {
#               define SMBW_MESSAGE    "Could not create " SMBW_DEBUG_FILE "\n" 
                        (* smbw_libc.write)(1, SMBW_MESSAGE, sizeof(SMBW_MESSAGE));
#               undef SMBW_MESSAGE
                        exit(1);
                }
        }
#endif
        
        errno = saved_errno;
}

/**
 ** Static Functions
 **/

static void stat_convert(struct SMBW_stat *src, struct stat *dest)
{
        memset(dest, '\0', sizeof(*dest));
	dest->st_size = src->s_size;
	dest->st_mode = src->s_mode;
	dest->st_ino = src->s_ino;
	dest->st_dev = src->s_dev;
	dest->st_rdev = src->s_rdev;
	dest->st_nlink = src->s_nlink;
	dest->st_uid = src->s_uid;
	dest->st_gid = src->s_gid;
	dest->st_atime = src->s_atime;
	dest->st_mtime = src->s_mtime;
	dest->st_ctime = src->s_ctime;
	dest->st_blksize = src->s_blksize;
	dest->st_blocks = src->s_blocks;
}

static void stat64_convert(struct SMBW_stat *src, struct stat64 *dest)
{
        memset(dest, '\0', sizeof(*dest));
	dest->st_size = src->s_size;
	dest->st_mode = src->s_mode;
	dest->st_ino = src->s_ino;
	dest->st_dev = src->s_dev;
	dest->st_rdev = src->s_rdev;
	dest->st_nlink = src->s_nlink;
	dest->st_uid = src->s_uid;
	dest->st_gid = src->s_gid;
	dest->st_atime = src->s_atime;
	dest->st_mtime = src->s_mtime;
	dest->st_ctime = src->s_ctime;
	dest->st_blksize = src->s_blksize;
	dest->st_blocks = src->s_blocks;
}

static void dirent_convert(struct SMBW_dirent *src, struct dirent *dest)
{
        char *p;
        
        memset(dest, '\0', sizeof(*dest));
	dest->d_ino = src->d_ino;
	dest->d_off = src->d_off;
        
        switch(src->d_type)
        {
        case SMBC_WORKGROUP:
        case SMBC_SERVER:
        case SMBC_FILE_SHARE:
        case SMBC_DIR:
                dest->d_type = DT_DIR;
                break;
                
        case SMBC_FILE:
                dest->d_type = DT_REG;
                break;
                
        case SMBC_PRINTER_SHARE:
                dest->d_type = DT_CHR;
                break;
                
        case SMBC_COMMS_SHARE:
                dest->d_type = DT_SOCK;
                break;
                
        case SMBC_IPC_SHARE:
                dest->d_type = DT_FIFO;
                break;
                
        case SMBC_LINK:
                dest->d_type = DT_LNK;
                break;
        }
        
	dest->d_reclen = src->d_reclen;
	smbw_strlcpy(dest->d_name, src->d_name, sizeof(dest->d_name));
        p = dest->d_name + strlen(dest->d_name) + 1;
        smbw_strlcpy(p,
                     src->d_comment,
                     sizeof(dest->d_name) - (p - dest->d_name));
}

static void dirent64_convert(struct SMBW_dirent *src, struct dirent64 *dest)
{
        char *p;
        
        memset(dest, '\0', sizeof(*dest));
	dest->d_ino = src->d_ino;
	dest->d_off = src->d_off;
        
        switch(src->d_type)
        {
        case SMBC_WORKGROUP:
        case SMBC_SERVER:
        case SMBC_FILE_SHARE:
        case SMBC_DIR:
                dest->d_type = DT_DIR;
                break;
                
        case SMBC_FILE:
                dest->d_type = DT_REG;
                break;
                
        case SMBC_PRINTER_SHARE:
                dest->d_type = DT_CHR;
                break;
                
        case SMBC_COMMS_SHARE:
                dest->d_type = DT_SOCK;
                break;
                
        case SMBC_IPC_SHARE:
                dest->d_type = DT_FIFO;
                break;
                
        case SMBC_LINK:
                dest->d_type = DT_LNK;
                break;
        }
        
	dest->d_reclen = src->d_reclen;
	smbw_strlcpy(dest->d_name, src->d_name, sizeof(dest->d_name));
        p = dest->d_name + strlen(dest->d_name) + 1;
        smbw_strlcpy(p,
                     src->d_comment,
                     sizeof(dest->d_name) - (p - dest->d_name));
}

static int openx(char *name, int flags, mode_t mode, int (* f)(char *, int, mode_t))
{
	if (smbw_path(name)) {
		return smbw_open(name, flags, mode);
	}
        
        return (* f)(name, flags, mode);
}

static int closex(int fd, int (* f)(int fd))
{
	if (smbw_fd(fd)) {
		return smbw_close(fd);
	}
        
        return (* f)(fd);
}

static int fcntlx(int fd, int cmd, long arg, int (* f)(int, int, long))
{
	if (smbw_fd(fd)) {
		return smbw_fcntl(fd, cmd, arg);
	}
        
        return (* f)(fd, cmd, arg);
}

static int getdentsx(int fd, struct dirent *external, unsigned int count, int (* f)(int, struct dirent *, unsigned int))
{
	if (smbw_fd(fd)) {
                int i;
                int internal_count;
                struct SMBW_dirent *internal;
                int ret;
                int n;
                
                /*
                 * LIMITATION: If they pass a count which is not a multiple of
                 * the size of struct dirent, they will not get a partial
                 * structure; we ignore the excess count.
                 */
                n = (count / sizeof(struct dirent));
                
                internal_count = sizeof(struct SMBW_dirent) * n;
                internal = malloc(internal_count);
                if (internal == NULL) {
                        errno = ENOMEM;
                        return -1;
                }
		ret = smbw_getdents(fd, internal, internal_count);
                if (ret <= 0)
                        return ret;
                
                ret = sizeof(struct dirent) * n;
                
                for (i = 0; i < n; i++)
                        dirent_convert(&internal[i], &external[i]);
                
                return ret;
	}
        
        return (* f)(fd, external, count);
}

static off_t lseekx(int fd,
                    off_t offset,
                    int whence,
                    off_t (* f)(int, off_t, int))
{
        off_t           ret;
        
        /*
         * We have left the definitions of the smbw_ functions undefined,
         * because types such as off_t can differ in meaning betweent his
         * function and smbw.c et al.  Functions that return other than an
         * integer value, however, MUST have their return value defined.
         */
        off64_t         smbw_lseek();
        
        if (smbw_fd(fd)) {
		return (off_t) smbw_lseek(fd, offset, whence);
	}
        
        ret = (* f)(fd, offset, whence);
        if (smbw_debug)
        {
                printf("lseekx(%d, 0x%llx) returned 0x%llx\n",
                       fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

static off64_t lseek64x(int fd,
                        off64_t offset,
                        int whence,
                        off64_t (* f)(int, off64_t, int))
{
        off64_t         ret;
        
        /*
         * We have left the definitions of the smbw_ functions undefined,
         * because types such as off_t can differ in meaning betweent his
         * function and smbw.c et al.  Functions that return other than an
         * integer value, however, MUST have their return value defined.
         */
        off64_t         smbw_lseek();
        
	if (smbw_fd(fd))
		ret = smbw_lseek(fd, offset, whence);
        else
                ret = (* f)(fd, offset, whence);
        if (smbw_debug)
        {
                printf("lseek64x(%d, 0x%llx) returned 0x%llx\n",
                       fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

static ssize_t readx(int fd, void *buf, size_t count, ssize_t (* f)(int, void *, size_t))
{
	if (smbw_fd(fd)) {
		return smbw_read(fd, buf, count);
	}
        
        return (* f)(fd, buf, count);
}

static ssize_t writex(int fd, void *buf, size_t count, ssize_t (* f)(int, void *, size_t))
{
	if (smbw_fd(fd)) {
		return smbw_write(fd, buf, count);
	}
        
        return (* f)(fd, buf, count);
}


/**
 ** Wrapper Functions
 **/

int open(__const char *name, int flags, ...)
{
        va_list ap;
        mode_t mode;
        
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        
        check_init("open");
        
        return openx((char *) name, flags, mode, smbw_libc.open);
}

int _open(char *name, int flags, mode_t mode)
{
        check_init("open");
        
        return openx(name, flags, mode, smbw_libc._open);
}

int __open(char *name, int flags, mode_t mode)
{
        check_init("open");
        
        return openx(name, flags, mode, smbw_libc.__open);
}

int open64 (__const char *name, int flags, ...)
{
        va_list ap;
        mode_t mode;
        
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        
        check_init("open64");
        return openx((char *) name, flags, mode, smbw_libc.open64);
}

int _open64(char *name, int flags, mode_t mode)
{
        check_init("_open64");
        return openx(name, flags, mode, smbw_libc._open64);
}

int __open64(char *name, int flags, mode_t mode)
{
        check_init("__open64");
        return openx(name, flags, mode, smbw_libc.__open64);
}

ssize_t pread(int fd, void *buf, size_t size, off_t ofs)
{
        check_init("pread");
        
	if (smbw_fd(fd)) {
		return smbw_pread(fd, buf, size, ofs);
	}
        
        return (* smbw_libc.pread)(fd, buf, size, ofs);
}

ssize_t pread64(int fd, void *buf, size_t size, off64_t ofs)
{
        check_init("pread64");
        
	if (smbw_fd(fd)) {
		return smbw_pread(fd, buf, size, (off_t) ofs);
	}
        
        return (* smbw_libc.pread64)(fd, buf, size, ofs);
}

ssize_t pwrite(int fd, const void *buf, size_t size, off_t ofs)
{
        check_init("pwrite");
        
	if (smbw_fd(fd)) {
		return smbw_pwrite(fd, (void *) buf, size, ofs);
	}
        
        return (* smbw_libc.pwrite)(fd, (void *) buf, size, ofs);
}

ssize_t pwrite64(int fd,  const void *buf, size_t size, off64_t ofs)
{
        check_init("pwrite64");
        
	if (smbw_fd(fd)) {
		return smbw_pwrite(fd, (void *) buf, size, (off_t) ofs);
	}
        
        return (* smbw_libc.pwrite64)(fd, (void *) buf, size, ofs);
}

int chdir(const char *name)
{
        check_init("chdir");
        return smbw_chdir((char *) name);;
}

int __chdir(char *name)
{
        check_init("__chdir");
        return smbw_chdir(name);
}

int _chdir(char *name)
{
        check_init("_chdir");
        return smbw_chdir(name);
}

int close(int fd)
{
        check_init("close");
        return closex(fd, smbw_libc.close);
}

int __close(int fd)
{
        check_init("__close");
        return closex(fd, smbw_libc.__close);
}

int _close(int fd)
{
        check_init("_close");
        return closex(fd, smbw_libc._close);
}

int fchdir(int fd)
{
        check_init("fchdir");
	return smbw_fchdir(fd);
}

int __fchdir(int fd)
{
        check_init("__fchdir");
	return fchdir(fd);
}

int _fchdir(int fd)
{
        check_init("_fchdir");
	return fchdir(fd);
}

int fcntl (int fd, int cmd, ...)
{
        va_list ap;
        long arg;
        
        va_start(ap, cmd);
        arg = va_arg(ap, long);
        va_end(ap);
        
        check_init("fcntl");
        return fcntlx(fd, cmd, arg, smbw_libc.fcntl);
}

int __fcntl(int fd, int cmd, ...)
{
        va_list ap;
        long arg;
        
        va_start(ap, cmd);
        arg = va_arg(ap, long);
        va_end(ap);
        
        check_init("__fcntl");
        return fcntlx(fd, cmd, arg, smbw_libc.__fcntl);
}

int _fcntl(int fd, int cmd, ...)
{
        va_list ap;
        long arg;
        
        va_start(ap, cmd);
        arg = va_arg(ap, long);
        va_end(ap);
        
        check_init("_fcntl");
        return fcntlx(fd, cmd, arg, smbw_libc._fcntl);
}

int getdents(int fd, struct dirent *dirp, unsigned int count)
{
        check_init("getdents");
        return getdentsx(fd, dirp, count, smbw_libc.getdents);
}

int __getdents(int fd, struct dirent *dirp, unsigned int count)
{
        check_init("__getdents");
        return getdentsx(fd, dirp, count, smbw_libc.__getdents);
}

int _getdents(int fd, struct dirent *dirp, unsigned int count)
{
        check_init("_getdents");
        return getdentsx(fd, dirp, count, smbw_libc._getdents);
}

int getdents64(int fd, struct dirent64 *external, unsigned int count)
{
        check_init("getdents64");
	if (smbw_fd(fd)) {
                int i;
                struct SMBW_dirent *internal;
                int ret;
                int n;
                
                /*
                 * LIMITATION: If they pass a count which is not a multiple of
                 * the size of struct dirent, they will not get a partial
                 * structure; we ignore the excess count.
                 */
                n = (count / sizeof(struct dirent64));
                
                internal = malloc(sizeof(struct SMBW_dirent) * n);
                if (internal == NULL) {
                        errno = ENOMEM;
                        return -1;
                }
		ret = smbw_getdents(fd, internal, count);
                if (ret <= 0)
                        return ret;
                
                ret = sizeof(struct dirent) * count;
                
                for (i = 0; count; i++, count--)
                        dirent64_convert(&internal[i], &external[i]);
                
                return ret;
	}
        
        return (* smbw_libc.getdents64)(fd, external, count);
}

off_t lseek(int fd, off_t offset, int whence)
{
        off_t           ret;
        check_init("lseek");
        ret = lseekx(fd, offset, whence, smbw_libc.lseek);
        if (smbw_debug)
        {
                printf("lseek(%d, 0x%llx) returned 0x%llx\n",
                       fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

off_t __lseek(int fd, off_t offset, int whence)
{
        off_t           ret;
        check_init("__lseek");
        ret = lseekx(fd, offset, whence, smbw_libc.__lseek);
        if (smbw_debug)
        {
                printf("__lseek(%d, 0x%llx) returned 0x%llx\n",
                       fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

off_t _lseek(int fd, off_t offset, int whence)
{
        off_t           ret;
        check_init("_lseek");
        ret = lseekx(fd, offset, whence, smbw_libc._lseek);
        if (smbw_debug)
        {
                printf("_lseek(%d, 0x%llx) returned 0x%llx\n",
                       fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

off64_t lseek64(int fd, off64_t offset, int whence)
{
        off64_t         ret;
        check_init("lseek64");
        ret = lseek64x(fd, offset, whence, smbw_libc.lseek64);
        if (smbw_debug)
        {
                printf("lseek64(%d, 0x%llx) returned 0x%llx\n",
                       fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

off64_t __lseek64(int fd, off64_t offset, int whence)
{
        check_init("__lseek64");
        return lseek64x(fd, offset, whence, smbw_libc.__lseek64);
}

off64_t _lseek64(int fd, off64_t offset, int whence)
{
        off64_t         ret;
        check_init("_lseek64");
        ret = lseek64x(fd, offset, whence, smbw_libc._lseek64);
        if (smbw_debug)
        {
                printf("_lseek64(%d, 0x%llx) returned 0x%llx\n",
                       fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

ssize_t read(int fd, void *buf, size_t count)
{
        check_init("read");
        return readx(fd, buf, count, smbw_libc.read);
}

ssize_t __read(int fd, void *buf, size_t count)
{
        check_init("__read");
        return readx(fd, buf, count, smbw_libc.__read);
}

ssize_t _read(int fd, void *buf, size_t count)
{
        check_init("_read");
        return readx(fd, buf, count, smbw_libc._read);
}

ssize_t write(int fd, const void *buf, size_t count)
{
        check_init("write");
        return writex(fd, (void *) buf, count, smbw_libc.write);
}

ssize_t __write(int fd, const void *buf, size_t count)
{
        check_init("__write");
        return writex(fd, (void *) buf, count, smbw_libc.__write);
}

ssize_t _write(int fd, const void *buf, size_t count)
{
        check_init("_write");
        return writex(fd, (void *) buf, count, smbw_libc._write);
}

int access(const char *name, int mode)
{
        check_init("access");
        
	if (smbw_path((char *) name)) {
		return smbw_access((char *) name, mode);
	}
        
        return (* smbw_libc.access)((char *) name, mode);
}

int chmod(const char *name, mode_t mode)
{
        check_init("chmod");
        
	if (smbw_path((char *) name)) {
		return smbw_chmod((char *) name, mode);
	}
        
        return (* smbw_libc.chmod)((char *) name, mode);
}

int fchmod(int fd, mode_t mode)
{
        check_init("fchmod");
        
	if (smbw_fd(fd)) {
                /* Not yet implemented in libsmbclient */
                return ENOTSUP;
	}
        
        return (* smbw_libc.fchmod)(fd, mode);
}

int chown(const char *name, uid_t owner, gid_t group)
{
        check_init("chown");
        
	if (smbw_path((char *) name)) {
		return smbw_chown((char *) name, owner, group);
	}
        
        return (* smbw_libc.chown)((char *) name, owner, group);
}

int fchown(int fd, uid_t owner, gid_t group)
{
        check_init("fchown");
        
	if (smbw_fd(fd)) {
                /* Not yet implemented in libsmbclient */
                return ENOTSUP;
	}
        
        return (* smbw_libc.fchown)(fd, owner, group);
}

char *getcwd(char *buf, size_t size)
{
        check_init("getcwd");
	return (char *)smbw_getcwd(buf, size);
}

int mkdir(const char *name, mode_t mode)
{
        check_init("mkdir");
        
	if (smbw_path((char *) name)) {
		return smbw_mkdir((char *) name, mode);
	}
        
        return (* smbw_libc.mkdir)((char *) name, mode);
}

int __fxstat(int vers, int fd, struct stat *st)
{
        check_init("__fxstat");
        
	if (smbw_fd(fd)) {
                struct SMBW_stat statbuf;
		int ret = smbw_fstat(fd, &statbuf);
                stat_convert(&statbuf, st);
                return ret;
	}
        
        return (* smbw_libc.__fxstat)(vers, fd, st);
}

int __xstat(int vers, const char *name, struct stat *st)
{
        check_init("__xstat");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
		int ret = smbw_stat((char *) name, &statbuf);
                stat_convert(&statbuf, st);
                return ret;
	}
        
        return (* smbw_libc.__xstat)(vers, (char *) name, st);
}

int __lxstat(int vers, const char *name, struct stat *st)
{
        check_init("__lxstat");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
		int ret = smbw_stat((char *) name, &statbuf);
                stat_convert(&statbuf, st);
                return ret;
	}
        
        return (* smbw_libc.__lxstat)(vers, (char *) name, st);
}

int stat(const char *name, struct stat *st)
{
        check_init("stat");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
		int ret = smbw_stat((char *) name, &statbuf);
                stat_convert(&statbuf, st);
                return ret;
	}
        
        return (* smbw_libc.stat)((char *) name, st);
}

int lstat(const char *name, struct stat *st)
{
        check_init("lstat");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
                int ret = smbw_stat((char *) name, &statbuf);
                stat_convert(&statbuf, st);
                return ret;
	}
        
        return (* smbw_libc.lstat)((char *) name, st);
}

int fstat(int fd, struct stat *st)
{
        check_init("fstat");
        
	if (smbw_fd(fd)) {
                struct SMBW_stat statbuf;
		int ret = smbw_fstat(fd, &statbuf);
                stat_convert(&statbuf, st);
                return ret;
	}
        
        return (* smbw_libc.fstat)(fd, st);
}

int unlink(const char *name)
{
        check_init("unlink");
        
	if (smbw_path((char *) name)) {
		return smbw_unlink((char *) name);
	}
        
        return (* smbw_libc.unlink)((char *) name);
}

int utime(const char *name, const struct utimbuf *tvp)
{
        check_init("utime");
        
	if (smbw_path(name)) {
		return smbw_utime(name, (struct utimbuf *) tvp);
	}
        
        return (* smbw_libc.utime)((char *) name, (struct utimbuf *) tvp);
}

int utimes(const char *name, const struct timeval *tvp)
{
        check_init("utimes");
        
	if (smbw_path(name)) {
		return smbw_utimes(name, (struct timeval *) tvp);
	}
        
        return (* smbw_libc.utimes)((char *) name, (struct timeval *) tvp);
}

int readlink(const char *path, char *buf, size_t bufsize)
{
        check_init("readlink");
        
	if (smbw_path((char *) path)) {
		return smbw_readlink(path, (char *) buf, bufsize);
	}
        
        return (* smbw_libc.readlink)((char *) path, buf, bufsize);
}

int rename(const char *oldname, const char *newname)
{
	int p1, p2;
        
        check_init("rename");
        
	p1 = smbw_path((char *) oldname); 
	p2 = smbw_path((char *) newname); 
	if (p1 ^ p2) {
		/* can't cross filesystem boundaries */
		errno = EXDEV;
		return -1;
	}
	if (p1 && p2) {
		return smbw_rename((char *) oldname, (char *) newname);
	}
        
        return (* smbw_libc.rename)((char *) oldname, (char *) newname);
}

int rmdir(const char *name)
{
        check_init("rmdir");
        
	if (smbw_path((char *) name)) {
		return smbw_rmdir((char *) name);
	}
        
        return (* smbw_libc.rmdir)((char *) name);
}

int symlink(const char *topath, const char *frompath)
{
	int p1, p2;
        
        check_init("symlink");
        
	p1 = smbw_path((char *) topath); 
	p2 = smbw_path((char *) frompath); 
	if (p1 || p2) {
		/* can't handle symlinks */
		errno = EPERM;
		return -1;
	}
        
        return (* smbw_libc.symlink)((char *) topath, (char *) frompath);
}

int dup(int fd)
{
        check_init("dup");
        
	if (smbw_fd(fd)) {
		return smbw_dup(fd);
	}
        
        return (* smbw_libc.dup)(fd);
}

int dup2(int oldfd, int newfd)
{
        check_init("dup2");
        
	if (smbw_fd(newfd)) {
		(* smbw_libc.close)(newfd);
	}
        
	if (smbw_fd(oldfd)) {
		return smbw_dup2(oldfd, newfd);
	}
        
        return (* smbw_libc.dup2)(oldfd, newfd);
}


DIR *opendir(const char *name)
{
        check_init("opendir");
        
	if (smbw_path((char *) name)) {
		return (void *)smbw_opendir((char *) name);
	}
        
        return (* smbw_libc.opendir)((char *) name);
}

struct dirent *readdir(DIR *dir)
{
        check_init("readdir");
        
	if (smbw_dirp(dir)) {
                static struct dirent external;
                struct SMBW_dirent * internal = (void *)smbw_readdir(dir);
                if (internal != NULL) {
                        dirent_convert(internal, &external);
                        return &external;
                }
                return NULL;
	}
        return (* smbw_libc.readdir)(dir);
}

int closedir(DIR *dir)
{
        check_init("closedir");
        
	if (smbw_dirp(dir)) {
		return smbw_closedir(dir);
	}
        
        return (* smbw_libc.closedir)(dir);
}

long telldir(DIR *dir)
{
        check_init("telldir");
        
	if (smbw_dirp(dir)) {
		return (long) smbw_telldir(dir);
	}
        
        return (* smbw_libc.telldir)(dir);
}

void seekdir(DIR *dir, long offset)
{
        check_init("seekdir");
        
	if (smbw_dirp(dir)) {
		smbw_seekdir(dir, (long long) offset);
		return;
	}
        
        (* smbw_libc.seekdir)(dir, offset);
}

int creat(const char *path, mode_t mode)
{
	extern int creat_bits;
        
        check_init("creat");
	return openx((char *) path, creat_bits, mode, smbw_libc.open);
}

int creat64(const char *path, mode_t mode)
{
	extern int creat_bits;
        
        check_init("creat64");
	return openx((char *) path, creat_bits, mode, smbw_libc.open64);
}

int __xstat64 (int ver, const char *name, struct stat64 *st64)
{
        check_init("__xstat64");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
		int ret = smbw_stat((char *) name, &statbuf);
		stat64_convert(&statbuf, st64);
		return ret;
	}
        
        return (* smbw_libc.__xstat64)(ver, (char *) name, st64);
}

int stat64(const char *name, struct stat64 *st64)
{
        check_init("stat64");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
		int ret = smbw_stat((char *) name, &statbuf);
		stat64_convert(&statbuf, st64);
		return ret;
	}
        
        return (* smbw_libc.stat64)((char *) name, st64);
}

int __fxstat64(int ver, int fd, struct stat64 *st64)
{
        check_init("__fxstat64");
        
	if (smbw_fd(fd)) {
                struct SMBW_stat statbuf;
		int ret = smbw_fstat(fd, &statbuf);
		stat64_convert(&statbuf, st64);
		return ret;
	}
        
        return (* smbw_libc.__fxstat64)(ver, fd, st64);
}

int fstat64(int fd, struct stat64 *st64)
{
        check_init("fstat64");
        
	if (smbw_fd(fd)) {
                struct SMBW_stat statbuf;
		int ret = smbw_fstat(fd, &statbuf);
		stat64_convert(&statbuf, st64);
		return ret;
	}
        
        return (* smbw_libc.fstat64)(fd, st64);
}

int __lxstat64(int ver, const char *name, struct stat64 *st64)
{
        check_init("__lxstat64");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
		int ret = smbw_stat(name, &statbuf);
		stat64_convert(&statbuf, st64);
		return ret;
	}
        
        return (* smbw_libc.__lxstat64)(ver, (char *) name, st64);
}

int lstat64(const char *name, struct stat64 *st64)
{
        check_init("lstat64");
        
	if (smbw_path((char *) name)) {
                struct SMBW_stat statbuf;
		int ret = smbw_stat((char *) name, &statbuf);
		stat64_convert(&statbuf, st64);
		return ret;
	}
        
        return (* smbw_libc.lstat64)((char *) name, st64);
}

int _llseek(unsigned int fd,  unsigned  long  offset_high, unsigned  long  offset_low,  loff_t  *result, unsigned int whence)
{
        check_init("_llseek");
        
	if (smbw_fd(fd)) {
		*result = lseek(fd, offset_low, whence);
                return (*result < 0 ? -1 : 0);
	}
        
        return (* smbw_libc._llseek)(fd, offset_high, offset_low, result, whence);
}

struct dirent64 *readdir64(DIR *dir)
{
        check_init("readdir64");
        
	if (smbw_dirp(dir)) {
                static struct dirent64 external;
                struct SMBW_dirent * internal = (void *)smbw_readdir(dir);
                if (internal != NULL) {
                        dirent64_convert(internal, &external);
                        return &external;
                }
                return NULL;
	}
        
        return (* smbw_libc.readdir64)(dir);
}

int readdir_r(DIR *dir, struct dirent *external, struct dirent **result)
{
        check_init("readdir_r");
        
	if (smbw_dirp(dir)) {
                struct SMBW_dirent internal;
                int ret = smbw_readdir_r(dir, &internal, NULL);
                if (ret == 0) {
                        dirent_convert(&internal, external);
                        *result = external;
                }
		return ret;
	}
        
        return (* smbw_libc.readdir_r)(dir, external, result);
}

int readdir64_r(DIR *dir, struct dirent64 *external, struct dirent64 **result)
{
        check_init("readdir64_r");
        
	if (smbw_dirp(dir)) {
                struct SMBW_dirent internal;
                int ret = smbw_readdir_r(dir, &internal, NULL);
                if (ret == 0) {
                        dirent64_convert(&internal, external);
                        *result = external;
                }
		return ret;
	}
        
        return (* smbw_libc.readdir64_r)(dir, external, result);
}

int fork(void)
{
        check_init("fork");
	return smbw_fork();
}

int setxattr(const char *fname,
             const char *name,
             const void *value,
             size_t size,
             int flags)
{
	if (smbw_path(fname)) {
		return smbw_setxattr(fname, name, value, size, flags);
	}
        
        return (* smbw_libc.setxattr)(fname, name, value, size, flags);
}

int lsetxattr(const char *fname,
              const char *name,
              const void *value,
              size_t size,
              int flags)
{
	if (smbw_path(fname)) {
		return smbw_lsetxattr(fname, name, value, size, flags);
	}
        
        return (* smbw_libc.lsetxattr)(fname, name, value, size, flags);
}

int fsetxattr(int fd,
              const char *name,
              const void *value,
              size_t size,
              int flags)
{
	if (smbw_fd(fd)) {
		return smbw_fsetxattr(fd, name, value, size, flags);
	}
        
        return (* smbw_libc.fsetxattr)(fd, name, value, size, flags);
}

int getxattr(const char *fname,
             const char *name,
             const void *value,
             size_t size)
{
	if (smbw_path(fname)) {
		return smbw_getxattr(fname, name, value, size);
	}
        
        return (* smbw_libc.getxattr)(fname, name, value, size);
}

int lgetxattr(const char *fname,
              const char *name,
              const void *value,
              size_t size)
{
	if (smbw_path(fname)) {
		return smbw_lgetxattr(fname, name, value, size);
	}
        
        return (* smbw_libc.lgetxattr)(fname, name, value, size);
}

int fgetxattr(int fd,
              const char *name,
              const void *value,
              size_t size)
{
	if (smbw_fd(fd)) {
		return smbw_fgetxattr(fd, name, value, size);
	}
        
        return (* smbw_libc.fgetxattr)(fd, name, value, size);
}

int removexattr(const char *fname,
                const char *name)
{
	if (smbw_path(fname)) {
		return smbw_removexattr(fname, name);
	}
        
        return (* smbw_libc.removexattr)(fname, name);
}

int lremovexattr(const char *fname,
                 const char *name)
{
	if (smbw_path(fname)) {
		return smbw_lremovexattr(fname, name);
	}
        
        return (* smbw_libc.lremovexattr)(fname, name);
}

int fremovexattr(int fd,
                 const char *name)
{
	if (smbw_fd(fd)) {
		return smbw_fremovexattr(fd, name);
	}
        
        return (* smbw_libc.fremovexattr)(fd, name);
}

int listxattr(const char *fname,
              char *list,
              size_t size)
{
	if (smbw_path(fname)) {
		return smbw_listxattr(fname, list, size);
	}
        
        return (* smbw_libc.listxattr)(fname, list, size);
}

int llistxattr(const char *fname,
               char *list,
               size_t size)
{
	if (smbw_path(fname)) {
		return smbw_llistxattr(fname, list, size);
	}
        
        return (* smbw_libc.llistxattr)(fname, list, size);
}

int flistxattr(int fd,
               char *list,
               size_t size)
{
	if (smbw_fd(fd)) {
                return smbw_flistxattr(fd, list, size);
	}
        
        return (* smbw_libc.flistxattr)(fd, list, size);
}


/*
 * We're ending up with a different implementation of malloc() with smbwrapper
 * than without it.  The one with it does not support returning a non-NULL
 * pointer from a call to malloc(0), and many apps depend on getting a valid
 * pointer when requesting zero length (e.g. df, emacs).
 *
 * Unfortunately, initializing the smbw_libc[] array via the dynamic link
 * library (libdl) requires malloc so we can't just do the same type of
 * mapping to the C library as we do with everything else.  We need to
 * implement a different way of allocating memory that ensures that the C
 * library version of malloc() gets used.  This is the only place where we
 * kludge the code to use an undocumented interface to the C library.
 *
 * If anyone can come up with a way to dynamically link to the C library
 * rather than using this undocumented interface, I'd sure love to hear about
 * it.  Better yet, if you can figure out where the alternate malloc()
 * functions are coming from and arrange for them not to be called, that would
 * be even better.  We should try to avoid wrapping functions that don't
 * really require it.
 */

void *malloc(size_t size)
{
        void *__libc_malloc(size_t size);
        return __libc_malloc(size);
}

void *calloc(size_t nmemb, size_t size)
{
        void *__libc_calloc(size_t nmemb, size_t size);
        return __libc_calloc(nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
        void *__libc_realloc(void *ptr, size_t size);
        return __libc_realloc(ptr, size);
}

void free(void *ptr)
{
        static int      in_progress = 0;
        void __libc_free(void *ptr);
        
        if (in_progress) return;
        in_progress = 1;
        __libc_free(ptr);
        in_progress = 0;
}


#if 0                           /* SELECT */

static struct sigaction user_action[_NSIG];

static void
smbw_sigaction_handler(int signum,
                       siginfo_t *info,
                       void *context)
{
        /* Our entire purpose for trapping signals is to call this! */
        sys_select_signal();
        
        /* Call the user's handler */
        if (user_action[signum].sa_handler != SIG_IGN &&
            user_action[signum].sa_handler != SIG_DFL &&
            user_action[signum].sa_handler != SIG_ERR) {
                (* user_action[signum].sa_sigaction)(signum, info, context);
        }
}


/*
 * Every Samba signal handler must call sys_select_signal() to avoid a race
 * condition, so we'll take whatever signal handler is currently assigned,
 * call call sys_select_signal() in addition to their call.
 */
static int
do_select(int n,
          fd_set *readfds,
          fd_set *writefds,
          fd_set *exceptfds,
          struct timeval *timeout,
          int (* select_fn)(int n,
                            fd_set *readfds,
                            fd_set *writefds,
                            fd_set *exceptfds,
                            struct timeval *timeout))
{
        int i;
        int ret;
        int saved_errno;
        sigset_t sigset;
        struct sigaction new_action;
        
        saved_errno = errno;
        for (i=1; i<_NSIG; i++) {
                sigemptyset(&sigset);
                new_action.sa_mask = sigset;
                new_action.sa_flags = SA_SIGINFO;
                new_action.sa_sigaction = smbw_sigaction_handler;
                
                if (sigaction(i, &new_action, &user_action[i]) < 0) {
                        if (errno != EINVAL) {
                                return -1;
                        }
                }
        }
        errno = saved_errno;
        
        ret = (* select_fn)(n, readfds, writefds, exceptfds, timeout);
        saved_errno = errno;
        
        for (i=0; i<_NSIG; i++) {
                (void) sigaction(i, &user_action[i], NULL);
        }
        
        errno = saved_errno;
        return ret;
}

int
select(int n,
       fd_set *readfds,
       fd_set *writefds,
       fd_set *exceptfds,
       struct timeval *timeout)
{
        check_init("select");
        
        return do_select(n, readfds, writefds, exceptfds,
                         timeout, smbw_libc.select);
}

int
_select(int n,
        fd_set *readfds,
        fd_set *writefds,
        fd_set *exceptfds,
        struct timeval *timeout)
{
        check_init("_select");
        
        return do_select(n, readfds, writefds, exceptfds,
                         timeout, smbw_libc._select);
}

int
__select(int n,
         fd_set *readfds,
         fd_set *writefds,
         fd_set *exceptfds,
         struct timeval *timeout)
{
        check_init("__select");
        
        return do_select(n, readfds, writefds, exceptfds,
                         timeout, smbw_libc.__select);
}

#endif
