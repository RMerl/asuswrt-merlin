/* 
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000, 2002
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002 
   Copyright (C) Derrell Lipman 2003-2008
   Copyright (C) Jeremy Allison 2007, 2008
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"


#ifndef _LIBSMB_INTERNAL_H_
#define _LIBSMB_INTERNAL_H_

#include "../include/libsmbclient.h"
#include "libsmb/clirap.h"

#define SMBC_MAX_NAME  1023
#define SMBC_FILE_MODE (S_IFREG | 0444)
#define SMBC_DIR_MODE  (S_IFDIR | 0555)

/*
 * DOS Attribute values (used internally)
 */
typedef struct DOS_ATTR_DESC {
	int mode;
	SMB_OFF_T size;
	time_t create_time;
	time_t access_time;
	time_t write_time;
	time_t change_time;
	SMB_INO_T inode;
} DOS_ATTR_DESC;


/*
 * Internal flags for extended attributes
 */

/* internal mode values */
#define SMBC_XATTR_MODE_ADD          1
#define SMBC_XATTR_MODE_REMOVE       2
#define SMBC_XATTR_MODE_REMOVE_ALL   3
#define SMBC_XATTR_MODE_SET          4
#define SMBC_XATTR_MODE_CHOWN        5
#define SMBC_XATTR_MODE_CHGRP        6

#define CREATE_ACCESS_READ      READ_CONTROL_ACCESS

/*We should test for this in configure ... */
#ifndef ENOTSUP
#define ENOTSUP EOPNOTSUPP
#endif


struct _SMBCSRV {
	struct cli_state *cli;
	dev_t dev;
	bool no_pathinfo;
	bool no_pathinfo2;
        bool no_nt_session;
        struct policy_handle pol;

	SMBCSRV *next, *prev;
	
};

/* 
 * Keep directory entries in a list 
 */
struct smbc_dir_list {
	struct smbc_dir_list *next;
	struct smbc_dirent *dirent;
};


/*
 * Structure for open file management
 */ 
struct _SMBCFILE {
	int cli_fd; 
	char *fname;
	SMB_OFF_T offset;
	struct _SMBCSRV *srv;
	bool file;
	struct smbc_dir_list *dir_list, *dir_end, *dir_next;
	int dir_type, dir_error;

	SMBCFILE *next, *prev;
};


/*
 * Context structure
 */
struct SMBC_internal_data {

	/* True when this handle is initialized */
	bool                                    initialized;

        /* dirent pointer location */
	struct smbc_dirent			dirent;
	/*
         * Leave room for any urlencoded filename and the comment field.
         *
	 * We use (NAME_MAX * 3) plus whatever the max length of a comment is,
	 * plus a couple of null terminators (one after the filename,
	 * one after the comment).
         *
         * According to <linux/limits.h>, NAME_MAX is 255.  Is it longer
         * anyplace else?
         */
	char                                    _dirent_name[1024];

	/*
         * server connection list
	 */
	SMBCSRV *                               servers;
	
	/*
         * open file/dir list
	 */
	SMBCFILE *                              files;

        /*
         * Support "Create Time" in get/set with the *xattr() functions, if
         * true.  This replaces the dos attribute strings C_TIME, A_TIME and
         * M_TIME with CHANGE_TIME, ACCESS_TIME and WRITE_TIME, and adds
         * CREATE_TIME.  Default is FALSE, i.e.  to use the old-style shorter
         * names and to not support CREATE time, for backward compatibility.
         */
        bool                                    full_time_names;

        /*
         * The share mode of a file being opened.  To match POSIX semantics
         * (and maintain backward compatibility), DENY_NONE is the default.
         */
        smbc_share_mode                         share_mode;

        /*
         * Authentication function which includes the context.  This will be
         * used if set; otherwise context->callbacks.auth_fn() will be used.
         */
        smbc_get_auth_data_with_context_fn      auth_fn_with_context;

        /*
         * An opaque (to this library) user data handle which can be set
         * and retrieved with smbc_option_set() and smbc_option_get().
         */
        void *                                  user_data;

        /*
         * Should we attempt UNIX smb encryption ? 
         * Set to 0 if we should never attempt, set to 1 if
         * encryption requested, set to 2 if encryption required.
         */
        smbc_smb_encrypt_level                  smb_encryption_level;

        /*
         * Should we request case sensitivity of file names?
         */
        bool                                    case_sensitive;

	/*
	 * Auth info needed for DFS traversal.
	 */

	struct user_auth_info			*auth_info;

        struct smbc_server_cache * server_cache;

