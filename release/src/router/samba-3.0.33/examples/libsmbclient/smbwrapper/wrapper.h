/* 
   Unix SMB/Netbios implementation.
   Version 2.0
   SMB wrapper functions
   Copyright (C) Derrell Lipman 2003-2005
   
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

#ifndef __WRAPPER_H__
#define __WRAPPER_H__

#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <signal.h>
#include <stdio.h>

#ifndef __FD_SETSIZE
#  define __FD_SETSIZE  256
#endif

extern int smbw_fd_map[__FD_SETSIZE];
extern int smbw_ref_count[__FD_SETSIZE];
extern char smbw_cwd[PATH_MAX];
extern char smbw_prefix[];

typedef struct SMBW_stat {
        unsigned long           s_dev;     /* device */
        unsigned long           s_ino;     /* inode */
        unsigned long           s_mode;    /* protection */
        unsigned long           s_nlink;   /* number of hard links */
        unsigned long           s_uid;     /* user ID of owner */
        unsigned long           s_gid;     /* group ID of owner */
        unsigned long           s_rdev;    /* device type (if inode device) */
        unsigned long long      s_size;    /* total size, in bytes */
        unsigned long           s_blksize; /* blocksize for filesystem I/O */
        unsigned long           s_blocks;  /* number of blocks allocated */
        unsigned long           s_atime;   /* time of last access */
        unsigned long           s_mtime;   /* time of last modification */
        unsigned long           s_ctime;   /* time of last change */
} SMBW_stat;

typedef struct SMBW_dirent {
        unsigned long           d_ino;      /* inode number */
        unsigned long long      d_off;      /* offset to the next dirent */
        unsigned long           d_reclen;   /* length of this record */
        unsigned long           d_type;     /* type of file */
        char                    d_name[256]; /* filename */
        char                    d_comment[256]; /* comment */
} SMBW_dirent;

struct kernel_sigaction {
        __sighandler_t k_sa_handler;
        unsigned long sa_flags;
        sigset_t sa_mask;
};

