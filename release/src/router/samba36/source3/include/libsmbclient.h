/*=====================================================================
  Unix SMB/Netbios implementation.
  SMB client library API definitions
  Copyright (C) Andrew Tridgell 1998
  Copyright (C) Richard Sharpe 2000
  Copyright (C) John Terpsra 2000
  Copyright (C) Tom Jansen (Ninja ISD) 2002 
  Copyright (C) Derrell Lipman 2003-2008

   
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.
   
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
   
  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
  =====================================================================*/

#ifndef SMBCLIENT_H_INCLUDED
#define SMBCLIENT_H_INCLUDED

#undef DEPRECATED_SMBC_INTERFACE
#if ! defined(__LIBSMBCLIENT_INTERNAL__) && defined(__GNUC__)
# define DEPRECATED_SMBC_INTERFACE      __attribute__ ((deprecated))
#else
# define DEPRECATED_SMBC_INTERFACE
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*-------------------------------------------------------------------*/
/* The following are special comments to instruct DOXYGEN (automated 
 * documentation tool:
*/
/** \defgroup libsmbclient
*/
/** \defgroup structure Data Structures Type and Constants
*   \ingroup libsmbclient
*   Data structures, types, and constants
*/
/** \defgroup callback Callback function types
*   \ingroup libsmbclient
*   Callback functions
*/
/** \defgroup file File Functions
*   \ingroup libsmbclient
*   Functions used to access individual file contents
*/
/** \defgroup directory Directory Functions
*   \ingroup libsmbclient
*   Functions used to access directory entries
*/
/** \defgroup attribute Attributes Functions
*   \ingroup libsmbclient
*   Functions used to view or change file and directory attributes
*/
/** \defgroup print Print Functions
*   \ingroup libsmbclient
*   Functions used to access printing functionality
*/
/** \defgroup misc Miscellaneous Functions
*   \ingroup libsmbclient
*   Functions that don't fit in to other categories
*/
/*-------------------------------------------------------------------*/   

/* Make sure we have the following includes for now ... */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <utime.h>

#define SMBC_BASE_FD        10000 /* smallest file descriptor returned */

#define SMBC_WORKGROUP      1
#define SMBC_SERVER         2
#define SMBC_FILE_SHARE     3
#define SMBC_PRINTER_SHARE  4
#define SMBC_COMMS_SHARE    5
#define SMBC_IPC_SHARE      6
#define SMBC_DIR            7
#define SMBC_FILE           8
#define SMBC_LINK           9

/**@ingroup structure
 * Structure that represents a directory entry.
 *
 */
struct smbc_dirent 
{
	/** Type of entity.
	    SMBC_WORKGROUP=1,
	    SMBC_SERVER=2, 
	    SMBC_FILE_SHARE=3,
	    SMBC_PRINTER_SHARE=4,
	    SMBC_COMMS_SHARE=5,
	    SMBC_IPC_SHARE=6,
	    SMBC_DIR=7,
	    SMBC_FILE=8,
	    SMBC_LINK=9,*/ 
	unsigned int smbc_type; 

	/** Length of this smbc_dirent in bytes
	 */
	unsigned int dirlen;
	/** The length of the comment string in bytes (does not include
	 *  null terminator)
	 */
	unsigned int commentlen;
	/** Points to the null terminated comment string 
	 */
	char *comment;
	/** The length of the name string in bytes (does not include
	 *  null terminator)
	 */
	unsigned int namelen;
	/** Points to the null terminated name string 
	 */
	char name[1];
};

/*
 * Flags for smbc_setxattr()
 *   Specify a bitwise OR of these, or 0 to add or replace as necessary
 */
#define SMBC_XATTR_FLAG_CREATE       0x1 /* fail if attr already exists */
#define SMBC_XATTR_FLAG_REPLACE      0x2 /* fail if attr does not exist */


/*
 * Mappings of the DOS mode bits, as returned by smbc_getxattr() when the
 * attribute name "system.dos_attr.mode" (or "system.dos_attr.*" or
 * "system.*") is specified.
 */
#define SMBC_DOS_MODE_READONLY       0x01
#define SMBC_DOS_MODE_HIDDEN         0x02
#define SMBC_DOS_MODE_SYSTEM         0x04
#define SMBC_DOS_MODE_VOLUME_ID      0x08
#define SMBC_DOS_MODE_DIRECTORY      0x10
#define SMBC_DOS_MODE_ARCHIVE        0x20

/*
 * Valid values for the option "open_share_mode", when calling
 * smbc_setOptionOpenShareMode()
 */
typedef enum smbc_share_mode
{
    SMBC_SHAREMODE_DENY_DOS     = 0,
    SMBC_SHAREMODE_DENY_ALL     = 1,
    SMBC_SHAREMODE_DENY_WRITE   = 2,
    SMBC_SHAREMODE_DENY_READ    = 3,
    SMBC_SHAREMODE_DENY_NONE    = 4,
    SMBC_SHAREMODE_DENY_FCB     = 7
} smbc_share_mode;


/**
 * Values for option SMB Encryption Level, as set and retrieved with
 * smbc_setOptionSmbEncryptionLevel() and smbc_getOptionSmbEncryptionLevel()
 */
typedef enum smbc_smb_encrypt_level
{
    SMBC_ENCRYPTLEVEL_NONE      = 0,
    SMBC_ENCRYPTLEVEL_REQUEST   = 1,
    SMBC_ENCRYPTLEVEL_REQUIRE   = 2
} smbc_smb_encrypt_level;


/**
 * Capabilities set in the f_flag field of struct statvfs, from
 * smbc_statvfs(). These may be OR-ed together to reflect a full set of
 * available capabilities.
 */
typedef enum smbc_vfs_feature
{
    /* Defined by POSIX or in Linux include files (low-order bits) */
    SMBC_VFS_FEATURE_RDONLY         = (1 << 0),

    /* Specific to libsmbclient (high-order bits) */
    SMBC_VFS_FEATURE_DFS              = (1 << 28),
    SMBC_VFS_FEATURE_CASE_INSENSITIVE = (1 << 29),
    SMBC_VFS_FEATURE_NO_UNIXCIFS      = (1 << 30)
} smbc_vfs_feature;

typedef int smbc_bool;


#ifndef ENOATTR
# define ENOATTR ENOENT        /* No such attribute */
#endif




/**@ingroup structure
 * Structure that represents a print job.
 *
 */
#ifndef _CLIENT_H
struct print_job_info 
{
	/** numeric ID of the print job
	 */
	unsigned short id;
    
	/** represents print job priority (lower numbers mean higher priority)
	 */
	unsigned short priority;
    
	/** Size of the print job
	 */
	size_t size;
    
	/** Name of the user that owns the print job
	 */
	char user[128];
  
	/** Name of the print job. This will have no name if an anonymous print
	 *  file was opened. Ie smb://server/printer
	 */
	char name[128];

	/** Time the print job was spooled
	 */
	time_t t;
};
#endif /* _CLIENT_H */


/**@ingroup structure
 * Server handle 
 */
typedef struct _SMBCSRV  SMBCSRV;

/**@ingroup structure
 * File or directory handle 
 */
typedef struct _SMBCFILE SMBCFILE;

/**@ingroup structure
 * File or directory handle 
 */
typedef struct _SMBCCTX SMBCCTX;


/*
 * Flags for SMBCCTX->flags
 *
 * NEW CODE SHOULD NOT DIRECTLY MANIPULATE THE CONTEXT STRUCTURE.
 * Instead, use:
 *   smbc_setOptionUseKerberos()
 *   smbc_getOptionUseKerberos()
 *   smbc_setOptionFallbackAfterKerberos()
 *   smbc_getOptionFallbackAFterKerberos()
 *   smbc_setOptionNoAutoAnonymousLogin()
 *   smbc_getOptionNoAutoAnonymousLogin()
 *   smbc_setOptionUseCCache()
 *   smbc_getOptionUseCCache()
 */
# define SMB_CTX_FLAG_USE_KERBEROS (1 << 0)
# define SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS (1 << 1)
# define SMBCCTX_FLAG_NO_AUTO_ANONYMOUS_LOGON (1 << 2)
# define SMB_CTX_FLAG_USE_CCACHE (1 << 3)



/**@ingroup callback
 * Authentication callback function type (traditional method)
 * 
 * Type for the the authentication function called by the library to
 * obtain authentication credentals
 *
 * For kerberos support the function should just be called without
 * prompting the user for credentials. Which means a simple 'return'
 * should work. Take a look at examples/libsmbclient/get_auth_data_fn.h
 * and examples/libsmbclient/testbrowse.c.
 *
 * @param srv       Server being authenticated to
 *
 * @param shr       Share being authenticated to
 *
 * @param wg        Pointer to buffer containing a "hint" for the
 *                  workgroup to be authenticated.  Should be filled in
 *                  with the correct workgroup if the hint is wrong.
 * 
 * @param wglen     The size of the workgroup buffer in bytes
 *
 * @param un        Pointer to buffer containing a "hint" for the
 *                  user name to be use for authentication. Should be
 *                  filled in with the correct workgroup if the hint is
 *                  wrong.
 * 
 * @param unlen     The size of the username buffer in bytes
 *
 * @param pw        Pointer to buffer containing to which password 
 *                  copied
 * 
 * @param pwlen     The size of the password buffer in bytes
 *           
 */
typedef void (*smbc_get_auth_data_fn)(const char *srv, 
                                      const char *shr,
                                      char *wg, int wglen, 
                                      char *un, int unlen,
                                      char *pw, int pwlen);
/**@ingroup callback
 * Authentication callback function type (method that includes context)
 * 
 * Type for the the authentication function called by the library to
 * obtain authentication credentals
 *
 * For kerberos support the function should just be called without
 * prompting the user for credentials. Which means a simple 'return'
 * should work. Take a look at examples/libsmbclient/get_auth_data_fn.h
 * and examples/libsmbclient/testbrowse.c.
 *
 * @param c         Pointer to the smb context
 *
 * @param srv       Server being authenticated to
 *
 * @param shr       Share being authenticated to
 *
 * @param wg        Pointer to buffer containing a "hint" for the
 *                  workgroup to be authenticated.  Should be filled in
 *                  with the correct workgroup if the hint is wrong.
 * 
 * @param wglen     The size of the workgroup buffer in bytes
 *
 * @param un        Pointer to buffer containing a "hint" for the
 *                  user name to be use for authentication. Should be
 *                  filled in with the correct workgroup if the hint is
 *                  wrong.
 * 
 * @param unlen     The size of the username buffer in bytes
 *
 * @param pw        Pointer to buffer containing to which password 
 *                  copied
 * 
 * @param pwlen     The size of the password buffer in bytes
 *           
 */
typedef void (*smbc_get_auth_data_with_context_fn)(SMBCCTX *c,
                                                   const char *srv, 
                                                   const char *shr,
                                                   char *wg, int wglen, 
                                                   char *un, int unlen,
                                                   char *pw, int pwlen);


/**@ingroup callback
 * Print job info callback function type.
 *
 * @param i         pointer to print job information structure
 *
 */ 
typedef void (*smbc_list_print_job_fn)(struct print_job_info *i);
		

/**@ingroup callback
 * Check if a server is still good
 *
 * @param c         pointer to smb context
 *
 * @param srv       pointer to server to check
 *
 * @return          0 when connection is good. 1 on error.
 *
 */ 
typedef int (*smbc_check_server_fn)(SMBCCTX * c, SMBCSRV *srv);

/**@ingroup callback
 * Remove a server if unused
 *
 * @param c         pointer to smb context
 *
 * @param srv       pointer to server to remove
 *
 * @return          0 on success. 1 on failure.
 *
 */ 
typedef int (*smbc_remove_unused_server_fn)(SMBCCTX * c, SMBCSRV *srv);


/**@ingroup callback
 * Add a server to the cache system
 *
 * @param c         pointer to smb context
 *
 * @param srv       pointer to server to add
 *
 * @param server    server name 
 *
 * @param share     share name
 *
 * @param workgroup workgroup used to connect
 *
 * @param username  username used to connect
 *
 * @return          0 on success. 1 on failure.
 *
 */ 
