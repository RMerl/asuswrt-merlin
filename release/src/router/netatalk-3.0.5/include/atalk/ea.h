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

#ifndef ATALK_EA_H
#define ATALK_EA_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#elif HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#ifdef HAVE_SYS_EA_H
#include <sys/ea.h>
#endif

#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif

#ifdef HAVE_SOLARIS_ACLS
#include <sys/acl.h>
#endif

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#include <atalk/vfs.h>

/*
 * This seems to be the current limit fo HFS+, we arbitrarily force that
 *  which also safes us from buffer overflows
 */
#define MAX_EA_SIZE 3802

/*
 * At time of writing the 10.5.6 client adds 8 bytes to the
 * length of the EA that we send him 
*/
#define MAX_REPLY_EXTRA_BYTES 8

/* 
 * Library user must provide a static buffer of size ATTRNAMEBUFSIZ.
 * It's used when listing EAs as intermediate buffer. For afpd it's
 * defined in extattrs.c.
 */
#define ATTRNAMEBUFSIZ 4096

enum {
    kXAttrNoFollow = 0x1,
    kXAttrCreate = 0x2,
    kXAttrReplace = 0x4
};

#if !defined(HAVE_SETXATTR)
#define XATTR_CREATE  0x1       /* set value, fail if attr already exists */
#define XATTR_REPLACE 0x2       /* set value, fail if attr does not exist */
#endif

/* Names for our Extended Attributes adouble data */
#define AD_EA_META "org.netatalk.Metadata"
#define AD_EA_RESO "org.netatalk.ResourceFork"
#define NOT_NETATALK_EA(a) (strcmp((a), AD_EA_META) != 0) && (strcmp((a), AD_EA_RESO) != 0)

/****************************************************************************************
 * Wrappers for native EA functions taken from Samba
 ****************************************************************************************/
ssize_t sys_getxattr (const char *path, const char *name, void *value, size_t size);
ssize_t sys_lgetxattr (const char *path, const char *name, void *value, size_t size);
ssize_t sys_fgetxattr (int filedes, const char *name, void *value, size_t size);
ssize_t sys_listxattr (const char *path, char *list, size_t size);
ssize_t sys_llistxattr (const char *path, char *list, size_t size);
ssize_t sys_flistxattr (int filedes, char *list, size_t size);
int sys_removexattr (const char *path, const char *name);
int sys_lremovexattr (const char *path, const char *name);
int sys_fremovexattr (int filedes, const char *name);
int sys_setxattr (const char *path, const char *name, const void *value, size_t size, int flags);
int sys_lsetxattr (const char *path, const char *name, const void *value, size_t size, int flags);
int sys_fsetxattr (int filedes, const char *name, const void *value, size_t size, int flags);
int sys_copyxattr (const char *src, const char *dst);
int sys_getxattrfd(int fd, const char *uname, int oflag, ...);

/****************************************************************************************
 * Stuff for our implementation of storing EAs in files in .AppleDouble dirs
 ****************************************************************************************/

#define EA_INITED   0xea494e54  /* ea"INT", for interfacing ea_open w. ea_close */
#define EA_MAGIC    0x61644541 /* "adEA" */
#define EA_VERSION1 0x01
#define EA_VERSION  EA_VERSION1

typedef enum {
    /* ea_open flags */
    EA_CREATE    = (1<<1),      /* create if not existing on ea_open */
    EA_RDONLY    = (1<<2),      /* open read only */
    EA_RDWR      = (1<<3),      /* open read/write */
    /* ea_open internal flags */
    EA_DIR       = (1<<4)       /* ea header file is for a dir, ea_open adds it as appropiate */
} eaflags_t;

#define EA_MAGIC_OFF   0
#define EA_MAGIC_LEN   4
#define EA_VERSION_OFF (EA_MAGIC_OFF + EA_MAGIC_LEN)
#define EA_VERSION_LEN 2
#define EA_COUNT_OFF   (EA_VERSION_OFF + EA_VERSION_LEN)
#define EA_COUNT_LEN   2
#define EA_HEADER_SIZE (EA_MAGIC_LEN + EA_VERSION_LEN + EA_COUNT_LEN)