        /* POSIX emulation functions */
        struct
        {
#if 0 /* Left in libsmbclient.h for backward compatibility */
                smbc_open_fn                    open_fn;
                smbc_creat_fn                   creat_fn;
                smbc_read_fn                    read_fn;
                smbc_write_fn                   write_fn;
                smbc_unlink_fn                  unlink_fn;
                smbc_rename_fn                  rename_fn;
                smbc_lseek_fn                   lseek_fn;
                smbc_stat_fn                    stat_fn;
                smbc_fstat_fn                   fstat_fn;
#endif
                smbc_statvfs_fn                 statvfs_fn;
                smbc_fstatvfs_fn                fstatvfs_fn;
                smbc_ftruncate_fn               ftruncate_fn;
#if 0 /* Left in libsmbclient.h for backward compatibility */
                smbc_close_fn                   close_fn;
                smbc_opendir_fn                 opendir_fn;
                smbc_closedir_fn                closedir_fn;
                smbc_readdir_fn                 readdir_fn;
                smbc_getdents_fn                getdents_fn;
                smbc_mkdir_fn                   mkdir_fn;
                smbc_rmdir_fn                   rmdir_fn;
                smbc_telldir_fn                 telldir_fn;
                smbc_lseekdir_fn                lseekdir_fn;
                smbc_fstatdir_fn                fstatdir_fn;
                smbc_chmod_fn                   chmod_fn;
                smbc_utimes_fn                  utimes_fn;
                smbc_setxattr_fn                setxattr_fn;
                smbc_getxattr_fn                getxattr_fn;
                smbc_removexattr_fn             removexattr_fn;
                smbc_listxattr_fn               listxattr_fn;
#endif
        }               posix_emu;

#if 0 /* Left in libsmbclient.h for backward compatibility */
        /* Printing-related functions */
        struct
        {
                smbc_print_file_fn              print_file_fn;
                smbc_open_print_job_fn          open_print_job_fn;
                smbc_list_print_jobs_fn         list_print_jobs_fn;
                smbc_unlink_print_job_fn        unlink_print_job_fn;
        }               printing;
#endif

#if 0 /* None available yet */
        /* SMB high-level functions */
        struct
        {
        }               smb;

#endif
};	

/* Functions in libsmb_cache.c */
int
SMBC_add_cached_server(SMBCCTX * context,
                       SMBCSRV * newsrv,
                       const char * server,
                       const char * share, 
                       const char * workgroup,
                       const char * username);

SMBCSRV *
SMBC_get_cached_server(SMBCCTX * context,
                       const char * server, 
                       const char * share,
                       const char * workgroup,
                       const char * user);

int
SMBC_remove_cached_server(SMBCCTX * context,
                          SMBCSRV * server);

int
SMBC_purge_cached_servers(SMBCCTX * context);


/* Functions in libsmb_dir.c */
int
SMBC_check_options(char *server,
                   char *share,
                   char *path,
                   char *options);

SMBCFILE *
SMBC_opendir_ctx(SMBCCTX *context,
                 const char *fname);

int
SMBC_closedir_ctx(SMBCCTX *context,
                  SMBCFILE *dir);

struct smbc_dirent *
SMBC_readdir_ctx(SMBCCTX *context,
                 SMBCFILE *dir);

int
SMBC_getdents_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  struct smbc_dirent *dirp,
                  int count);

int
SMBC_mkdir_ctx(SMBCCTX *context,
               const char *fname,
               mode_t mode);

int
SMBC_rmdir_ctx(SMBCCTX *context,
               const char *fname);

off_t
SMBC_telldir_ctx(SMBCCTX *context,
                 SMBCFILE *dir);

int
SMBC_lseekdir_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  off_t offset);

int
SMBC_fstatdir_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  struct stat *st);

int
SMBC_chmod_ctx(SMBCCTX *context,
               const char *fname,
               mode_t newmode);

int
SMBC_utimes_ctx(SMBCCTX *context,
                const char *fname,
                struct timeval *tbuf);

int
SMBC_unlink_ctx(SMBCCTX *context,
                const char *fname);

int
SMBC_rename_ctx(SMBCCTX *ocontext,
                const char *oname, 
                SMBCCTX *ncontext,
                const char *nname);


/* Functions in libsmb_file.c */
SMBCFILE *
SMBC_open_ctx(SMBCCTX *context,
              const char *fname,
              int flags,
              mode_t mode);

SMBCFILE *
SMBC_creat_ctx(SMBCCTX *context,
               const char *path,
               mode_t mode);

ssize_t
SMBC_read_ctx(SMBCCTX *context,
              SMBCFILE *file,
              void *buf,
              size_t count);