typedef struct SMBW_libc
{
        /* write() is first, to allow debugging */
        ssize_t (* write)(int fd, void *buf, size_t count);
        int (* open)(char *name, int flags, mode_t mode);
        int (* _open)(char *name, int flags, mode_t mode) ;
        int (* __open)(char *name, int flags, mode_t mode) ;
        int (* open64)(char *name, int flags, mode_t mode);
        int (* _open64)(char *name, int flags, mode_t mode) ;
        int (* __open64)(char *name, int flags, mode_t mode) ;
        ssize_t (* pread)(int fd, void *buf, size_t size, off_t ofs);
        ssize_t (* pread64)(int fd, void *buf, size_t size, off64_t ofs);
        ssize_t (* pwrite)(int fd, void *buf, size_t size, off_t ofs);
        ssize_t (* pwrite64)(int fd, void *buf, size_t size, off64_t ofs);
        int (* close)(int fd);
        int (* __close)(int fd);
        int (* _close)(int fd);
        int (* fcntl)(int fd, int cmd, long arg);
        int (* __fcntl)(int fd, int cmd, long arg);
        int (* _fcntl)(int fd, int cmd, long arg);
        int (* getdents)(int fd, struct dirent *dirp, unsigned int count);
        int (* __getdents)(int fd, struct dirent *dirp, unsigned int count);
        int (* _getdents)(int fd, struct dirent *dirp, unsigned int count);
        int (* getdents64)(int fd, struct dirent64 *dirp, unsigned int count);
        off_t (* lseek)(int fd, off_t offset, int whence);
        off_t (* __lseek)(int fd, off_t offset, int whence);
        off_t (* _lseek)(int fd, off_t offset, int whence);
        off64_t (* lseek64)(int fd, off64_t offset, int whence);
        off64_t (* __lseek64)(int fd, off64_t offset, int whence);
        off64_t (* _lseek64)(int fd, off64_t offset, int whence);
        ssize_t (* read)(int fd, void *buf, size_t count);
        ssize_t (* __read)(int fd, void *buf, size_t count);
        ssize_t (* _read)(int fd, void *buf, size_t count);
        ssize_t (* __write)(int fd, void *buf, size_t count);
        ssize_t (* _write)(int fd, void *buf, size_t count);
        int (* access)(char *name, int mode);
        int (* chmod)(char *name, mode_t mode);
        int (* fchmod)(int fd, mode_t mode);
        int (* chown)(char *name, uid_t owner, gid_t group);
        int (* fchown)(int fd, uid_t owner, gid_t group);
        int (* __xstat)(int vers, char *name, struct stat *st);
        char * ( *getcwd)(char *buf, size_t size);
        int (* mkdir)(char *name, mode_t mode);
        int (* __fxstat)(int vers, int fd, struct stat *st);
        int (* __lxstat)(int vers, char *name, struct stat *st);
        int (* stat)(char *name, struct stat *st);
        int (* lstat)(char *name, struct stat *st);
        int (* fstat)(int fd, struct stat *st);
        int (* unlink)(char *name);
        int (* utime)(char *name, struct utimbuf *tvp);
        int (* utimes)(char *name, struct timeval *tvp);
        int (* readlink)(char *path, char *buf, size_t bufsize);
        int (* rename)(char *oldname, char *newname);
        int (* rmdir)(char *name);
        int (* symlink)(char *topath, char *frompath);
        int (* dup)(int fd);
        int (* dup2)(int oldfd, int newfd);
        DIR * (* opendir)(char *name);
        struct dirent * (* readdir)(DIR *dir);
        int (* closedir)(DIR *dir);
        off_t (* telldir)(DIR *dir);
        void (* seekdir)(DIR *dir, off_t offset);
        int (* creat)(char *path, mode_t mode);
        int (* creat64)(char *path, mode_t mode);
        int (* __xstat64)(int ver, char *name, struct stat64 *st64);
        int (* stat64)(char *name, struct stat64 *st64);
        int (* __fxstat64)(int ver, int fd, struct stat64 *st64);
        int (* fstat64)(int fd, struct stat64 *st64);
        int (* __lxstat64)(int ver, char *name, struct stat64 *st64);
        int (* lstat64)(char *name, struct stat64 *st64);
        int (* _llseek)(unsigned int fd,  unsigned  long  offset_high, unsigned  long  offset_low,  loff_t  *result, unsigned int whence);
        struct dirent64 * (* readdir64)(DIR *dir);
        int (* readdir_r)(DIR *dir, struct dirent *entry, struct dirent **result);
        int (* readdir64_r)(DIR *dir, struct dirent64 *entry, struct dirent64 **result);
        int (* setxattr)(const char *fname,
                         const char *name,
                         const void *value,
                         size_t size,
                         int flags);
        int (* lsetxattr)(const char *fname,
                          const char *name,
                          const void *value,
                          size_t size,
                          int flags);
        int (* fsetxattr)(int smbw_fd,
                          const char *name,
                          const void *value,
                          size_t size,
                          int flags);
        int (* getxattr)(const char *fname,
                         const char *name,
                         const void *value,
                         size_t size);
        int (* lgetxattr)(const char *fname,
                          const char *name,
                          const void *value,
                          size_t size);
        int (* fgetxattr)(int smbw_fd,
                          const char *name,
                          const void *value,
                          size_t size);
        int (* removexattr)(const char *fname,
                            const char *name);
        int (* lremovexattr)(const char *fname,
                             const char *name);
        int (* fremovexattr)(int smbw_fd,
                             const char *name);
        int (* listxattr)(const char *fname,
                          char *list,
                          size_t size);
        int (* llistxattr)(const char *fname,
                           char *list,
                           size_t size);
        int (* flistxattr)(int smbw_fd,
                           char *list,
                           size_t size);
        int (* chdir)(const char *path);
        int (* fchdir)(int fd);
        pid_t (* fork)(void);
        int (* select)(int n,
                       fd_set *readfds,
                       fd_set *writefds,
                       fd_set *exceptfds,
                       struct timeval *timeout);
        int (* _select)(int n,
                        fd_set *readfds,
                        fd_set *writefds,
                        fd_set *exceptfds,
                        struct timeval *timeout);
        int (* __select)(int n,
                         fd_set *readfds,
                         fd_set *writefds,
                         fd_set *exceptfds,
                         struct timeval *timeout);
} SMBW_libc_pointers;

extern SMBW_libc_pointers smbw_libc;

#endif /* __WRAPPER_H__ */