/* 
 * structs describing the layout of the Extended Attributes bookkeeping file.
 * This isn't really an AppleDouble structure, it's just a binary blob that
 * lives in our .AppleDouble directory too.
 */

struct ea_entry {
    size_t       ea_namelen; /* len of ea_name without terminating 0 ie. strlen(ea_name)*/
    size_t       ea_size;    /* size of EA*/
    char         *ea_name;   /* name of the EA */
};

/* We read the on-disk data into *ea_data and parse it into this*/
struct ea {
    uint32_t             ea_inited;       /* needed for interfacing ea_open w. ea_close */
    const struct vol     *vol;            /* vol handle, ea_close needs it */
    int                  dirfd;           /* for *at (cf openat) semantics, -1 means ignore */
    char                 *filename;       /* name of file, needed by ea_close too */
    unsigned int         ea_count;        /* number of EAs in ea_entries array */
    struct ea_entry      (*ea_entries)[]; /* malloced and realloced as needed by ea_count*/
    int                  ea_fd;           /* open fd for ea_data */
    eaflags_t            ea_flags;        /* flags */
    size_t               ea_size;         /* size of header file = size of ea_data buffer */
    char                 *ea_data;        /* pointer to buffer into that we actually *
                                           * read the disc file into                 */
};

/* On-disk format, just for reference ! */
#if 0
struct ea_entry_ondisk {
    uint32_t               ea_size;
    char                   ea_name[]; /* zero terminated string */
};

struct ea_ondisk {
    u_int32_t              ea_magic;
    u_int16_t              ea_version;
    u_int16_t              ea_count;
    struct ea_entry_ondisk ea_entries[ea_count];
};
#endif /* 0 */

/* VFS inderected funcs ... : */

/* Default adouble EAs */
extern int get_easize(VFS_FUNC_ARGS_EA_GETSIZE);
extern int get_eacontent(VFS_FUNC_ARGS_EA_GETCONTENT);
extern int list_eas(VFS_FUNC_ARGS_EA_LIST);
extern int set_ea(VFS_FUNC_ARGS_EA_SET);
extern int remove_ea(VFS_FUNC_ARGS_EA_REMOVE);
/* ... EA VFS funcs that deal with file/dir cp/mv/rm */
extern int ea_deletefile(VFS_FUNC_ARGS_DELETEFILE);
extern int ea_renamefile(VFS_FUNC_ARGS_RENAMEFILE);
extern int ea_copyfile(VFS_FUNC_ARGS_COPYFILE);
extern int ea_chown(VFS_FUNC_ARGS_CHOWN);
extern int ea_chmod_file(VFS_FUNC_ARGS_SETFILEMODE);
extern int ea_chmod_dir(VFS_FUNC_ARGS_SETDIRUNIXMODE);

/* native EAs */
extern int sys_get_easize(VFS_FUNC_ARGS_EA_GETSIZE);
extern int sys_get_eacontent(VFS_FUNC_ARGS_EA_GETCONTENT);
extern int sys_list_eas(VFS_FUNC_ARGS_EA_LIST);
extern int sys_set_ea(VFS_FUNC_ARGS_EA_SET);
extern int sys_remove_ea(VFS_FUNC_ARGS_EA_REMOVE);
/* native EA VFSfile/dir cp/mv/rm */
extern int sys_ea_copyfile(VFS_FUNC_ARGS_COPYFILE);

/* dbd needs access to these */
extern int ea_open(const struct vol * restrict vol,
                   const char * restrict uname,
                   eaflags_t eaflags,
                   struct ea * restrict ea);
extern int ea_openat(const struct vol * restrict vol,
                     int dirfd,
                     const char * restrict uname,
                     eaflags_t eaflags,
                     struct ea * restrict ea);
extern int ea_close(struct ea * restrict ea);
extern char *ea_path(const struct ea * restrict ea, const char * restrict eaname, int macname);

#endif /* ATALK_EA_H */
