/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef _DISK_SHARE_
#define _DISK_SHARE_

#define ADMIN_ORDER 0
#define MAX_ACCOUNT_NUM 6

#define SHARE_LAYER MOUNT_LAYER+1

// Support Protocol
#define PROTOCOL_CIFS "cifs"
#define PROTOCOL_FTP "ftp"
#define PROTOCOL_MEDIASERVER "dms"
#ifdef RTCONFIG_WEBDAV_OLD
#define PROTOCOL_WEBDAV "webdav"
#define MAX_PROTOCOL_NUM 4
#else
#define MAX_PROTOCOL_NUM 3
#endif

#define PROTOCOL_CIFS_BIT 0
#define PROTOCOL_FTP_BIT 1
#define PROTOCOL_MEDIASERVER_BIT 2
#ifdef RTCONFIG_WEBDAV_OLD
#define PROTOCOL_WEBDAV_BIT 3
#endif

#define DEFAULT_SAMBA_RIGHT 3
#define DEFAULT_FTP_RIGHT 3
#define DEFAULT_DMS_RIGHT 1
#ifdef RTCONFIG_WEBDAV_OLD
#define DEFAULT_WEBDAV_RIGHT 3
#endif

extern void set_file_integrity(const char *const file_name);
extern int check_file_integrity(const char *const file_name);

extern int get_account_list(int *, char ***);
extern int get_folder_list(const char *const, int *, char ***);
extern int get_all_folder(const char *const, int *, char ***);
extern int get_var_file_name(const char *const account, const char *const path, char **file_name);
extern void free_2_dimension_list(int *, char ***);

extern int initial_folder_list(const char *const);
extern int initial_var_file(const char *const, const char *const);
extern int initial_all_var_file(const char *const);
extern int test_of_var_files(const char *const);
extern int create_if_no_var_files(const char *const);
extern int modify_if_exist_new_folder(const char *const, const char *const);

extern int get_permission(const char *const, const char *const, const char *const, const char *const);
extern int set_permission(const char *const, const char *const, const char *const, const char *const, const int);

extern int add_account(const char *const, const char *const);
extern int del_account(const char *const);
extern int mod_account(const char *const, const char *const, const char *const);
extern int test_if_exist_account(const char *const);

extern int add_folder(const char *const, const char *const, const char *const);
extern int del_folder(const char *const, const char *const);
extern int mod_folder(const char *const, const char *const, const char *const);
extern int test_if_exist_share(const char *const, const char *const);

extern int how_many_layer(const char *const, char **, char **);

#endif // _DISK_SHARE_