typedef int (*smbc_add_cached_srv_fn)   (SMBCCTX * c, SMBCSRV *srv, 
				    const char * server, const char * share,
				    const char * workgroup, const char * username);

/**@ingroup callback
 * Look up a server in the cache system
 *
 * @param c         pointer to smb context
 *
 * @param server    server name to match
 *
 * @param share     share name to match
 *
 * @param workgroup workgroup to match
 *
 * @param username  username to match
 *
 * @return          pointer to SMBCSRV on success. NULL on failure.
 *
 */ 
typedef SMBCSRV * (*smbc_get_cached_srv_fn)   (SMBCCTX * c, const char * server,
					       const char * share, const char * workgroup,
                                               const char * username);

/**@ingroup callback
 * Check if a server is still good
 *
 * @param c         pointer to smb context
 *
 * @param srv       pointer to server to remove
 *
 * @return          0 when found and removed. 1 on failure.
 *
 */ 
typedef int (*smbc_remove_cached_srv_fn)(SMBCCTX * c, SMBCSRV *srv);


/**@ingroup callback
 * Try to remove all servers from the cache system and disconnect
 *
 * @param c         pointer to smb context
 *
 * @return          0 when found and removed. 1 on failure.
 *
 */ 
typedef int (*smbc_purge_cached_fn)     (SMBCCTX * c);



/*****************************************
 * Getters and setters for CONFIGURATION *
 *****************************************/

/** Get the debug level */
int
smbc_getDebug(SMBCCTX *c);

/** Set the debug level */
void
smbc_setDebug(SMBCCTX *c, int debug);

/** Get the netbios name used for making connections */
char *
smbc_getNetbiosName(SMBCCTX *c);

/** Set the netbios name used for making connections */
void
smbc_setNetbiosName(SMBCCTX *c, char * netbios_name);

/** Get the workgroup used for making connections */
char *
smbc_getWorkgroup(SMBCCTX *c);

/** Set the workgroup used for making connections */
void smbc_setWorkgroup(SMBCCTX *c, char * workgroup);

/** Get the username used for making connections */
char *
smbc_getUser(SMBCCTX *c);

/** Set the username used for making connections */
void
smbc_setUser(SMBCCTX *c, char * user);

/**
 * Get the timeout used for waiting on connections and response data
 * (in milliseconds)
 */
int
smbc_getTimeout(SMBCCTX *c);

/**
 * Set the timeout used for waiting on connections and response data
 * (in milliseconds)
 */
void
smbc_setTimeout(SMBCCTX *c, int timeout);



/***********************************
 * Getters and setters for OPTIONS *
 ***********************************/

/** Get whether to log to standard error instead of standard output */
smbc_bool
smbc_getOptionDebugToStderr(SMBCCTX *c);

/** Set whether to log to standard error instead of standard output */
void
smbc_setOptionDebugToStderr(SMBCCTX *c, smbc_bool b);

/**
 * Get whether to use new-style time attribute names, e.g. WRITE_TIME rather
 * than the old-style names such as M_TIME.  This allows also setting/getting
 * CREATE_TIME which was previously unimplemented.  (Note that the old C_TIME
 * was supposed to be CHANGE_TIME but was confused and sometimes referred to
 * CREATE_TIME.)
 */
smbc_bool
smbc_getOptionFullTimeNames(SMBCCTX *c);

/**
 * Set whether to use new-style time attribute names, e.g. WRITE_TIME rather
 * than the old-style names such as M_TIME.  This allows also setting/getting
 * CREATE_TIME which was previously unimplemented.  (Note that the old C_TIME
 * was supposed to be CHANGE_TIME but was confused and sometimes referred to
 * CREATE_TIME.)
 */
void
smbc_setOptionFullTimeNames(SMBCCTX *c, smbc_bool b);

/**
 * Get the share mode to use for files opened with SMBC_open_ctx().  The
 * default is SMBC_SHAREMODE_DENY_NONE.
 */
smbc_share_mode
smbc_getOptionOpenShareMode(SMBCCTX *c);

/**
 * Set the share mode to use for files opened with SMBC_open_ctx().  The
 * default is SMBC_SHAREMODE_DENY_NONE.
 */
void
smbc_setOptionOpenShareMode(SMBCCTX *c, smbc_share_mode share_mode);

/** Retrieve a previously saved user data handle */
void *
smbc_getOptionUserData(SMBCCTX *c);

/** Save a user data handle */
void
smbc_setOptionUserData(SMBCCTX *c, void *user_data);

/** Get the encoded value for encryption level. */
smbc_smb_encrypt_level
smbc_getOptionSmbEncryptionLevel(SMBCCTX *c);

/** Set the encoded value for encryption level. */
void
smbc_setOptionSmbEncryptionLevel(SMBCCTX *c, smbc_smb_encrypt_level level);

/**
 * Get whether to treat file names as case-sensitive if we can't determine
 * when connecting to the remote share whether the file system is case
 * sensitive. This defaults to FALSE since it's most likely that if we can't
 * retrieve the file system attributes, it's a very old file system that does
 * not support case sensitivity.
 */
smbc_bool
smbc_getOptionCaseSensitive(SMBCCTX *c);

/**
 * Set whether to treat file names as case-sensitive if we can't determine
 * when connecting to the remote share whether the file system is case
 * sensitive. This defaults to FALSE since it's most likely that if we can't
 * retrieve the file system attributes, it's a very old file system that does
 * not support case sensitivity.
 */
void
smbc_setOptionCaseSensitive(SMBCCTX *c, smbc_bool b);


/**
 * Get from how many local master browsers should the list of workgroups be
 * retrieved.  It can take up to 12 minutes or longer after a server becomes a
 * local master browser, for it to have the entire browse list (the list of
 * workgroups/domains) from an entire network.  Since a client never knows
 * which local master browser will be found first, the one which is found
 * first and used to retrieve a browse list may have an incomplete or empty
 * browse list.  By requesting the browse list from multiple local master
 * browsers, a more complete list can be generated.  For small networks (few
 * workgroups), it is recommended that this value be set to 0, causing the
 * browse lists from all found local master browsers to be retrieved and
 * merged.  For networks with many workgroups, a suitable value for this
 * variable is probably somewhere around 3. (Default: 3).
 */
int
smbc_getOptionBrowseMaxLmbCount(SMBCCTX *c);

/**
 * Set from how many local master browsers should the list of workgroups be
 * retrieved.  It can take up to 12 minutes or longer after a server becomes a
 * local master browser, for it to have the entire browse list (the list of
 * workgroups/domains) from an entire network.  Since a client never knows
 * which local master browser will be found first, the one which is found
 * first and used to retrieve a browse list may have an incomplete or empty
 * browse list.  By requesting the browse list from multiple local master
 * browsers, a more complete list can be generated.  For small networks (few
 * workgroups), it is recommended that this value be set to 0, causing the
 * browse lists from all found local master browsers to be retrieved and
 * merged.  For networks with many workgroups, a suitable value for this
 * variable is probably somewhere around 3. (Default: 3).
 */
void
smbc_setOptionBrowseMaxLmbCount(SMBCCTX *c, int count);

/**
 * Get whether to url-encode readdir entries.
 *
 * There is a difference in the desired return strings from
 * smbc_readdir() depending upon whether the filenames are to
 * be displayed to the user, or whether they are to be
 * appended to the path name passed to smbc_opendir() to call
 * a further smbc_ function (e.g. open the file with
 * smbc_open()).  In the former case, the filename should be
 * in "human readable" form.  In the latter case, the smbc_
 * functions expect a URL which must be url-encoded.  Those
 * functions decode the URL.  If, for example, smbc_readdir()
 * returned a file name of "abc%20def.txt", passing a path
 * with this file name attached to smbc_open() would cause
 * smbc_open to attempt to open the file "abc def.txt" since
 * the %20 is decoded into a space.
 *
 * Set this option to True if the names returned by
 * smbc_readdir() should be url-encoded such that they can be
 * passed back to another smbc_ call.  Set it to False if the
 * names returned by smbc_readdir() are to be presented to the
 * user.
 *
 * For backwards compatibility, this option defaults to False.
 */
smbc_bool
smbc_getOptionUrlEncodeReaddirEntries(SMBCCTX *c);

/**
 * Set whether to url-encode readdir entries.
 *
 * There is a difference in the desired return strings from
 * smbc_readdir() depending upon whether the filenames are to
 * be displayed to the user, or whether they are to be
 * appended to the path name passed to smbc_opendir() to call
 * a further smbc_ function (e.g. open the file with
 * smbc_open()).  In the former case, the filename should be
 * in "human readable" form.  In the latter case, the smbc_
 * functions expect a URL which must be url-encoded.  Those
 * functions decode the URL.  If, for example, smbc_readdir()
 * returned a file name of "abc%20def.txt", passing a path
 * with this file name attached to smbc_open() would cause
 * smbc_open to attempt to open the file "abc def.txt" since
 * the %20 is decoded into a space.
 *
 * Set this option to True if the names returned by
 * smbc_readdir() should be url-encoded such that they can be
 * passed back to another smbc_ call.  Set it to False if the
 * names returned by smbc_readdir() are to be presented to the
 * user.
 *
 * For backwards compatibility, this option defaults to False.
 */
void
smbc_setOptionUrlEncodeReaddirEntries(SMBCCTX *c, smbc_bool b);

/**
 * Get whether to use the same connection for all shares on a server.
 *
 * Some Windows versions appear to have a limit to the number
 * of concurrent SESSIONs and/or TREE CONNECTions.  In
 * one-shot programs (i.e. the program runs and then quickly
 * ends, thereby shutting down all connections), it is
 * probably reasonable to establish a new connection for each
 * share.  In long-running applications, the limitation can be
 * avoided by using only a single connection to each server,
 * and issuing a new TREE CONNECT when the share is accessed.
 */
smbc_bool
smbc_getOptionOneSharePerServer(SMBCCTX *c);

/**
 * Set whether to use the same connection for all shares on a server.
 *
 * Some Windows versions appear to have a limit to the number
 * of concurrent SESSIONs and/or TREE CONNECTions.  In
 * one-shot programs (i.e. the program runs and then quickly
 * ends, thereby shutting down all connections), it is
 * probably reasonable to establish a new connection for each
 * share.  In long-running applications, the limitation can be
 * avoided by using only a single connection to each server,
 * and issuing a new TREE CONNECT when the share is accessed.
 */
void
smbc_setOptionOneSharePerServer(SMBCCTX *c, smbc_bool b);

/** Get whether to enable use of kerberos */
smbc_bool
smbc_getOptionUseKerberos(SMBCCTX *c);

/** Set whether to enable use of kerberos */
void
smbc_setOptionUseKerberos(SMBCCTX *c, smbc_bool b);

/** Get whether to fallback after kerberos */
smbc_bool
smbc_getOptionFallbackAfterKerberos(SMBCCTX *c);

/** Set whether to fallback after kerberos */
void
smbc_setOptionFallbackAfterKerberos(SMBCCTX *c, smbc_bool b);

/** Get whether to automatically select anonymous login */
smbc_bool
smbc_getOptionNoAutoAnonymousLogin(SMBCCTX *c);

/** Set whether to automatically select anonymous login */
void
smbc_setOptionNoAutoAnonymousLogin(SMBCCTX *c, smbc_bool b);

/** Get whether to enable use of the winbind ccache */
smbc_bool
smbc_getOptionUseCCache(SMBCCTX *c);

/** Set whether to enable use of the winbind ccache */
void
smbc_setOptionUseCCache(SMBCCTX *c, smbc_bool b);



/*************************************
 * Getters and setters for FUNCTIONS *
 *************************************/

/** Get the function for obtaining authentication data */
smbc_get_auth_data_fn smbc_getFunctionAuthData(SMBCCTX *c);

/** Set the function for obtaining authentication data */
void smbc_setFunctionAuthData(SMBCCTX *c, smbc_get_auth_data_fn fn);

/** Get the new-style authentication function which includes the context. */
smbc_get_auth_data_with_context_fn
smbc_getFunctionAuthDataWithContext(SMBCCTX *c);

