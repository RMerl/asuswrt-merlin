/* 
   Unix SMB/Netbios implementation.
   Version 2.0
   SMB wrapper functions - definitions
   Copyright (C) Andrew Tridgell 1998
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

#ifndef _SMBW_H
#define _SMBW_H

#include <sys/types.h>
#include <errno.h>
#include <malloc.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"             /* must come before libsmbclient.h */
#include "libsmbclient.h"
#include "wrapper.h"

#ifndef __restrict
#  define __restrict
#endif

#undef DEBUG
#define DEBUG(level, s) do { if (level <= debug_level) printf s; } while (0)


#define SMBW_PREFIX "/smb"
#define SMBW_DUMMY "/dev/null"

extern int smbw_initialized;
#define SMBW_INIT()     do { if (! smbw_initialized) smbw_init(); } while (0)

#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T)
#  define SMBW_OFF_T off64_t
#else
#  define SMBW_OFF_T off_t
#endif


/* The following definitions come from  smbwrapper/smbw.c  */

typedef enum {
        SMBW_RCT_Increment,
        SMBW_RCT_Decrement,
        SMBW_RCT_Get,
        SMBW_RCT_Set
} Ref_Count_Type;

int smbw_ref(int client_fd, Ref_Count_Type type, ...);
void smbw_init(void);
int smbw_fd(int fd);
int smbw_path(const char *path);
void smbw_clean_fname(char *name);
void smbw_fix_path(const char *src, char *dest);
void smbw_set_auth_data_fn(smbc_get_auth_data_fn fn);
int smbw_open(const char *fname, int flags, mode_t mode);
ssize_t smbw_pread(int fd, void *buf, size_t count, SMBW_OFF_T ofs);
ssize_t smbw_read(int fd, void *buf, size_t count);
ssize_t smbw_write(int fd, void *buf, size_t count);
ssize_t smbw_pwrite(int fd, void *buf, size_t count, SMBW_OFF_T ofs);
int smbw_close(int fd);
int smbw_fcntl(int fd, int cmd, long arg);
int smbw_access(const char *name, int mode);
int smbw_readlink(const char *path, char *buf, size_t bufsize);
int smbw_unlink(const char *fname);
int smbw_rename(const char *oldname, const char *newname);
int smbw_utime(const char *fname, void *buf);
int smbw_utimes(const char *fname, void *buf);
int smbw_chown(const char *fname, uid_t owner, gid_t group);
int smbw_chmod(const char *fname, mode_t newmode);
SMBW_OFF_T smbw_lseek(int smbw_fd, SMBW_OFF_T offset, int whence);
int smbw_dup(int fd);
int smbw_dup2(int fd, int fd2);
int smbw_fork(void);

/* The following definitions come from  smbwrapper/smbw_dir.c  */

int smbw_dirp(DIR * dirp);
int smbw_dir_open(const char *fname);
int smbw_dir_fstat(int fd, SMBW_stat *st);
int smbw_dir_close(int fd);
int smbw_getdents(unsigned int fd, SMBW_dirent *dirp, int count);
int smbw_chdir(const char *name);
int smbw_mkdir(const char *fname, mode_t mode);
int smbw_rmdir(const char *fname);
char *smbw_getcwd(char *buf, size_t size);
int smbw_fchdir(int fd);
DIR *smbw_opendir(const char *fname);
SMBW_dirent *smbw_readdir(DIR *dirp);
int smbw_readdir_r(DIR *dirp,
                   struct SMBW_dirent *__restrict entry,
                   struct SMBW_dirent **__restrict result);
int smbw_closedir(DIR *dirp);
void smbw_seekdir(DIR *dirp, long long offset);
long long smbw_telldir(DIR *dirp);
int smbw_setxattr(const char *fname,
                  const char *name,
                  const void *value,
                  size_t size,
                  int flags);
int smbw_lsetxattr(const char *fname,
                   const char *name,
                   const void *value,
                   size_t size,
                   int flags);
int smbw_fsetxattr(int smbw_fd,
                   const char *name,
                   const void *value,
                   size_t size,
                   int flags);
int smbw_getxattr(const char *fname,
                  const char *name,
                  const void *value,
                  size_t size);
int smbw_lgetxattr(const char *fname,
                   const char *name,
                   const void *value,
                   size_t size);
int smbw_fgetxattr(int smbw_fd,
                   const char *name,
                   const void *value,
                   size_t size);
int smbw_removexattr(const char *fname,
                     const char *name);
int smbw_lremovexattr(const char *fname,
                      const char *name);
int smbw_fremovexattr(int smbw_fd,
                      const char *name);
int smbw_listxattr(const char *fname,
                   char *list,
                   size_t size);
int smbw_llistxattr(const char *fname,
                    char *list,
                    size_t size);
int smbw_flistxattr(int smbw_fd,
                    char *list,
                    size_t size);

/* The following definitions come from  smbwrapper/smbw_stat.c  */

int smbw_fstat(int fd, SMBW_stat *st);
int smbw_stat(const char *fname, SMBW_stat *st);

/* The following definitions come from smbwrapper/cache.c */
int
smbw_cache_functions(SMBCCTX * context);


#endif /* _SMBW_H */