ssize_t
SMBC_write_ctx(SMBCCTX *context,
               SMBCFILE *file,
               const void *buf,
               size_t count);

int
SMBC_close_ctx(SMBCCTX *context,
               SMBCFILE *file);

bool
SMBC_getatr(SMBCCTX * context,
            SMBCSRV *srv,
            char *path,
            uint16 *mode,
            SMB_OFF_T *size,
            struct timespec *create_time_ts,
            struct timespec *access_time_ts,
            struct timespec *write_time_ts,
            struct timespec *change_time_ts,
            SMB_INO_T *ino);

bool
SMBC_setatr(SMBCCTX * context, SMBCSRV *srv, char *path, 
            time_t create_time,
            time_t access_time,
            time_t write_time,
            time_t change_time,
            uint16 mode);

off_t
SMBC_lseek_ctx(SMBCCTX *context,
               SMBCFILE *file,
               off_t offset,
               int whence);

int
SMBC_ftruncate_ctx(SMBCCTX *context,
                   SMBCFILE *file,
                   off_t length);


/* Functions in libsmb_misc.c */
int
SMBC_dlist_contains(SMBCFILE * list, SMBCFILE *p);

int
SMBC_errno(SMBCCTX *context,
           struct cli_state *c);


/* Functions in libsmb_path.c */
int
SMBC_parse_path(TALLOC_CTX *ctx,
		SMBCCTX *context,
                const char *fname,
                char **pp_workgroup,
                char **pp_server,
                char **pp_share,
                char **pp_path,
		char **pp_user,
                char **pp_password,
                char **pp_options);


/* Functions in libsmb_printjob.c */
SMBCFILE *
SMBC_open_print_job_ctx(SMBCCTX *context,
                        const char *fname);

int
SMBC_print_file_ctx(SMBCCTX *c_file,
                    const char *fname,
                    SMBCCTX *c_print,
                    const char *printq);

int
SMBC_list_print_jobs_ctx(SMBCCTX *context,
                         const char *fname,
                         smbc_list_print_job_fn fn);

int
SMBC_unlink_print_job_ctx(SMBCCTX *context,
                          const char *fname,
                          int id);


/* Functions in libsmb_server.c */
int
SMBC_check_server(SMBCCTX * context,
                  SMBCSRV * server);

int
SMBC_remove_unused_server(SMBCCTX * context,
                          SMBCSRV * srv);

void
SMBC_call_auth_fn(TALLOC_CTX *ctx,
                  SMBCCTX *context,
                  const char *server,
                  const char *share,
                  char **pp_workgroup,
                  char **pp_username,
                  char **pp_password);

void
SMBC_get_auth_data(const char *server, const char *share,
                   char *workgroup_buf, int workgroup_buf_len,
                   char *username_buf, int username_buf_len,
                   char *password_buf, int password_buf_len);

SMBCSRV *
SMBC_find_server(TALLOC_CTX *ctx,
                 SMBCCTX *context,
                 const char *server,
                 const char *share,
                 char **pp_workgroup,
                 char **pp_username,
                 char **pp_password);

SMBCSRV *
SMBC_server(TALLOC_CTX *ctx,
            SMBCCTX *context,
            bool connect_if_not_found,
            const char *server,
            const char *share,
            char **pp_workgroup,
            char **pp_username,
            char **pp_password);

SMBCSRV *
SMBC_attr_server(TALLOC_CTX *ctx,
                 SMBCCTX *context,
                 const char *server,
                 const char *share,
                 char **pp_workgroup,
                 char **pp_username,
                 char **pp_password);


/* Functions in libsmb_stat.c */
int
SMBC_stat_ctx(SMBCCTX *context,
              const char *fname,
              struct stat *st);

int
SMBC_fstat_ctx(SMBCCTX *context,
               SMBCFILE *file,
               struct stat *st);


int
SMBC_statvfs_ctx(SMBCCTX *context,
                 char *path,
                 struct statvfs *st);


int
SMBC_fstatvfs_ctx(SMBCCTX *context,
                  SMBCFILE *file,
                  struct statvfs *st);


/* Functions in libsmb_xattr.c */
int
SMBC_setxattr_ctx(SMBCCTX *context,
                  const char *fname,
                  const char *name,
                  const void *value,
                  size_t size,
                  int flags);

int
SMBC_getxattr_ctx(SMBCCTX *context,
                  const char *fname,
                  const char *name,
                  const void *value,
                  size_t size);

int
SMBC_removexattr_ctx(SMBCCTX *context,
                     const char *fname,
                     const char *name);

int
SMBC_listxattr_ctx(SMBCCTX *context,
                   const char *fname,
                   char *list,
                   size_t size);


#endif
