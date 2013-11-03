/*
   Copyright (c) 2004 Didier Gautheron
 
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

   vfs layer for afp
*/

#ifndef ATALK_VFS_H
#define ATALK_VFS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <atalk/adouble.h>
#include <atalk/volume.h>
#include <atalk/acl.h>

#define VFS_FUNC_ARGS_VALIDUPATH const struct vol *vol, const char *name
#define VFS_FUNC_VARS_VALIDUPATH vol, name

#define VFS_FUNC_ARGS_CHOWN const struct vol *vol, const char *path, uid_t uid, gid_t gid
#define VFS_FUNC_VARS_CHOWN vol, path, uid, gid

#define VFS_FUNC_ARGS_RENAMEDIR const struct vol *vol, int dirfd, const char *oldpath, const char *newpath
#define VFS_FUNC_VARS_RENAMEDIR vol, dirfd, oldpath, newpath

#define VFS_FUNC_ARGS_DELETECURDIR const struct vol *vol
#define VFS_FUNC_VARS_DELETECURDIR vol

#define VFS_FUNC_ARGS_SETFILEMODE const struct vol *vol, const char *name, mode_t mode, struct stat *st
#define VFS_FUNC_VARS_SETFILEMODE vol, name, mode, st

#define VFS_FUNC_ARGS_SETDIRMODE const struct vol *vol,  const char *name, mode_t mode, struct stat *st
#define VFS_FUNC_VARS_SETDIRMODE vol, name, mode, st

#define VFS_FUNC_ARGS_SETDIRUNIXMODE const struct vol *vol, const char *name, mode_t mode, struct stat *st
#define VFS_FUNC_VARS_SETDIRUNIXMODE vol, name, mode, st

#define VFS_FUNC_ARGS_SETDIROWNER const struct vol *vol, const char *name, uid_t uid, gid_t gid
#define VFS_FUNC_VARS_SETDIROWNER vol, name, uid, gid

#define VFS_FUNC_ARGS_DELETEFILE const struct vol *vol, int dirfd, const char *file
#define VFS_FUNC_VARS_DELETEFILE vol, dirfd, file

#define VFS_FUNC_ARGS_RENAMEFILE const struct vol *vol, int dirfd, const char *src, const char *dst
#define VFS_FUNC_VARS_RENAMEFILE vol, dirfd, src, dst

#define VFS_FUNC_ARGS_COPYFILE const struct vol *vol, int sfd, const char *src, const char *dst
#define VFS_FUNC_VARS_COPYFILE vol, sfd, src, dst

#ifdef HAVE_SOLARIS_ACLS
#define VFS_FUNC_ARGS_ACL const struct vol *vol, const char *path, int cmd, int count, void *aces
#define VFS_FUNC_VARS_ACL vol, path, cmd, count, aces
#endif
#ifdef HAVE_POSIX_ACLS
#define VFS_FUNC_ARGS_ACL const struct vol *vol, const char *path, acl_type_t type, int count, acl_t acl
#define VFS_FUNC_VARS_ACL vol, path, type, count, acl
#endif

#define VFS_FUNC_ARGS_REMOVE_ACL const struct vol *vol, const char *path, int dir
#define VFS_FUNC_VARS_REMOVE_ACL vol, path, dir

#define VFS_FUNC_ARGS_EA_GETSIZE const struct vol * restrict vol, char * restrict rbuf, size_t * restrict rbuflen, const char * restrict uname, int oflag, const char * restrict attruname
#define VFS_FUNC_VARS_EA_GETSIZE vol, rbuf, rbuflen, uname, oflag, attruname

#define VFS_FUNC_ARGS_EA_GETCONTENT const struct vol * restrict vol, char * restrict rbuf, size_t * restrict rbuflen,  const char * restrict uname, int oflag, const char * restrict attruname, int maxreply
#define VFS_FUNC_VARS_EA_GETCONTENT vol, rbuf, rbuflen, uname, oflag, attruname, maxreply

#define VFS_FUNC_ARGS_EA_LIST const struct vol * restrict vol, char * restrict attrnamebuf, size_t * restrict buflen, const char * restrict uname, int oflag
#define VFS_FUNC_VARS_EA_LIST vol, attrnamebuf, buflen, uname, oflag

#define VFS_FUNC_ARGS_EA_SET const struct vol * restrict vol, const char * restrict uname, const char * restrict attruname, const char * restrict ibuf, size_t attrsize, int oflag
#define VFS_FUNC_VARS_EA_SET vol, uname, attruname, ibuf, attrsize, oflag

#define VFS_FUNC_ARGS_EA_REMOVE const struct vol * restrict vol, const char * restrict uname, const char * restrict attruname, int oflag
#define VFS_FUNC_VARS_EA_REMOVE vol, uname, attruname, oflag

/*
 * Forward declaration. We need it because of the circular inclusion of
 * of vfs.h <-> volume.h. 
 */
struct vol;

struct vfs_ops {
    int (*vfs_validupath)    (VFS_FUNC_ARGS_VALIDUPATH);
    int (*vfs_chown)         (VFS_FUNC_ARGS_CHOWN);
    int (*vfs_renamedir)     (VFS_FUNC_ARGS_RENAMEDIR);
    int (*vfs_deletecurdir)  (VFS_FUNC_ARGS_DELETECURDIR);
    int (*vfs_setfilmode)    (VFS_FUNC_ARGS_SETFILEMODE);
    int (*vfs_setdirmode)    (VFS_FUNC_ARGS_SETDIRMODE);
    int (*vfs_setdirunixmode)(VFS_FUNC_ARGS_SETDIRUNIXMODE);
    int (*vfs_setdirowner)   (VFS_FUNC_ARGS_SETDIROWNER);
    int (*vfs_deletefile)    (VFS_FUNC_ARGS_DELETEFILE);
    int (*vfs_renamefile)    (VFS_FUNC_ARGS_RENAMEFILE);
    int (*vfs_copyfile)      (VFS_FUNC_ARGS_COPYFILE);

#ifdef HAVE_ACLS
    /* ACLs */
    int (*vfs_acl)           (VFS_FUNC_ARGS_ACL);
    int (*vfs_remove_acl)    (VFS_FUNC_ARGS_REMOVE_ACL);
#endif

    /* Extended Attributes */
    int (*vfs_ea_getsize)    (VFS_FUNC_ARGS_EA_GETSIZE);
    int (*vfs_ea_getcontent) (VFS_FUNC_ARGS_EA_GETCONTENT);
    int (*vfs_ea_list)       (VFS_FUNC_ARGS_EA_LIST);
    int (*vfs_ea_set)        (VFS_FUNC_ARGS_EA_SET);
    int (*vfs_ea_remove)     (VFS_FUNC_ARGS_EA_REMOVE);
};

extern void initvol_vfs(struct vol * restrict vol);

#endif /* ATALK_VFS_H */