/** Set the new-style authentication function which includes the context. */
void
smbc_setFunctionAuthDataWithContext(SMBCCTX *c,
                                    smbc_get_auth_data_with_context_fn fn);

/** Get the function for checking if a server is still good */
smbc_check_server_fn smbc_getFunctionCheckServer(SMBCCTX *c);

/** Set the function for checking if a server is still good */
void smbc_setFunctionCheckServer(SMBCCTX *c, smbc_check_server_fn fn);

/** Get the function for removing a server if unused */
smbc_remove_unused_server_fn smbc_getFunctionRemoveUnusedServer(SMBCCTX *c);

/** Set the function for removing a server if unused */
void smbc_setFunctionRemoveUnusedServer(SMBCCTX *c,
                                        smbc_remove_unused_server_fn fn);

/** Get the function for adding a cached server */
smbc_add_cached_srv_fn smbc_getFunctionAddCachedServer(SMBCCTX *c);

/** Set the function for adding a cached server */
void smbc_setFunctionAddCachedServer(SMBCCTX *c, smbc_add_cached_srv_fn fn);

/** Get the function for server cache lookup */
smbc_get_cached_srv_fn smbc_getFunctionGetCachedServer(SMBCCTX *c);

/** Set the function for server cache lookup */
void smbc_setFunctionGetCachedServer(SMBCCTX *c, smbc_get_cached_srv_fn fn);

/** Get the function for server cache removal */
smbc_remove_cached_srv_fn smbc_getFunctionRemoveCachedServer(SMBCCTX *c);

/** Set the function for server cache removal */
void smbc_setFunctionRemoveCachedServer(SMBCCTX *c,
                                        smbc_remove_cached_srv_fn fn);

/**
 * Get the function for server cache purging.  This function tries to
 * remove all cached servers (e.g. on disconnect)
 */
smbc_purge_cached_fn smbc_getFunctionPurgeCachedServers(SMBCCTX *c);

/**
 * Set the function for server cache purging.  This function tries to
 * remove all cached servers (e.g. on disconnect)
 */
void smbc_setFunctionPurgeCachedServers(SMBCCTX *c,
                                        smbc_purge_cached_fn fn);

/** Get the function to store private data of the server cache */
struct smbc_server_cache * smbc_getServerCacheData(SMBCCTX *c);

/** Set the function to store private data of the server cache */
void smbc_setServerCacheData(SMBCCTX *c, struct smbc_server_cache * cache);



/*****************************************************************
 * Callable functions for files.                                 *
 * Each callable has a function signature typedef, a declaration *
 * for the getter, and a declaration for the setter.             *
 *****************************************************************/

typedef SMBCFILE * (*smbc_open_fn)(SMBCCTX *c,
                                   const char *fname,
                                   int flags,
                                   mode_t mode);
smbc_open_fn smbc_getFunctionOpen(SMBCCTX *c);
void smbc_setFunctionOpen(SMBCCTX *c, smbc_open_fn fn);

typedef SMBCFILE * (*smbc_creat_fn)(SMBCCTX *c,
                                    const char *path,
                                    mode_t mode);
smbc_creat_fn smbc_getFunctionCreat(SMBCCTX *c);
void smbc_setFunctionCreat(SMBCCTX *c, smbc_creat_fn);

typedef ssize_t (*smbc_read_fn)(SMBCCTX *c,
                                SMBCFILE *file,
                                void *buf,
                                size_t count);
smbc_read_fn smbc_getFunctionRead(SMBCCTX *c);
void smbc_setFunctionRead(SMBCCTX *c, smbc_read_fn fn);

typedef ssize_t (*smbc_write_fn)(SMBCCTX *c,
                                 SMBCFILE *file,
                                 const void *buf,
                                 size_t count);
smbc_write_fn smbc_getFunctionWrite(SMBCCTX *c);
void smbc_setFunctionWrite(SMBCCTX *c, smbc_write_fn fn);

typedef int (*smbc_unlink_fn)(SMBCCTX *c,
                              const char *fname);
smbc_unlink_fn smbc_getFunctionUnlink(SMBCCTX *c);
void smbc_setFunctionUnlink(SMBCCTX *c, smbc_unlink_fn fn);

typedef int (*smbc_rename_fn)(SMBCCTX *ocontext,
                              const char *oname,
                              SMBCCTX *ncontext,
                              const char *nname);
smbc_rename_fn smbc_getFunctionRename(SMBCCTX *c);
void smbc_setFunctionRename(SMBCCTX *c, smbc_rename_fn fn);

typedef off_t (*smbc_lseek_fn)(SMBCCTX *c,
                               SMBCFILE * file,
                               off_t offset,
                               int whence);
smbc_lseek_fn smbc_getFunctionLseek(SMBCCTX *c);
void smbc_setFunctionLseek(SMBCCTX *c, smbc_lseek_fn fn);

typedef int (*smbc_stat_fn)(SMBCCTX *c,
                            const char *fname,
                            struct stat *st);
smbc_stat_fn smbc_getFunctionStat(SMBCCTX *c);
void smbc_setFunctionStat(SMBCCTX *c, smbc_stat_fn fn);

typedef int (*smbc_fstat_fn)(SMBCCTX *c,
                             SMBCFILE *file,
                             struct stat *st);
smbc_fstat_fn smbc_getFunctionFstat(SMBCCTX *c);
void smbc_setFunctionFstat(SMBCCTX *c, smbc_fstat_fn fn);

typedef int (*smbc_statvfs_fn)(SMBCCTX *c,
                               char *path,
                               struct statvfs *st);
smbc_statvfs_fn smbc_getFunctionStatVFS(SMBCCTX *c);
void smbc_setFunctionStatVFS(SMBCCTX *c, smbc_statvfs_fn fn);

typedef int (*smbc_fstatvfs_fn)(SMBCCTX *c,
                                SMBCFILE *file,
                                struct statvfs *st);
smbc_fstatvfs_fn smbc_getFunctionFstatVFS(SMBCCTX *c);
void smbc_setFunctionFstatVFS(SMBCCTX *c, smbc_fstatvfs_fn fn);

typedef int (*smbc_ftruncate_fn)(SMBCCTX *c,
                                 SMBCFILE *f,
                                 off_t size);
smbc_ftruncate_fn smbc_getFunctionFtruncate(SMBCCTX *c);
void smbc_setFunctionFtruncate(SMBCCTX *c, smbc_ftruncate_fn fn);

typedef int (*smbc_close_fn)(SMBCCTX *c,
                             SMBCFILE *file);
smbc_close_fn smbc_getFunctionClose(SMBCCTX *c);
void smbc_setFunctionClose(SMBCCTX *c, smbc_close_fn fn);



/*****************************************************************
 * Callable functions for directories.                           *
 * Each callable has a function signature typedef, a declaration *
 * for the getter, and a declaration for the setter.             *
 *****************************************************************/

typedef SMBCFILE * (*smbc_opendir_fn)(SMBCCTX *c,
                                      const char *fname);
smbc_opendir_fn smbc_getFunctionOpendir(SMBCCTX *c);
void smbc_setFunctionOpendir(SMBCCTX *c, smbc_opendir_fn fn);

typedef int (*smbc_closedir_fn)(SMBCCTX *c,
                                SMBCFILE *dir);
smbc_closedir_fn smbc_getFunctionClosedir(SMBCCTX *c);
void smbc_setFunctionClosedir(SMBCCTX *c, smbc_closedir_fn fn);

typedef struct smbc_dirent * (*smbc_readdir_fn)(SMBCCTX *c,
                                                SMBCFILE *dir);
smbc_readdir_fn smbc_getFunctionReaddir(SMBCCTX *c);
void smbc_setFunctionReaddir(SMBCCTX *c, smbc_readdir_fn fn);

typedef int (*smbc_getdents_fn)(SMBCCTX *c,
                                SMBCFILE *dir,
                                struct smbc_dirent *dirp,
                                int count);
smbc_getdents_fn smbc_getFunctionGetdents(SMBCCTX *c);
void smbc_setFunctionGetdents(SMBCCTX *c, smbc_getdents_fn fn);

typedef int (*smbc_mkdir_fn)(SMBCCTX *c,
                             const char *fname,
                             mode_t mode);
smbc_mkdir_fn smbc_getFunctionMkdir(SMBCCTX *c);
void smbc_setFunctionMkdir(SMBCCTX *c, smbc_mkdir_fn fn);

typedef int (*smbc_rmdir_fn)(SMBCCTX *c,
                             const char *fname);
smbc_rmdir_fn smbc_getFunctionRmdir(SMBCCTX *c);
void smbc_setFunctionRmdir(SMBCCTX *c, smbc_rmdir_fn fn);

typedef off_t (*smbc_telldir_fn)(SMBCCTX *c,
                                 SMBCFILE *dir);
smbc_telldir_fn smbc_getFunctionTelldir(SMBCCTX *c);
void smbc_setFunctionTelldir(SMBCCTX *c, smbc_telldir_fn fn);

typedef int (*smbc_lseekdir_fn)(SMBCCTX *c,
                                SMBCFILE *dir,
                                off_t offset);
smbc_lseekdir_fn smbc_getFunctionLseekdir(SMBCCTX *c);
void smbc_setFunctionLseekdir(SMBCCTX *c, smbc_lseekdir_fn fn);

typedef int (*smbc_fstatdir_fn)(SMBCCTX *c,
                                SMBCFILE *dir,
                                struct stat *st);
smbc_fstatdir_fn smbc_getFunctionFstatdir(SMBCCTX *c);
void smbc_setFunctionFstatdir(SMBCCTX *c, smbc_fstatdir_fn fn);



/*****************************************************************
 * Callable functions applicable to both files and directories.  *
 * Each callable has a function signature typedef, a declaration *
 * for the getter, and a declaration for the setter.             *
 *****************************************************************/

typedef int (*smbc_chmod_fn)(SMBCCTX *c,
                             const char *fname,
                             mode_t mode);
smbc_chmod_fn smbc_getFunctionChmod(SMBCCTX *c);
void smbc_setFunctionChmod(SMBCCTX *c, smbc_chmod_fn fn);

typedef int (*smbc_utimes_fn)(SMBCCTX *c,
                              const char *fname,
                              struct timeval *tbuf);
smbc_utimes_fn smbc_getFunctionUtimes(SMBCCTX *c);
void smbc_setFunctionUtimes(SMBCCTX *c, smbc_utimes_fn fn);

typedef int (*smbc_setxattr_fn)(SMBCCTX *context,
                                const char *fname,
                                const char *name,
                                const void *value,
                                size_t size,
                                int flags);
smbc_setxattr_fn smbc_getFunctionSetxattr(SMBCCTX *c);
void smbc_setFunctionSetxattr(SMBCCTX *c, smbc_setxattr_fn fn);

typedef int (*smbc_getxattr_fn)(SMBCCTX *context,
                                const char *fname,
                                const char *name,
                                const void *value,
                                size_t size);
smbc_getxattr_fn smbc_getFunctionGetxattr(SMBCCTX *c);
void smbc_setFunctionGetxattr(SMBCCTX *c, smbc_getxattr_fn fn);

typedef int (*smbc_removexattr_fn)(SMBCCTX *context,
                                   const char *fname,
                                   const char *name);
smbc_removexattr_fn smbc_getFunctionRemovexattr(SMBCCTX *c);
void smbc_setFunctionRemovexattr(SMBCCTX *c, smbc_removexattr_fn fn);

typedef int (*smbc_listxattr_fn)(SMBCCTX *context,
                                 const char *fname,
                                 char *list,
                                 size_t size);
smbc_listxattr_fn smbc_getFunctionListxattr(SMBCCTX *c);
void smbc_setFunctionListxattr(SMBCCTX *c, smbc_listxattr_fn fn);



/*****************************************************************
 * Callable functions for printing.                              *
 * Each callable has a function signature typedef, a declaration *
 * for the getter, and a declaration for the setter.             *
 *****************************************************************/

typedef int (*smbc_print_file_fn)(SMBCCTX *c_file,
                                  const char *fname,
                                  SMBCCTX *c_print,
                                  const char *printq);
