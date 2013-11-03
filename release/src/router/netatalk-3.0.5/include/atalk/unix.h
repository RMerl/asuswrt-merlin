/*
   Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef ATALK_UNIX_H
#define ATALK_UNIX_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <atalk/globals.h>

#define NETATALK_DIOSZ_STACK 65536
#define NETATALK_DIOSZ_HEAP  (1024*1024)

struct vol;

/* vfs/unix.c */
extern int netatalk_unlink(const char *name);
extern int netatalk_unlinkat(int dirfd, const char *name);
extern int statat(int dirfd, const char *path, struct stat *st);
extern DIR *opendirat(int dirfd, const char *path);

/* rmdir ENOENT not an error */
extern int netatalk_rmdir(int dirfd, const char *name);
extern int netatalk_rmdir_all_errors(int dirfd, const char *name);

extern int setfilmode(const struct vol *vol, const char *name, mode_t mode, struct stat *st);
extern int dir_rx_set(mode_t mode);
extern int unix_rename(int sfd, const char *oldpath, int dfd, const char *newpath);
extern int copy_file(int sfd, const char *src, const char *dst, mode_t mode);
extern int copy_file_fd(int sfd, int dfd);
extern int copy_ea(const char *ea, int sfd, const char *src, const char *dst, mode_t mode);

extern void become_root(void);
extern void unbecome_root(void);
extern int gmem(gid_t gid, int ngroups, gid_t *groups);
extern int set_groups(AFPObj *obj, struct passwd *pwd);
extern const char *print_groups(int ngroups, gid_t *groups);
#endif  /* ATALK_UNIX_H */