smbc_print_file_fn smbc_getFunctionPrintFile(SMBCCTX *c);
void smbc_setFunctionPrintFile(SMBCCTX *c, smbc_print_file_fn fn);

typedef SMBCFILE * (*smbc_open_print_job_fn)(SMBCCTX *c,
                                             const char *fname);
smbc_open_print_job_fn smbc_getFunctionOpenPrintJob(SMBCCTX *c);
void smbc_setFunctionOpenPrintJob(SMBCCTX *c,
                                  smbc_open_print_job_fn fn);

typedef int (*smbc_list_print_jobs_fn)(SMBCCTX *c,
                                       const char *fname,
                                       smbc_list_print_job_fn fn);
smbc_list_print_jobs_fn smbc_getFunctionListPrintJobs(SMBCCTX *c);
void smbc_setFunctionListPrintJobs(SMBCCTX *c,
                                   smbc_list_print_jobs_fn fn);

typedef int (*smbc_unlink_print_job_fn)(SMBCCTX *c,
                                        const char *fname,
                                        int id);
smbc_unlink_print_job_fn smbc_getFunctionUnlinkPrintJob(SMBCCTX *c);
void smbc_setFunctionUnlinkPrintJob(SMBCCTX *c,
                                    smbc_unlink_print_job_fn fn);


/**@ingroup misc
 * Create a new SBMCCTX (a context).
 *
 * Must be called before the context is passed to smbc_context_init()
 *
 * @return          The given SMBCCTX pointer on success, NULL on error with errno set:
 *                  - ENOMEM Out of memory
 *
 * @see             smbc_free_context(), smbc_init_context()
 *
 * @note            Do not forget to smbc_init_context() the returned SMBCCTX pointer !
 */
SMBCCTX * smbc_new_context(void);

/**@ingroup misc
 * Delete a SBMCCTX (a context) acquired from smbc_new_context().
 *
 * The context will be deleted if possible.
 *
 * @param context   A pointer to a SMBCCTX obtained from smbc_new_context()
 *
 * @param shutdown_ctx   If 1, all connections and files will be closed even if they are busy.
 *
 *
 * @return          Returns 0 on succes. Returns 1 on failure with errno set:
 *                  - EBUSY Server connections are still used, Files are open or cache 
 *                          could not be purged
 *                  - EBADF context == NULL
 *
 * @see             smbc_new_context()
 *
 * @note            It is advised to clean up all the contexts with shutdown_ctx set to 1
 *                  just before exit()'ing. When shutdown_ctx is 0, this function can be
 *                  use in periodical cleanup functions for example.
 */
int smbc_free_context(SMBCCTX * context, int shutdown_ctx);


/**@ingroup misc
 *
 * @deprecated.  Use smbc_setOption*() functions instead.
 */
void
smbc_option_set(SMBCCTX *context,
                char *option_name,
                ... /* option_value */);

/*
 * @deprecated.  Use smbc_getOption*() functions instead.
 */
void *
smbc_option_get(SMBCCTX *context,
                char *option_name);

/**@ingroup misc
 * Initialize a SBMCCTX (a context).
 *
 * Must be called before using any SMBCCTX API function
 *
 * @param context   A pointer to a SMBCCTX obtained from smbc_new_context()
 *
 * @return          A pointer to the given SMBCCTX on success,
 *                  NULL on error with errno set:
 *                  - EBADF  NULL context given
 *                  - ENOMEM Out of memory
 *                  - ENOENT The smb.conf file would not load
 *
 * @see             smbc_new_context()
 *
 * @note            my_context = smbc_init_context(smbc_new_context())
 *                  is perfectly safe, but it might leak memory on
 *                  smbc_context_init() failure. Avoid this.
 *                  You'll have to call smbc_free_context() yourself
 *                  on failure.  
 */

SMBCCTX * smbc_init_context(SMBCCTX * context);

/**@ingroup misc
 * Initialize the samba client library.
 *
 * Must be called before using any of the smbclient API function
 *  
 * @param fn        The function that will be called to obtaion 
 *                  authentication credentials.
 *
 * @param debug     Allows caller to set the debug level. Can be
 *                  changed in smb.conf file. Allows caller to set
 *                  debugging if no smb.conf.
 *   
 * @return          0 on success, < 0 on error with errno set:
 *                  - ENOMEM Out of memory
 *                  - ENOENT The smb.conf file would not load
 *
 */

int smbc_init(smbc_get_auth_data_fn fn, int debug);

/**@ingroup misc
 * Set or retrieve the compatibility library's context pointer
 *
 * @param context   New context to use, or NULL.  If a new context is provided,
 *                  it must have allocated with smbc_new_context() and
 *                  initialized with smbc_init_context(), followed, optionally,
 *                  by some manual changes to some of the non-internal fields.
 *
 * @return          The old context.
 *
 * @see             smbc_new_context(), smbc_init_context(), smbc_init()
 *
 * @note            This function may be called prior to smbc_init() to force
 *                  use of the next context without any internal calls to
 *                  smbc_new_context() or smbc_init_context().  It may also
 *                  be called after smbc_init() has already called those two
 *                  functions, to replace the existing context with a new one.
 *                  Care should be taken, in this latter case, to ensure that
 *                  the server cache and any data allocated by the
 *                  authentication functions have been freed, if necessary.
 */

SMBCCTX * smbc_set_context(SMBCCTX * new_context);

/**@ingroup file
 * Open a file on an SMB server.
 *
 * @param furl      The smb url of the file to be opened. 
 *
 * @param flags     Is one of O_RDONLY, O_WRONLY or O_RDWR which 
 *                  request opening  the  file  read-only,write-only
 *                  or read/write. flags may also be bitwise-or'd with
 *                  one or  more of  the following: 
 *                  O_CREAT - If the file does not exist it will be 
 *                  created.
 *                  O_EXCL - When  used with O_CREAT, if the file 
 *                  already exists it is an error and the open will 
 *                  fail. 
 *                  O_TRUNC - If the file already exists it will be
 *                  truncated.
 *                  O_APPEND The  file  is  opened  in  append mode 
 *
 * @param mode      mode specifies the permissions to use if a new 
 *                  file is created.  It  is  modified  by  the 
 *                  process's umask in the usual way: the permissions
 *                  of the created file are (mode & ~umask) 
 *
 *                  Not currently use, but there for future use.
 *                  We will map this to SYSTEM, HIDDEN, etc bits
 *                  that reverses the mapping that smbc_fstat does.
 *
 * @return          Valid file handle, < 0 on error with errno set:
 *                  - ENOMEM  Out of memory
 *                  - EINVAL if an invalid parameter passed, like no 
 *                  file, or smbc_init not called.
 *                  - EEXIST  pathname already exists and O_CREAT and 
 *                  O_EXCL were used.
 *                  - EISDIR  pathname  refers  to  a  directory  and  
 *                  the access requested involved writing.
 *                  - EACCES  The requested access to the file is not 
 *                  allowed 
 *                  - ENODEV The requested share does not exist
 *                  - ENOTDIR A file on the path is not a directory
 *                  - ENOENT  A directory component in pathname does 
 *                  not exist.
 *
 * @see             smbc_creat()
 *
 * @note            This call uses an underlying routine that may create
 *                  a new connection to the server specified in the URL.
 *                  If the credentials supplied in the URL, or via the
 *                  auth_fn in the smbc_init call, fail, this call will
 *                  try again with an empty username and password. This 
 *                  often gets mapped to the guest account on some machines.
 */

int smbc_open(const char *furl, int flags, mode_t mode);

/**@ingroup file
 * Create a file on an SMB server.
 *
 * Same as calling smbc_open() with flags = O_CREAT|O_WRONLY|O_TRUNC 
 *   
 * @param furl      The smb url of the file to be created
 *  
 * @param mode      mode specifies the permissions to use if  a  new  
 *                  file is created.  It  is  modified  by  the 
 *                  process's umask in the usual way: the permissions
 *                  of the created file are (mode & ~umask)
 *
 *                  NOTE, the above is not true. We are dealing with 
 *                  an SMB server, which has no concept of a umask!
 *      
 * @return          Valid file handle, < 0 on error with errno set:
 *                  - ENOMEM  Out of memory
 *                  - EINVAL if an invalid parameter passed, like no 
 *                  file, or smbc_init not called.
 *                  - EEXIST  pathname already exists and O_CREAT and
 *                  O_EXCL were used.
 *                  - EISDIR  pathname  refers  to  a  directory  and
 *                  the access requested involved writing.
 *                  - EACCES  The requested access to the file is not
 *                  allowed 
 *                  - ENOENT  A directory component in pathname does 
 *                  not exist.
 *                  - ENODEV The requested share does not exist.
 * @see             smbc_open()
 *
 */

int smbc_creat(const char *furl, mode_t mode);

/**@ingroup file
 * Read from a file using an opened file handle.
 *
 * @param fd        Open file handle from smbc_open() or smbc_creat()
 *
 * @param buf       Pointer to buffer to receive read data
 *
 * @param bufsize   Size of buf in bytes
 *
 * @return          Number of bytes read;
 *                  0 upon EOF;
 *                  < 0 on error, with errno set:
 *                  - EISDIR fd refers to a directory
 *                  - EBADF  fd  is  not  a valid file descriptor or 
 *                    is not open for reading.
 *                  - EINVAL fd is attached to an object which is 
 *                    unsuitable for reading, or no buffer passed or
 *		      smbc_init not called.
 *
 * @see             smbc_open(), smbc_write()
 *
 */
ssize_t smbc_read(int fd, void *buf, size_t bufsize);


/**@ingroup file
 * Write to a file using an opened file handle.
 *
 * @param fd        Open file handle from smbc_open() or smbc_creat()
 *
 * @param buf       Pointer to buffer to recieve read data
 *
 * @param bufsize   Size of buf in bytes
 *
 * @return          Number of bytes written, < 0 on error with errno set:
 *                  - EISDIR fd refers to a directory.
 *                  - EBADF  fd  is  not  a valid file descriptor or 
 *                  is not open for reading.
 *                  - EINVAL fd is attached to an object which is 
 *                  unsuitable for reading, or no buffer passed or
 *		    smbc_init not called.
 *
 * @see             smbc_open(), smbc_read()
 *
 */
ssize_t smbc_write(int fd, const void *buf, size_t bufsize);


/**@ingroup file
 * Seek to a specific location in a file.
 *
 * @param fd        Open file handle from smbc_open() or smbc_creat()
 * 
 * @param offset    Offset in bytes from whence
 * 
 * @param whence    A location in the file:
 *                  - SEEK_SET The offset is set to offset bytes from
 *                  the beginning of the file
 *                  - SEEK_CUR The offset is set to current location 
 *                  plus offset bytes.
 *                  - SEEK_END The offset is set to the size of the 
 *                  file plus offset bytes.
 *
 * @return          Upon successful completion, lseek returns the 
 *                  resulting offset location as measured in bytes 
 *                  from the beginning  of the file. Otherwise, a value
 *                  of (off_t)-1 is returned and errno is set to 
 *                  indicate the error:
 *                  - EBADF  Fildes is not an open file descriptor.
 *                  - EINVAL Whence is not a proper value or smbc_init
 *		      not called.
 *
 * @todo Are all the whence values really supported?
 * 
 * @todo Are errno values complete and correct?
 */
off_t smbc_lseek(int fd, off_t offset, int whence);


/**@ingroup file
 * Close an open file handle.
 *
 * @param fd        The file handle to close
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EBADF  fd isn't a valid open file descriptor
 *                  - EINVAL smbc_init() failed or has not been called
 *
 * @see             smbc_open(), smbc_creat()
 */
int smbc_close(int fd);


/**@ingroup directory
 * Unlink (delete) a file or directory.
 *
 * @param furl      The smb url of the file to delete
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EACCES or EPERM Write  access  to the directory 
 *                  containing pathname is not allowed or one  
 *                  of  the  directories in pathname did not allow
 *                  search (execute) permission
 *                  - ENOENT A directory component in pathname does
 *                  not exist
 *                  - EINVAL NULL was passed in the file param or
 *		      smbc_init not called.
 *                  - EACCES You do not have access to the file
 *                  - ENOMEM Insufficient kernel memory was available
 *
 * @see             smbc_rmdir()s
 *
 * @todo Are errno values complete and correct?
 */
int smbc_unlink(const char *furl);


/**@ingroup directory
 * Rename or move a file or directory.
 * 
 * @param ourl      The original smb url (source url) of file or 
 *                  directory to be moved
 * 
 * @param nurl      The new smb url (destination url) of the file
 *                  or directory after the move.  Currently nurl must
 *                  be on the same share as ourl.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EISDIR nurl is an existing directory, but ourl is
 *                  not a directory.
 *                  - EEXIST nurl is  a  non-empty directory, 
 *                  i.e., contains entries other than "." and ".."
 *                  - EINVAL The  new  url  contained  a path prefix 
 *                  of the old, or, more generally, an  attempt was
 *                  made  to make a directory a subdirectory of itself
 *		    or smbc_init not called.
 *                  - ENOTDIR A component used as a directory in ourl 
 *                  or nurl path is not, in fact, a directory.  Or, 
 *                  ourl  is a directory, and newpath exists but is not
 *                  a directory.
 *                  - EACCES or EPERM Write access to the directory 
 *                  containing ourl or nurl is not allowed for the 
 *                  process's effective uid,  or  one of the 
 *                  directories in ourl or nurl did not allow search
 *                  (execute) permission,  or ourl  was  a  directory
 *                  and did not allow write permission.
 *                  - ENOENT A  directory component in ourl or nurl 
 *                  does not exist.
 *                  - EXDEV Rename across shares not supported.
 *                  - ENOMEM Insufficient kernel memory was available.
 *                  - EEXIST The target file, nurl, already exists.
 *
 *
 * @todo Are we going to support copying when urls are not on the same
 *       share?  I say no... NOTE. I agree for the moment.
 *
 */
int smbc_rename(const char *ourl, const char *nurl);


/**@ingroup directory
 * Open a directory used to obtain directory entries.
 *
 * @param durl      The smb url of the directory to open
 *
 * @return          Valid directory handle. < 0 on error with errno set:
 *                  - EACCES Permission denied.
 *                  - EINVAL A NULL file/URL was passed, or the URL would
 *                  not parse, or was of incorrect form or smbc_init not
 *                  called.
 *                  - ENOENT durl does not exist, or name is an 
 *                  - ENOMEM Insufficient memory to complete the 
 *                  operation.                              
 *                  - ENOTDIR name is not a directory.
 *                  - EPERM the workgroup could not be found.
 *                  - ENODEV the workgroup or server could not be found.
 *
 * @see             smbc_getdents(), smbc_readdir(), smbc_closedir()
 *
 */
int smbc_opendir(const char *durl);


/**@ingroup directory
 * Close a directory handle opened by smbc_opendir().
 *
 * @param dh        Directory handle to close
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EBADF dh is an invalid directory handle
 *
 * @see             smbc_opendir()
 */
int smbc_closedir(int dh);


/**@ingroup directory
 * Get multiple directory entries.
 *
 * smbc_getdents() reads as many dirent structures from the an open 
 * directory handle into a specified memory area as will fit.
 *
 * @param dh        Valid directory as returned by smbc_opendir()
 *
 * @param dirp      pointer to buffer that will receive the directory
 *                  entries.
 * 
 * @param count     The size of the dirp buffer in bytes
 *
 * @returns         If any dirents returned, return will indicate the
 *                  total size. If there were no more dirents available,
 *                  0 is returned. < 0 indicates an error.
 *                  - EBADF  Invalid directory handle
 *                  - EINVAL Result buffer is too small or smbc_init
 *		    not called.
 *                  - ENOENT No such directory.
 * @see             , smbc_dirent, smbc_readdir(), smbc_open()
 *
 * @todo Are errno values complete and correct?
 *
 * @todo Add example code so people know how to parse buffers.
 */
int smbc_getdents(unsigned int dh, struct smbc_dirent *dirp, int count);


/**@ingroup directory
 * Get a single directory entry.
 *
 * @param dh        Valid directory as returned by smbc_opendir()
 *
 * @return          A pointer to a smbc_dirent structure, or NULL if an
 *                  error occurs or end-of-directory is reached:
 *                  - EBADF Invalid directory handle
 *                  - EINVAL smbc_init() failed or has not been called
 *
 * @see             smbc_dirent, smbc_getdents(), smbc_open()
 */
struct smbc_dirent* smbc_readdir(unsigned int dh);


/**@ingroup directory
 * Get the current directory offset.
 *
 * smbc_telldir() may be used in conjunction with smbc_readdir() and
 * smbc_lseekdir().
 *
 * @param dh        Valid directory as returned by smbc_opendir()
 *
 * @return          The current location in the directory stream or -1
 *                  if an error occur.  The current location is not
 *                  an offset. Becuase of the implementation, it is a 
 *                  handle that allows the library to find the entry
 *                  later.
 *                  - EBADF dh is not a valid directory handle
 *                  - EINVAL smbc_init() failed or has not been called
 *                  - ENOTDIR if dh is not a directory
 *
 * @see             smbc_readdir()
 *
 */
off_t smbc_telldir(int dh);


/**@ingroup directory
 * lseek on directories.
 *
 * smbc_lseekdir() may be used in conjunction with smbc_readdir() and
 * smbc_telldir(). (rewind by smbc_lseekdir(fd, NULL))
 *
 * @param fd        Valid directory as returned by smbc_opendir()
 * 
 * @param offset    The offset (as returned by smbc_telldir). Can be
 *                  NULL, in which case we will rewind
 *
 * @return          0 on success, -1 on failure
 *                  - EBADF dh is not a valid directory handle
 *                  - ENOTDIR if dh is not a directory
 *                  - EINVAL offset did not refer to a valid dirent or
 *		      smbc_init not called.
 *
 * @see             smbc_telldir()
 *
 *
 * @todo In what does the reture and errno values mean?
 */
int smbc_lseekdir(int fd, off_t offset);

/**@ingroup directory
 * Create a directory.
 *
 * @param durl      The url of the directory to create
 *
 * @param mode      Specifies  the  permissions to use. It is modified
 *                  by the process's umask in the usual way: the 
 *                  permissions of the created file are (mode & ~umask).
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EEXIST directory url already exists
 *                  - EACCES The parent directory does not allow write
 *                  permission to the process, or one of the directories
 *                  - ENOENT A directory component in pathname does not
 *                  exist.
 *                  - EINVAL NULL durl passed or smbc_init not called.
 *                  - ENOMEM Insufficient memory was available.
 *
 * @see             smbc_rmdir()
 *
 */
int smbc_mkdir(const char *durl, mode_t mode);


/**@ingroup directory
 * Remove a directory.
 * 
 * @param durl      The smb url of the directory to remove
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EACCES or EPERM Write access to the directory
 *                  containing pathname was not allowed.
 *                  - EINVAL durl is NULL or smbc_init not called.
 *                  - ENOENT A directory component in pathname does not
 *                  exist.
 *                  - ENOTEMPTY directory contains entries.
 *                  - ENOMEM Insufficient kernel memory was available.
 *
 * @see             smbc_mkdir(), smbc_unlink() 
 *
 * @todo Are errno values complete and correct?
 */
int smbc_rmdir(const char *durl);


/**@ingroup attribute
 * Get information about a file or directory.
 *
 * @param url       The smb url to get information for
 *
 * @param st        pointer to a buffer that will be filled with 
 *                  standard Unix struct stat information.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - ENOENT A component of the path file_name does not
 *                  exist.
 *                  - EINVAL a NULL url was passed or smbc_init not called.
 *                  - EACCES Permission denied.
 *                  - ENOMEM Out of memory
 *                  - ENOTDIR The target dir, url, is not a directory.
 *
 * @see             Unix stat()
 *
 */
int smbc_stat(const char *url, struct stat *st);


/**@ingroup attribute
 * Get file information via an file descriptor.
 * 
 * @param fd        Open file handle from smbc_open() or smbc_creat()
 *
 * @param st        pointer to a buffer that will be filled with 
 *                  standard Unix struct stat information.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EBADF  filedes is bad.
 *                  - EACCES Permission denied.
 *                  - EBADF fd is not a valid file descriptor
 *                  - EINVAL Problems occurred in the underlying routines
 *		      or smbc_init not called.
 *                  - ENOMEM Out of memory
 *
 * @see             smbc_stat(), Unix stat()
 *
 */
int smbc_fstat(int fd, struct stat *st);


/**@ingroup attribute
 * Get file system information for a specified path.
 * 
 * @param url       The smb url to get information for
 *
 * @param st        pointer to a buffer that will be filled with 
 *                  standard Unix struct statvfs information.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EBADF  filedes is bad.
 *                  - EACCES Permission denied.
 *                  - EBADF fd is not a valid file descriptor
 *                  - EINVAL Problems occurred in the underlying routines
 *		      or smbc_init not called.
 *                  - ENOMEM Out of memory
 *
 * @see             Unix fstatvfs()
 *
 */
int
smbc_statvfs(char *url,
             struct statvfs *st);

/**@ingroup attribute
 * Get file system information via an file descriptor.
 * 
 * @param fd        Open file handle from smbc_open(), smbc_creat(),
 *                  or smbc_opendir()
 *
 * @param st        pointer to a buffer that will be filled with 
 *                  standard Unix struct statvfs information.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EBADF  filedes is bad.
 *                  - EACCES Permission denied.
 *                  - EBADF fd is not a valid file descriptor
 *                  - EINVAL Problems occurred in the underlying routines
 *		      or smbc_init not called.
 *                  - ENOMEM Out of memory
 *
 * @see             Unix fstatvfs()
 *
 */
int
smbc_fstatvfs(int fd,
              struct statvfs *st);


/**@ingroup attribute
 * Truncate a file given a file descriptor
 * 
 * @param fd        Open file handle from smbc_open() or smbc_creat()
 *
 * @param size      size to truncate the file to
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EBADF  filedes is bad.
 *                  - EACCES Permission denied.
 *                  - EBADF fd is not a valid file descriptor
 *                  - EINVAL Problems occurred in the underlying routines
 *		      or smbc_init not called.
 *                  - ENOMEM Out of memory
 *
 * @see             , Unix ftruncate()
 *
 */
int smbc_ftruncate(int fd, off_t size);


/**@ingroup attribute
 * Change the permissions of a file.
 *
 * @param url       The smb url of the file or directory to change
 *                  permissions of
 * 
 * @param mode      The permissions to set:
 *                  - Put good explaination of permissions here!
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EPERM  The effective UID does not match the owner
 *                  of the file, and is not zero
 *                  - ENOENT The file does not exist.
 *                  - ENOMEM Insufficient was available.
 *                  - ENOENT file or directory does not exist
 *
 * @todo Actually implement this fuction?
 *
 * @todo Are errno values complete and correct?
 */
int smbc_chmod(const char *url, mode_t mode);

/**
 * @ingroup attribute
 * Change the last modification time on a file
 *
 * @param url       The smb url of the file or directory to change
 *                  the modification time of
 *
 * @param tbuf      An array of two timeval structures which contains,
 *                  respectively, the desired access and modification times.
 *                  NOTE: Only the tv_sec field off each timeval structure is
 *                  used.  The tv_usec (microseconds) portion is ignored.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - EPERM  Permission was denied.
 *
 */
int smbc_utimes(const char *url, struct timeval *tbuf);

#ifdef HAVE_UTIME_H
/**
 * @ingroup attribute
 * Change the last modification time on a file
 *
 * @param url       The smb url of the file or directory to change
 *                  the modification time of
 *
 * @param utbuf     A pointer to a utimebuf structure which contains the
 *                  desired access and modification times.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - ENOMEM No memory was available for internal needs
 *                  - EPERM  Permission was denied.
 *
 */
int smbc_utime(const char *fname, struct utimbuf *utbuf);
#endif

/**@ingroup attribute
 * Set extended attributes for a file.  This is used for modifying a file's
 * security descriptor (i.e. owner, group, and access control list)
 *
 * @param url       The smb url of the file or directory to set extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be changed.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter should contain a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @param value     The value to be assigned to the specified attribute name.
 *                  This buffer should contain only the attribute value if the
 *                  name was of the "system.nt_sec_desc.<attribute_name>"
 *                  form.  If the name was of the "system.nt_sec_desc.*" form
 *                  then a complete security descriptor, with name:value pairs
 *                  separated by tabs, commas, or newlines (not spaces!),
 *                  should be provided in this value buffer.  A complete
 *                  security descriptor will contain one or more entries
 *                  selected from the following:
 *
 *                    REVISION:<revision number>
 *                    OWNER:<sid or name>
 *                    GROUP:<sid or name>
 *                    ACL:<sid or name>:<type>/<flags>/<mask>
 *
 *                  The  revision of the ACL specifies the internal Windows NT
 *                  ACL revision for the security descriptor. If not specified
 *                  it defaults to  1.  Using values other than 1 may cause
 *                  strange behaviour.
 *
 *                  The owner and group specify the owner and group sids for
 *                  the object. If the attribute name (either '*+' with a
 *                  complete security descriptor, or individual 'owner+' or
 *                  'group+' attribute names) ended with a plus sign, the
 *                  specified name is resolved to a SID value, using the
 *                  server on which the file or directory resides.  Otherwise,
 *                  the value should be provided in SID-printable format as
 *                  S-1-x-y-z, and is used directly.  The <sid or name>
 *                  associated with the ACL: attribute should be provided
 *                  similarly.
 *
 * @param size      The number of the bytes of data in the value buffer
 *
 * @param flags     A bit-wise OR of zero or more of the following:
 *                    SMBC_XATTR_FLAG_CREATE -
 *                      fail if the named attribute already exists
 *                    SMBC_XATTR_FLAG_REPLACE -
 *                      fail if the attribute does not already exist
 *
 *                  If neither flag is specified, the specified attributes
 *                  will be added or replace existing attributes of the same
 *                  name, as necessary.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL  The client library is not properly initialized
 *                            or one of the parameters is not of a correct
 *                            form
 *                  - ENOMEM No memory was available for internal needs
 *                  - EEXIST  If the attribute already exists and the flag
 *                            SMBC_XATTR_FLAG_CREAT was specified
 *                  - ENOATTR If the attribute does not exist and the flag
 *                            SMBC_XATTR_FLAG_REPLACE was specified
 *                  - EPERM   Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 * @note            Attribute names are compared in a case-insensitive
 *                  fashion.  All of the following are equivalent, although
 *                  the all-lower-case name is the preferred format:
 *                    system.nt_sec_desc.owner
 *                    SYSTEM.NT_SEC_DESC.OWNER
 *                    sYsTeM.nt_sEc_desc.owNER
 *
 */
int smbc_setxattr(const char *url,
                  const char *name,
                  const void *value,
                  size_t size,
                  int flags);


/**@ingroup attribute
 * Set extended attributes for a file.  This is used for modifying a file's
 * security descriptor (i.e. owner, group, and access control list).  The
 * POSIX function which this maps to would act on a symbolic link rather than
 * acting on what the symbolic link points to, but with no symbolic links in
 * SMB file systems, this function is functionally identical to
 * smbc_setxattr().
 *
 * @param url       The smb url of the file or directory to set extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be changed.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter should contain a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @param value     The value to be assigned to the specified attribute name.
 *                  This buffer should contain only the attribute value if the
 *                  name was of the "system.nt_sec_desc.<attribute_name>"
 *                  form.  If the name was of the "system.nt_sec_desc.*" form
 *                  then a complete security descriptor, with name:value pairs
 *                  separated by tabs, commas, or newlines (not spaces!),
 *                  should be provided in this value buffer.  A complete
 *                  security descriptor will contain one or more entries
 *                  selected from the following:
 *
 *                    REVISION:<revision number>
 *                    OWNER:<sid or name>
 *                    GROUP:<sid or name>
 *                    ACL:<sid or name>:<type>/<flags>/<mask>
 *
 *                  The  revision of the ACL specifies the internal Windows NT
 *                  ACL revision for the security descriptor. If not specified
 *                  it defaults to  1.  Using values other than 1 may cause
 *                  strange behaviour.
 *
 *                  The owner and group specify the owner and group sids for
 *                  the object. If the attribute name (either '*+' with a
 *                  complete security descriptor, or individual 'owner+' or
 *                  'group+' attribute names) ended with a plus sign, the
 *                  specified name is resolved to a SID value, using the
 *                  server on which the file or directory resides.  Otherwise,
 *                  the value should be provided in SID-printable format as
 *                  S-1-x-y-z, and is used directly.  The <sid or name>
 *                  associated with the ACL: attribute should be provided
 *                  similarly.
 *
 * @param size      The number of the bytes of data in the value buffer
 *
 * @param flags     A bit-wise OR of zero or more of the following:
 *                    SMBC_XATTR_FLAG_CREATE -
 *                      fail if the named attribute already exists
 *                    SMBC_XATTR_FLAG_REPLACE -
 *                      fail if the attribute does not already exist
 *
 *                  If neither flag is specified, the specified attributes
 *                  will be added or replace existing attributes of the same
 *                  name, as necessary.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL  The client library is not properly initialized
 *                            or one of the parameters is not of a correct
 *                            form
 *                  - ENOMEM No memory was available for internal needs
 *                  - EEXIST  If the attribute already exists and the flag
 *                            SMBC_XATTR_FLAG_CREAT was specified
 *                  - ENOATTR If the attribute does not exist and the flag
 *                            SMBC_XATTR_FLAG_REPLACE was specified
 *                  - EPERM   Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 * @note            Attribute names are compared in a case-insensitive
 *                  fashion.  All of the following are equivalent, although
 *                  the all-lower-case name is the preferred format:
 *                    system.nt_sec_desc.owner
 *                    SYSTEM.NT_SEC_DESC.OWNER
 *                    sYsTeM.nt_sEc_desc.owNER
 *
 */
int smbc_lsetxattr(const char *url,
                   const char *name,
                   const void *value,
                   size_t size,
                   int flags);


/**@ingroup attribute
 * Set extended attributes for a file.  This is used for modifying a file's
 * security descriptor (i.e. owner, group, and access control list)
 *
 * @param fd        A file descriptor associated with an open file (as
 *                  previously returned by smbc_open(), to get extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be changed.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter should contain a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @param value     The value to be assigned to the specified attribute name.
 *                  This buffer should contain only the attribute value if the
 *                  name was of the "system.nt_sec_desc.<attribute_name>"
 *                  form.  If the name was of the "system.nt_sec_desc.*" form
 *                  then a complete security descriptor, with name:value pairs
 *                  separated by tabs, commas, or newlines (not spaces!),
 *                  should be provided in this value buffer.  A complete
 *                  security descriptor will contain one or more entries
 *                  selected from the following:
 *
 *                    REVISION:<revision number>
 *                    OWNER:<sid or name>
 *                    GROUP:<sid or name>
 *                    ACL:<sid or name>:<type>/<flags>/<mask>
 *
 *                  The  revision of the ACL specifies the internal Windows NT
 *                  ACL revision for the security descriptor. If not specified
 *                  it defaults to  1.  Using values other than 1 may cause
 *                  strange behaviour.
 *
 *                  The owner and group specify the owner and group sids for
 *                  the object. If the attribute name (either '*+' with a
 *                  complete security descriptor, or individual 'owner+' or
 *                  'group+' attribute names) ended with a plus sign, the
 *                  specified name is resolved to a SID value, using the
 *                  server on which the file or directory resides.  Otherwise,
 *                  the value should be provided in SID-printable format as
 *                  S-1-x-y-z, and is used directly.  The <sid or name>
 *                  associated with the ACL: attribute should be provided
 *                  similarly.
 *
 * @param size      The number of the bytes of data in the value buffer
 *
 * @param flags     A bit-wise OR of zero or more of the following:
 *                    SMBC_XATTR_FLAG_CREATE -
 *                      fail if the named attribute already exists
 *                    SMBC_XATTR_FLAG_REPLACE -
 *                      fail if the attribute does not already exist
 *
 *                  If neither flag is specified, the specified attributes
 *                  will be added or replace existing attributes of the same
 *                  name, as necessary.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL  The client library is not properly initialized
 *                            or one of the parameters is not of a correct
 *                            form
 *                  - ENOMEM No memory was available for internal needs
 *                  - EEXIST  If the attribute already exists and the flag
 *                            SMBC_XATTR_FLAG_CREAT was specified
 *                  - ENOATTR If the attribute does not exist and the flag
 *                            SMBC_XATTR_FLAG_REPLACE was specified
 *                  - EPERM   Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 * @note            Attribute names are compared in a case-insensitive
 *                  fashion.  All of the following are equivalent, although
 *                  the all-lower-case name is the preferred format:
 *                    system.nt_sec_desc.owner
 *                    SYSTEM.NT_SEC_DESC.OWNER
 *                    sYsTeM.nt_sEc_desc.owNER
 *
 */
int smbc_fsetxattr(int fd,
                   const char *name,
                   const void *value,
                   size_t size,
                   int flags);


/**@ingroup attribute
 * Get extended attributes for a file.
 *
 * @param url       The smb url of the file or directory to get extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be retrieved.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter will return a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @param value     A pointer to a buffer in which the value of the specified
 *                  attribute will be placed (unless size is zero).
 *
 * @param size      The size of the buffer pointed to by value.  This parameter
 *                  may also be zero, in which case the size of the buffer
 *                  required to hold the attribute value will be returned,
 *                  but nothing will be placed into the value buffer.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL  The client library is not properly initialized
 *                            or one of the parameters is not of a correct
 *                            form
 *                  - ENOMEM No memory was available for internal needs
 *                  - EEXIST  If the attribute already exists and the flag
 *                            SMBC_XATTR_FLAG_CREAT was specified
 *                  - ENOATTR If the attribute does not exist and the flag
 *                            SMBC_XATTR_FLAG_REPLACE was specified
 *                  - EPERM   Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 */
int smbc_getxattr(const char *url,
                  const char *name,
                  const void *value,
                  size_t size);


/**@ingroup attribute
 * Get extended attributes for a file.  The POSIX function which this maps to
 * would act on a symbolic link rather than acting on what the symbolic link
 * points to, but with no symbolic links in SMB file systems, this function
 * is functionally identical to smbc_getxattr().
 *
 * @param url       The smb url of the file or directory to get extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be retrieved.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter will return a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @param value     A pointer to a buffer in which the value of the specified
 *                  attribute will be placed (unless size is zero).
 *
 * @param size      The size of the buffer pointed to by value.  This parameter
 *                  may also be zero, in which case the size of the buffer
 *                  required to hold the attribute value will be returned,
 *                  but nothing will be placed into the value buffer.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL  The client library is not properly initialized
 *                            or one of the parameters is not of a correct
 *                            form
 *                  - ENOMEM No memory was available for internal needs
 *                  - EEXIST  If the attribute already exists and the flag
 *                            SMBC_XATTR_FLAG_CREAT was specified
 *                  - ENOATTR If the attribute does not exist and the flag
 *                            SMBC_XATTR_FLAG_REPLACE was specified
 *                  - EPERM   Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 */
int smbc_lgetxattr(const char *url,
                   const char *name,
                   const void *value,
                   size_t size);


/**@ingroup attribute
 * Get extended attributes for a file.
 *
 * @param fd        A file descriptor associated with an open file (as
 *                  previously returned by smbc_open(), to get extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be retrieved.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter will return a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @param value     A pointer to a buffer in which the value of the specified
 *                  attribute will be placed (unless size is zero).
 *
 * @param size      The size of the buffer pointed to by value.  This parameter
 *                  may also be zero, in which case the size of the buffer
 *                  required to hold the attribute value will be returned,
 *                  but nothing will be placed into the value buffer.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL  The client library is not properly initialized
 *                            or one of the parameters is not of a correct
 *                            form
 *                  - ENOMEM No memory was available for internal needs
 *                  - EEXIST  If the attribute already exists and the flag
 *                            SMBC_XATTR_FLAG_CREAT was specified
 *                  - ENOATTR If the attribute does not exist and the flag
 *                            SMBC_XATTR_FLAG_REPLACE was specified
 *                  - EPERM   Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 */
int smbc_fgetxattr(int fd,
                   const char *name,
                   const void *value,
                   size_t size);


/**@ingroup attribute
 * Remove extended attributes for a file.  This is used for modifying a file's
 * security descriptor (i.e. owner, group, and access control list)
 *
 * @param url       The smb url of the file or directory to remove the extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be removed.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter will return a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - ENOMEM No memory was available for internal needs
 *                  - EPERM  Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 */
int smbc_removexattr(const char *url,
                     const char *name);


/**@ingroup attribute
 * Remove extended attributes for a file.  This is used for modifying a file's
 * security descriptor (i.e. owner, group, and access control list) The POSIX
 * function which this maps to would act on a symbolic link rather than acting
 * on what the symbolic link points to, but with no symbolic links in SMB file
 * systems, this function is functionally identical to smbc_removexattr().
 *
 * @param url       The smb url of the file or directory to remove the extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be removed.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter will return a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - ENOMEM No memory was available for internal needs
 *                  - EPERM  Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 */
int smbc_lremovexattr(const char *url,
                      const char *name);


/**@ingroup attribute
 * Remove extended attributes for a file.  This is used for modifying a file's
 * security descriptor (i.e. owner, group, and access control list)
 *
 * @param fd        A file descriptor associated with an open file (as
 *                  previously returned by smbc_open(), to get extended
 *                  attributes for.
 * 
 * @param name      The name of an attribute to be removed.  Names are of
 *                  one of the following forms:
 *
 *                     system.nt_sec_desc.<attribute name>
 *                     system.nt_sec_desc.*
 *                     system.nt_sec_desc.*+
 *
 *                  where <attribute name> is one of:
 *
 *                     revision
 *                     owner
 *                     owner+
 *                     group
 *                     group+
 *                     acl:<name or sid>
 *                     acl+:<name or sid>
 *
 *                  In the forms "system.nt_sec_desc.*" and
 *                  "system.nt_sec_desc.*+", the asterisk and plus signs are
 *                  literal, i.e. the string is provided exactly as shown, and
 *                  the value parameter will return a complete security
 *                  descriptor with name:value pairs separated by tabs,
 *                  commas, or newlines (not spaces!).
 *
 *                  The plus sign ('+') indicates that SIDs should be mapped
 *                  to names.  Without the plus sign, SIDs are not mapped;
 *                  rather they are simply converted to a string format.
 *
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - ENOMEM No memory was available for internal needs
 *                  - EPERM  Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 */
int smbc_fremovexattr(int fd,
                      const char *name);


/**@ingroup attribute
 * List the supported extended attribute names associated with a file
 *
 * @param url       The smb url of the file or directory to list the extended
 *                  attributes for.
 *
 * @param list      A pointer to a buffer in which the list of attributes for
 *                  the specified file or directory will be placed (unless
 *                  size is zero).
 *
 * @param size      The size of the buffer pointed to by list.  This parameter
 *                  may also be zero, in which case the size of the buffer
 *                  required to hold all of the attribute names will be
 *                  returned, but nothing will be placed into the list buffer.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - ENOMEM No memory was available for internal needs
 *                  - EPERM  Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 * @note            This function always returns all attribute names supported
 *                  by NT file systems, regardless of whether the referenced
 *                  file system supports extended attributes (e.g. a Windows
 *                  2000 machine supports extended attributes if NTFS is used,
 *                  but not if FAT is used, and Windows 98 doesn't support
 *                  extended attributes at all.  Whether this is a feature or
 *                  a bug is yet to be decided.
 */
int smbc_listxattr(const char *url,
                   char *list,
                   size_t size);

/**@ingroup attribute
 * List the supported extended attribute names associated with a file The
 * POSIX function which this maps to would act on a symbolic link rather than
 * acting on what the symbolic link points to, but with no symbolic links in
 * SMB file systems, this function is functionally identical to
 * smbc_listxattr().
 *
 * @param url       The smb url of the file or directory to list the extended
 *                  attributes for.
 *
 * @param list      A pointer to a buffer in which the list of attributes for
 *                  the specified file or directory will be placed (unless
 *                  size is zero).
 *
 * @param size      The size of the buffer pointed to by list.  This parameter
 *                  may also be zero, in which case the size of the buffer
 *                  required to hold all of the attribute names will be
 *                  returned, but nothing will be placed into the list buffer.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - ENOMEM No memory was available for internal needs
 *                  - EPERM  Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 * @note            This function always returns all attribute names supported
 *                  by NT file systems, regardless of wether the referenced
 *                  file system supports extended attributes (e.g. a Windows
 *                  2000 machine supports extended attributes if NTFS is used,
 *                  but not if FAT is used, and Windows 98 doesn't support
 *                  extended attributes at all.  Whether this is a feature or
 *                  a bug is yet to be decided.
 */
int smbc_llistxattr(const char *url,
                    char *list,
                    size_t size);

/**@ingroup attribute
 * List the supported extended attribute names associated with a file
 *
 * @param fd        A file descriptor associated with an open file (as
 *                  previously returned by smbc_open(), to get extended
 *                  attributes for.
 * 
 * @param list      A pointer to a buffer in which the list of attributes for
 *                  the specified file or directory will be placed (unless
 *                  size is zero).
 *
 * @param size      The size of the buffer pointed to by list.  This parameter
 *                  may also be zero, in which case the size of the buffer
 *                  required to hold all of the attribute names will be
 *                  returned, but nothing will be placed into the list buffer.
 * 
 * @return          0 on success, < 0 on error with errno set:
 *                  - EINVAL The client library is not properly initialized
 *                  - ENOMEM No memory was available for internal needs
 *                  - EPERM  Permission was denied.
 *                  - ENOTSUP The referenced file system does not support
 *                            extended attributes
 *
 * @note            This function always returns all attribute names supported
 *                  by NT file systems, regardless of wether the referenced
 *                  file system supports extended attributes (e.g. a Windows
 *                  2000 machine supports extended attributes if NTFS is used,
 *                  but not if FAT is used, and Windows 98 doesn't support
 *                  extended attributes at all.  Whether this is a feature or
 *                  a bug is yet to be decided.
 */
int smbc_flistxattr(int fd,
                    char *list,
                    size_t size);

/**@ingroup print
 * Print a file given the name in fname. It would be a URL ...
 * 
 * @param fname     The URL of a file on a remote SMB server that the
 *                  caller wants printed
 *
 * @param printq    The URL of the print share to print the file to.
 *
 * @return          0 on success, < 0 on error with errno set:         
 *
 *                  - EINVAL fname or printq was NULL or smbc_init not
 * 		      not called.
 *                  and errors returned by smbc_open
 *
 */                                     
int smbc_print_file(const char *fname, const char *printq);

/**@ingroup print
 * Open a print file that can be written to by other calls. This simply
 * does an smbc_open call after checking if there is a file name on the
 * URI. If not, a temporary name is added ...
 *
 * @param fname     The URL of the print share to print to?
 *
 * @returns         A file handle for the print file if successful.
 *                  Returns -1 if an error ocurred and errno has the values
 *                  - EINVAL fname was NULL or smbc_init not called.
 *                  - all errors returned by smbc_open
 *
 */
int smbc_open_print_job(const char *fname);

/**@ingroup print
 * List the print jobs on a print share, for the moment, pass a callback 
 *
 * @param purl      The url of the print share to list the jobs of
 * 
 * @param fn        Callback function the receives printjob info
 * 
 * @return          0 on success, < 0 on error with errno set: 
 *                  - EINVAL fname was NULL or smbc_init not called
 *                  - EACCES ???
 */
int smbc_list_print_jobs(const char *purl, smbc_list_print_job_fn fn);

/**@ingroup print
 * Delete a print job 
 *
 * @param purl      Url of the print share
 *
 * @param id        The id of the job to delete
 *
 * @return          0 on success, < 0 on error with errno set: 
 *                  - EINVAL fname was NULL or smbc_init not called
 *
 * @todo    what errno values are possible here?
 */
int smbc_unlink_print_job(const char *purl, int id);

/**@ingroup callback
 * Remove a server from the cached server list it's unused.
 *
 * @param context    pointer to smb context
 *
 * @param srv        pointer to server to remove
 *
 * @return On success, 0 is returned. 1 is returned if the server could not
 *         be removed. Also useable outside libsmbclient.
 */
int smbc_remove_unused_server(SMBCCTX * context, SMBCSRV * srv);

#ifdef __cplusplus
}
#endif

/**@ingroup directory
 * Convert strings of %xx to their single character equivalent.
 *
 * @param dest      A pointer to a buffer in which the resulting decoded
 *                  string should be placed.  This may be a pointer to the
 *                  same buffer as src_segment.
 * 
 * @param src       A pointer to the buffer containing the URL to be decoded.
 *                  Any %xx sequences herein are converted to their single
 *                  character equivalent.  Each 'x' must be a valid hexadecimal
 *                  digit, or that % sequence is left undecoded.
 *
 * @param max_dest_len
 *                  The size of the buffer pointed to by dest_segment.
 * 
 * @return          The number of % sequences which could not be converted
 *                  due to lack of two following hexadecimal digits.
 */
#ifdef __cplusplus
extern "C" {
#endif
int
smbc_urldecode(char *dest, char * src, size_t max_dest_len);
#ifdef __cplusplus
}
#endif


/*
 * Convert any characters not specifically allowed in a URL into their %xx
 * equivalent.
 *
 * @param dest      A pointer to a buffer in which the resulting encoded
 *                  string should be placed.  Unlike smbc_urldecode(), this
 *                  must be a buffer unique from src.
 * 
 * @param src       A pointer to the buffer containing the string to be encoded.
 *                  Any character not specifically allowed in a URL is converted
 *                  into its hexadecimal value and encoded as %xx.
 *
 * @param max_dest_len
 *                  The size of the buffer pointed to by dest_segment.
 * 
 * @returns         The remaining buffer length.
 */
#ifdef __cplusplus
extern "C" {
#endif
int
smbc_urlencode(char * dest, char * src, int max_dest_len);
#ifdef __cplusplus
}
#endif


/**@ingroup directory
 * Return the version of the linked Samba code, and thus the version of the
 * libsmbclient code.
 *
 * @return          The version string.
 */
#ifdef __cplusplus
extern "C" {
#endif
const char *
smbc_version(void);
#ifdef __cplusplus
}
#endif

/**@ingroup misc
 * Set the users credentials globally so they can be used for DFS
 * referrals. Probably best to use this function in the smbc_get_auth_data_fn
 * callback.
 *
 * @param workgroup      Workgroup of the user.
 *
 * @param user           Username of user.
 *
 * @param password       Password of user.
 *
 * @param use_kerberos   Whether to use Kerberos
 *
 * @param signing_state  One of these strings (all equivalents on same line):
 *                         "off", "no", "false"
 *                         "on", "yes", "true", "auto"
 *                         "force", "required", "forced"
 */

void
smbc_set_credentials(const char *workgroup,
                     const char *user,
                     const char *password,
                     smbc_bool use_kerberos,
                     const char *signing_state);

/*
 * Wrapper around smbc_set_credentials.
 * Used to set correct credentials that will
 * be used to connect to DFS target share 
 * in libsmbclient
 */

void
smbc_set_credentials_with_fallback(SMBCCTX *ctx,
		                   const char *workgroup,
			     	   const char *user,
			    	   const char *password);


/**
 * @ingroup threads
 *
 * Initialize for threads using the Posix Threads (pthread)
 * implementation. This is a built-in implementation, avoiding the need to
 * implement the component functions of the thread interface. If this function
 * is used, it is not necessary to call smbc_thread_impl().
 *
 * @return {void}
 */
void
smbc_thread_posix(void);

/**
 * @ingroup threads
 *
 * Initialize for an arbitrary thread implementation. The caller should
 * provide, as parameters, pointers to functions to implement the requisite
 * low-level thread functionality. A function must be provided for each
 * parameter; none may be null.
 *
 * If the thread implementation is POSIX Threads (pthreads), then the much
 * simpler smbc_thread_pthread() function may be used instead of this one.
 *
 * @param create_mutex
 *   Create a mutex. This function should expect three parameters: lockname,
 *   pplock, and location. It should create a unique mutex for each unique
 *   lockname it is provided, and return the mutex identifier in *pplock. The
 *   location parameter can be used for debugging, as it contains the
 *   compiler-provided __location__ of the call.
 *
 * @param destroy_mutex
 *   Destroy a mutex. This function should expect two parameters: plock and
 *   location. It should destroy the mutex associated with the identifier
 *   plock. The location parameter can be used for debugging, as it contains
 *   the compiler-provided __location__ of the call.
 *
 * @param lock_mutex
 *   Lock a mutex. This function should expect three parameters: plock,
 *   lock_type, and location. The mutex aassociated with identifier plock
 *   should be locked if lock_type is 1, and unlocked if lock_type is 2. The
 *   location parameter can be used for debugging, as it contains the
 *   compiler-provided __location__ of the call.
 *
 * @param create_tls
 *   Create thread local storage. This function should expect three
 *   parameters: keyname, ppkey, and location. It should allocate an
 *   implementation-specific amount of memory and assign the pointer to that
 *   allocated memory to *ppkey. The location parameter can be used for
 *   debugging, as it contains the compiler-provided __location__ of the
 *   call. This function should return 0 upon success, non-zero upon failure.
 *
 * @param destroy_tls
 *   Destroy thread local storage. This function should expect two parameters:
 *   ppkey and location. The ppkey parameter points to a variable containing a
 *   thread local storage key previously provided by the create_tls
 *   function. The location parameter can be used for debugging, as it
 *   contains the compiler-provided __location__ of the call.
 *
 * @param set_tls
 *   Set a thread local storage variable's value. This function should expect
 *   three parameters: pkey, pval, and location. The pkey parameter is a
 *   thread local storage key previously provided by the create_tls
 *   function. The (void *) pval parameter contains the value to be placed in
 *   the thread local storage variable identified by pkey. The location
 *   parameter can be used for debugging, as it contains the compiler-provided
 *   __location__ of the call. This function should return 0 upon success;
 *   non-zero otherwise.
 *
 * @param get_tls
 *   Retrieve a thread local storage variable's value. This function should
 *   expect two parameters: pkey and location. The pkey parameter is a thread
 *   local storage key previously provided by the create_tls function, and
 *   which has previously been used in a call to the set_tls function to
 *   initialize a thread local storage variable. The location parameter can be
 *   used for debugging, as it contains the compiler-provided __location__ of
 *   the call. This function should return the (void *) value stored in the
 *   variable identified by pkey.
 *
 * @return {void}
 */
void
smbc_thread_impl(
        /* Mutex functions. */
        int (*create_mutex)(const char *lockname,
                            void **pplock,
                            const char *location),
        void (*destroy_mutex)(void *plock,
                              const char *location),
        int (*lock_mutex)(void *plock,
                          int lock_type,
                          const char *location),
    
        /* Thread local storage. */
        int (*create_tls)(const char *keyname,
                          void **ppkey,
                          const char *location),
        void (*destroy_tls)(void **ppkey,
                            const char *location),
        int (*set_tls)(void *pkey,
                       const void *pval,
                       const char *location),
        void *(*get_tls)(void *pkey,
                         const char *location)
        );



/**
 * @ingroup structure
 * Structure that contains a client context information 
 * This structure is known as SMBCCTX
 *
 * DO NOT DIRECTLY MANIPULATE THE CONTEXT STRUCTURE!  The data in the context
 * structure should all be considered private to the library.  It remains here
 * only for backward compatibility.
 *
 * See the comments herein for use of the setter and getter functions which
 * should now be used for manipulating these values.  New features, functions,
 * etc., are not added here but rather in _internal where they are not
 * directly visible to applications.  This makes it much easier to maintain
 * ABI compatibility.
 */
struct _SMBCCTX
{
        /**
         * debug level
         *
         * DEPRECATED:
         * Use smbc_getDebug() and smbc_setDebug()
         */
        int     debug DEPRECATED_SMBC_INTERFACE;
	
        /**
         * netbios name used for making connections
         *
         * DEPRECATED:
         * Use smbc_getNetbiosName() and smbc_setNetbiosName()
         */
        char * netbios_name DEPRECATED_SMBC_INTERFACE;

        /**
         * workgroup name used for making connections
         *
         * DEPRECATED:
         * Use smbc_getWorkgroup() and smbc_setWorkgroup()
         */
        char * workgroup DEPRECATED_SMBC_INTERFACE;

        /**
         * username used for making connections
         *
         * DEPRECATED:
         * Use smbc_getUser() and smbc_setUser()
         */
        char * user DEPRECATED_SMBC_INTERFACE;

        /**
         * timeout used for waiting on connections / response data (in
         * milliseconds)
         *
         * DEPRECATED:
         * Use smbc_getTimeout() and smbc_setTimeout()
         */
        int timeout DEPRECATED_SMBC_INTERFACE;

	/**
         * callable functions for files:
	 * For usage and return values see the SMBC_* functions
         *
         * DEPRECATED:
         *
         * Use smbc_getFunction*() and smbc_setFunction*(), e.g.
         * smbc_getFunctionOpen(), smbc_setFunctionUnlink(), etc.
	 */ 
        smbc_open_fn                    open DEPRECATED_SMBC_INTERFACE;
        smbc_creat_fn                   creat DEPRECATED_SMBC_INTERFACE;
        smbc_read_fn                    read DEPRECATED_SMBC_INTERFACE;
        smbc_write_fn                   write DEPRECATED_SMBC_INTERFACE;
        smbc_unlink_fn                  unlink DEPRECATED_SMBC_INTERFACE;
        smbc_rename_fn                  rename DEPRECATED_SMBC_INTERFACE;
        smbc_lseek_fn                   lseek DEPRECATED_SMBC_INTERFACE;
        smbc_stat_fn                    stat DEPRECATED_SMBC_INTERFACE;
        smbc_fstat_fn                   fstat DEPRECATED_SMBC_INTERFACE;
#if 0 /* internal */
        smbc_ftruncate_fn               ftruncate_fn;
#endif
        smbc_close_fn                   close_fn DEPRECATED_SMBC_INTERFACE;
        smbc_opendir_fn                 opendir DEPRECATED_SMBC_INTERFACE;
        smbc_closedir_fn                closedir DEPRECATED_SMBC_INTERFACE;
        smbc_readdir_fn                 readdir DEPRECATED_SMBC_INTERFACE;
        smbc_getdents_fn                getdents DEPRECATED_SMBC_INTERFACE;
        smbc_mkdir_fn                   mkdir DEPRECATED_SMBC_INTERFACE;
        smbc_rmdir_fn                   rmdir DEPRECATED_SMBC_INTERFACE;
        smbc_telldir_fn                 telldir DEPRECATED_SMBC_INTERFACE;
        smbc_lseekdir_fn                lseekdir DEPRECATED_SMBC_INTERFACE;
        smbc_fstatdir_fn                fstatdir DEPRECATED_SMBC_INTERFACE;
        smbc_chmod_fn                   chmod DEPRECATED_SMBC_INTERFACE;
        smbc_utimes_fn                  utimes DEPRECATED_SMBC_INTERFACE;
        smbc_setxattr_fn                setxattr DEPRECATED_SMBC_INTERFACE;
        smbc_getxattr_fn                getxattr DEPRECATED_SMBC_INTERFACE;
        smbc_removexattr_fn             removexattr DEPRECATED_SMBC_INTERFACE;
        smbc_listxattr_fn               listxattr DEPRECATED_SMBC_INTERFACE;

        /* Printing-related functions */
        smbc_print_file_fn              print_file DEPRECATED_SMBC_INTERFACE;
        smbc_open_print_job_fn          open_print_job DEPRECATED_SMBC_INTERFACE;
        smbc_list_print_jobs_fn         list_print_jobs DEPRECATED_SMBC_INTERFACE;
        smbc_unlink_print_job_fn        unlink_print_job DEPRECATED_SMBC_INTERFACE;

        /*
        ** Callbacks
        *
        * DEPRECATED:
        *
        * See the comment above each field, for the getter and setter
        * functions that should now be used.
        */
	struct _smbc_callbacks
        {
		/**
                 * authentication function callback: called upon auth requests
                 *
                 * DEPRECATED:
                 * Use smbc_getFunctionAuthData(), smbc_setFunctionAuthData()
		 */
                smbc_get_auth_data_fn auth_fn DEPRECATED_SMBC_INTERFACE;
		
		/**
                 * check if a server is still good
                 *
                 * DEPRECATED:
                 * Use smbc_getFunctionCheckServer(),
                 * smbc_setFunctionCheckServer()
		 */
		smbc_check_server_fn check_server_fn DEPRECATED_SMBC_INTERFACE;

		/**
                 * remove a server if unused
                 *
                 * DEPRECATED:
                 * Use smbc_getFunctionRemoveUnusedServer(),
                 * smbc_setFunctionCheckServer()
		 */
		smbc_remove_unused_server_fn remove_unused_server_fn DEPRECATED_SMBC_INTERFACE;

		/** Cache subsystem
                 *
		 * For an example cache system see
		 * samba/source/libsmb/libsmb_cache.c
                 *
                 * Cache subsystem * functions follow.
		 */

		/**
                 * server cache addition 
                 *
                 * DEPRECATED:
                 * Use smbc_getFunctionAddCachedServer(),
                 * smbc_setFunctionAddCachedServer()
		 */
		smbc_add_cached_srv_fn add_cached_srv_fn DEPRECATED_SMBC_INTERFACE;

		/**
                 * server cache lookup 
                 *
                 * DEPRECATED:
                 * Use smbc_getFunctionGetCachedServer(),
                 * smbc_setFunctionGetCachedServer()
		 */
		smbc_get_cached_srv_fn get_cached_srv_fn DEPRECATED_SMBC_INTERFACE;

		/**
                 * server cache removal
                 *
                 * DEPRECATED:
                 * Use smbc_getFunctionRemoveCachedServer(),
                 * smbc_setFunctionRemoveCachedServer()
		 */
		smbc_remove_cached_srv_fn remove_cached_srv_fn DEPRECATED_SMBC_INTERFACE;
		
		/**
                 * server cache purging, try to remove all cached servers
                 * (disconnect)
                 *
                 * DEPRECATED:
                 * Use smbc_getFunctionPurgeCachedServers(),
                 * smbc_setFunctionPurgeCachedServers()
		 */
		smbc_purge_cached_fn purge_cached_fn DEPRECATED_SMBC_INTERFACE;
	} callbacks;

        /**
         * Space where the private data of the server cache used to be
         *
         * DEPRECATED:
         * Use smbc_getServerCacheData(), smbc_setServerCacheData()
         */
        void * reserved DEPRECATED_SMBC_INTERFACE;

        /*
         * Very old configuration options.
         * 
         * DEPRECATED:
         * Use one of the following functions instead:
         *   smbc_setOptionUseKerberos()
         *   smbc_getOptionUseKerberos()
         *   smbc_setOptionFallbackAfterKerberos()
         *   smbc_getOptionFallbackAfterKerberos()
         *   smbc_setOptionNoAutoAnonymousLogin()
         *   smbc_getOptionNoAutoAnonymousLogin()
         */
        int flags DEPRECATED_SMBC_INTERFACE;
	
        /**
         * user options selections that apply to this session
         *
         * NEW OPTIONS ARE NOT ADDED HERE!
         *
         * DEPRECATED:
         * To set and retrieve options, use the smbc_setOption*() and
         * smbc_getOption*() functions.
         */
        struct _smbc_options {
                int browse_max_lmb_count DEPRECATED_SMBC_INTERFACE;
                int urlencode_readdir_entries DEPRECATED_SMBC_INTERFACE;
                int one_share_per_server DEPRECATED_SMBC_INTERFACE;
        } options DEPRECATED_SMBC_INTERFACE;
	
	/** INTERNAL DATA
	 * do _NOT_ touch this from your program !
	 */
	struct SMBC_internal_data * internal;
};


#endif /* SMBCLIENT_H_INCLUDED */
