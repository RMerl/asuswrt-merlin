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
#define __LIBSMBCLIENT_INTERNAL__
#include "libsmbclient.h"
#include "libsmb_internal.h"


/** Get the netbios name used for making connections */
char *
smbc_getNetbiosName(SMBCCTX *c)
{
        return c->netbios_name;
}

/** Set the netbios name used for making connections */
void
smbc_setNetbiosName(SMBCCTX *c, char * netbios_name)
{
	SAFE_FREE(c->netbios_name);
	if (netbios_name) {
		c->netbios_name = SMB_STRDUP(netbios_name);
	}
}

/** Get the workgroup used for making connections */
char *
smbc_getWorkgroup(SMBCCTX *c)
{
        return c->workgroup;
}

/** Set the workgroup used for making connections */
void
smbc_setWorkgroup(SMBCCTX *c, char * workgroup)
{
	SAFE_FREE(c->workgroup);
	if (workgroup) {
		c->workgroup = SMB_STRDUP(workgroup);
	}
}

/** Get the username used for making connections */
char *
smbc_getUser(SMBCCTX *c)
{
        return c->user;
}

/** Set the username used for making connections */
void
smbc_setUser(SMBCCTX *c, char * user)
{
	SAFE_FREE(c->user);
	if (user) {
		c->user = SMB_STRDUP(user);
	}
}

/** Get the debug level */
int
smbc_getDebug(SMBCCTX *c)
{
        return c->debug;
}

/** Set the debug level */
void
smbc_setDebug(SMBCCTX *c, int debug)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", debug);
        c->debug = debug;
	lp_set_cmdline("log level", buf); 
}

/**
 * Get the timeout used for waiting on connections and response data
 * (in milliseconds)
 */
int
smbc_getTimeout(SMBCCTX *c)
{
        return c->timeout;
}

/**
 * Set the timeout used for waiting on connections and response data
 * (in milliseconds)
 */
void
smbc_setTimeout(SMBCCTX *c, int timeout)
{
        c->timeout = timeout;
}

/** Get whether to log to standard error instead of standard output */
smbc_bool
smbc_getOptionDebugToStderr(SMBCCTX *c)
{
	/* Because this is a global concept, it is better to check
	 * what is really set, rather than what we wanted set
	 * (particularly as you cannot go back to stdout). */
        return debug_get_output_is_stderr();
}

/** Set whether to log to standard error instead of standard output.
 * This option is 'sticky' - once set to true, it cannot be set to
 * false again, as it is global to the process, as once we have been
 * told that it is not safe to safe to write to stdout, we shouldn't
 * go back as we don't know it was this context that set it that way.
 */
void
smbc_setOptionDebugToStderr(SMBCCTX *c, smbc_bool b)
{
	if (b) {
		/*
		 * We do not have a unique per-thread debug state? For
		 * now, we'll just leave it up to the user. If any one
		 * context spefies debug to stderr then all will be (and
		 * will stay that way, as it is unsafe to flip back if
		 * stdout is in use for other things)
		 */
		setup_logging("libsmbclient", DEBUG_STDERR);
	}
}

/**
 * Get whether to use new-style time attribute names, e.g. WRITE_TIME rather
 * than the old-style names such as M_TIME.  This allows also setting/getting
 * CREATE_TIME which was previously unimplemented.  (Note that the old C_TIME
 * was supposed to be CHANGE_TIME but was confused and sometimes referred to
 * CREATE_TIME.)
 */
smbc_bool
smbc_getOptionFullTimeNames(SMBCCTX *c)
{
        return c->internal->full_time_names;
}

/**
 * Set whether to use new-style time attribute names, e.g. WRITE_TIME rather
 * than the old-style names such as M_TIME.  This allows also setting/getting
 * CREATE_TIME which was previously unimplemented.  (Note that the old C_TIME
 * was supposed to be CHANGE_TIME but was confused and sometimes referred to
 * CREATE_TIME.)
 */
void
smbc_setOptionFullTimeNames(SMBCCTX *c, smbc_bool b)
{
        c->internal->full_time_names = b;
}

/**
 * Get the share mode to use for files opened with SMBC_open_ctx().  The
 * default is SMBC_SHAREMODE_DENY_NONE.
 */
smbc_share_mode
smbc_getOptionOpenShareMode(SMBCCTX *c)
{
        return c->internal->share_mode;
}

/**
 * Set the share mode to use for files opened with SMBC_open_ctx().  The
 * default is SMBC_SHAREMODE_DENY_NONE.
 */
void
smbc_setOptionOpenShareMode(SMBCCTX *c, smbc_share_mode share_mode)
{
        c->internal->share_mode = share_mode;
}

/** Retrieve a previously set user data handle */
void *
smbc_getOptionUserData(SMBCCTX *c)
{
        return c->internal->user_data;
}

/** Save a user data handle */
void
smbc_setOptionUserData(SMBCCTX *c, void *user_data)
{
        c->internal->user_data = user_data;
}

/** Get the encoded value for encryption level. */
smbc_smb_encrypt_level
smbc_getOptionSmbEncryptionLevel(SMBCCTX *c)
{
        return c->internal->smb_encryption_level;
}

/** Set the encoded value for encryption level. */
void
smbc_setOptionSmbEncryptionLevel(SMBCCTX *c, smbc_smb_encrypt_level level)
{
        c->internal->smb_encryption_level = level;
}

/**
 * Get whether to treat file names as case-sensitive if we can't determine
 * when connecting to the remote share whether the file system is case
 * sensitive. This defaults to FALSE since it's most likely that if we can't
 * retrieve the file system attributes, it's a very old file system that does
 * not support case sensitivity.
 */
smbc_bool
smbc_getOptionCaseSensitive(SMBCCTX *c)
{
        return c->internal->case_sensitive;
}

/**
 * Set whether to treat file names as case-sensitive if we can't determine
 * when connecting to the remote share whether the file system is case
 * sensitive. This defaults to FALSE since it's most likely that if we can't
 * retrieve the file system attributes, it's a very old file system that does
 * not support case sensitivity.
 */
void
smbc_setOptionCaseSensitive(SMBCCTX *c, smbc_bool b)
{
        c->internal->case_sensitive = b;
}

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
smbc_getOptionBrowseMaxLmbCount(SMBCCTX *c)
{
        return c->options.browse_max_lmb_count;
}

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
smbc_setOptionBrowseMaxLmbCount(SMBCCTX *c, int count)
{
        c->options.browse_max_lmb_count = count;
}

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
smbc_getOptionUrlEncodeReaddirEntries(SMBCCTX *c)
{
        return c->options.urlencode_readdir_entries;
}

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
smbc_setOptionUrlEncodeReaddirEntries(SMBCCTX *c, smbc_bool b)
{
        c->options.urlencode_readdir_entries = b;
}

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
smbc_getOptionOneSharePerServer(SMBCCTX *c)
{
        return c->options.one_share_per_server;
}

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
smbc_setOptionOneSharePerServer(SMBCCTX *c, smbc_bool b)
{
        c->options.one_share_per_server = b;
}

/** Get whether to enable use of kerberos */
smbc_bool
smbc_getOptionUseKerberos(SMBCCTX *c)
{
        return c->flags & SMB_CTX_FLAG_USE_KERBEROS ? True : False;
}

/** Set whether to enable use of kerberos */
void
smbc_setOptionUseKerberos(SMBCCTX *c, smbc_bool b)
{
        if (b) {
                c->flags |= SMB_CTX_FLAG_USE_KERBEROS;
        } else {
                c->flags &= ~SMB_CTX_FLAG_USE_KERBEROS;
        }
}

/** Get whether to fallback after kerberos */
smbc_bool
smbc_getOptionFallbackAfterKerberos(SMBCCTX *c)
{
        return c->flags & SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS ? True : False;
}

/** Set whether to fallback after kerberos */
void
smbc_setOptionFallbackAfterKerberos(SMBCCTX *c, smbc_bool b)
{
        if (b) {
                c->flags |= SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS;
        } else {
                c->flags &= ~SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS;
        }
}

/** Get whether to automatically select anonymous login */
smbc_bool
smbc_getOptionNoAutoAnonymousLogin(SMBCCTX *c)
{
        return c->flags & SMBCCTX_FLAG_NO_AUTO_ANONYMOUS_LOGON ? True : False;
}

/** Set whether to automatically select anonymous login */
void
smbc_setOptionNoAutoAnonymousLogin(SMBCCTX *c, smbc_bool b)
{
        if (b) {
                c->flags |= SMBCCTX_FLAG_NO_AUTO_ANONYMOUS_LOGON;
        } else {
                c->flags &= ~SMBCCTX_FLAG_NO_AUTO_ANONYMOUS_LOGON;
        }
}

/** Get whether to enable use of kerberos */
smbc_bool
smbc_getOptionUseCCache(SMBCCTX *c)
{
        return c->flags & SMB_CTX_FLAG_USE_CCACHE ? True : False;
}

/** Set whether to enable use of kerberos */
void
smbc_setOptionUseCCache(SMBCCTX *c, smbc_bool b)
{
        if (b) {
                c->flags |= SMB_CTX_FLAG_USE_CCACHE;
        } else {
                c->flags &= ~SMB_CTX_FLAG_USE_CCACHE;
        }
}

/** Get the function for obtaining authentication data */
smbc_get_auth_data_fn
smbc_getFunctionAuthData(SMBCCTX *c)
{
        return c->callbacks.auth_fn;
}

/** Set the function for obtaining authentication data */
void
smbc_setFunctionAuthData(SMBCCTX *c, smbc_get_auth_data_fn fn)
{
        c->internal->auth_fn_with_context = NULL;
        c->callbacks.auth_fn = fn;
}

/** Get the new-style authentication function which includes the context. */
smbc_get_auth_data_with_context_fn
smbc_getFunctionAuthDataWithContext(SMBCCTX *c)
{
        return c->internal->auth_fn_with_context;
}

/** Set the new-style authentication function which includes the context. */
void
smbc_setFunctionAuthDataWithContext(SMBCCTX *c,
                                    smbc_get_auth_data_with_context_fn fn)
{
        c->callbacks.auth_fn = NULL;
        c->internal->auth_fn_with_context = fn;
}

/** Get the function for checking if a server is still good */
smbc_check_server_fn
smbc_getFunctionCheckServer(SMBCCTX *c)
{
        return c->callbacks.check_server_fn;
}

/** Set the function for checking if a server is still good */
void
smbc_setFunctionCheckServer(SMBCCTX *c, smbc_check_server_fn fn)
{
        c->callbacks.check_server_fn = fn;
}

/** Get the function for removing a server if unused */
smbc_remove_unused_server_fn
smbc_getFunctionRemoveUnusedServer(SMBCCTX *c)
{
        return c->callbacks.remove_unused_server_fn;
}

/** Set the function for removing a server if unused */
void
smbc_setFunctionRemoveUnusedServer(SMBCCTX *c,
                                   smbc_remove_unused_server_fn fn)
{
        c->callbacks.remove_unused_server_fn = fn;
}

/** Get the function for adding a cached server */
smbc_add_cached_srv_fn
smbc_getFunctionAddCachedServer(SMBCCTX *c)
{
        return c->callbacks.add_cached_srv_fn;
}

/** Set the function for adding a cached server */
void
smbc_setFunctionAddCachedServer(SMBCCTX *c, smbc_add_cached_srv_fn fn)
{
        c->callbacks.add_cached_srv_fn = fn;
}

/** Get the function for server cache lookup */
smbc_get_cached_srv_fn
smbc_getFunctionGetCachedServer(SMBCCTX *c)
{
        return c->callbacks.get_cached_srv_fn;
}

/** Set the function for server cache lookup */
void
smbc_setFunctionGetCachedServer(SMBCCTX *c, smbc_get_cached_srv_fn fn)
{
        c->callbacks.get_cached_srv_fn = fn;
}

/** Get the function for server cache removal */
smbc_remove_cached_srv_fn
smbc_getFunctionRemoveCachedServer(SMBCCTX *c)
{
        return c->callbacks.remove_cached_srv_fn;
}

/** Set the function for server cache removal */
void
smbc_setFunctionRemoveCachedServer(SMBCCTX *c,
                                   smbc_remove_cached_srv_fn fn)
{
        c->callbacks.remove_cached_srv_fn = fn;
}

/**
 * Get the function for server cache purging.  This function tries to
 * remove all cached servers (e.g. on disconnect)
 */
smbc_purge_cached_fn
smbc_getFunctionPurgeCachedServers(SMBCCTX *c)
{
        return c->callbacks.purge_cached_fn;
}

/** Set the function to store private data of the server cache */
void smbc_setServerCacheData(SMBCCTX *c, struct smbc_server_cache * cache)
{
        c->internal->server_cache = cache;
}

/** Get the function to store private data of the server cache */
struct smbc_server_cache * smbc_getServerCacheData(SMBCCTX *c)
{
        return c->internal->server_cache;
}


/**
 * Set the function for server cache purging.  This function tries to
 * remove all cached servers (e.g. on disconnect)
 */
void
smbc_setFunctionPurgeCachedServers(SMBCCTX *c, smbc_purge_cached_fn fn)
{
        c->callbacks.purge_cached_fn = fn;
}

/**
 * Callable functions for files.
 */

smbc_open_fn
smbc_getFunctionOpen(SMBCCTX *c)
{
        return c->open;
}

void
smbc_setFunctionOpen(SMBCCTX *c, smbc_open_fn fn)
{
        c->open = fn;
}

smbc_creat_fn
smbc_getFunctionCreat(SMBCCTX *c)
{
        return c->creat;
}

void
smbc_setFunctionCreat(SMBCCTX *c, smbc_creat_fn fn)
{
        c->creat = fn;
}

smbc_read_fn
smbc_getFunctionRead(SMBCCTX *c)
{
        return c->read;
}

void
smbc_setFunctionRead(SMBCCTX *c, smbc_read_fn fn)
{
        c->read = fn;
}

smbc_write_fn
smbc_getFunctionWrite(SMBCCTX *c)
{
        return c->write;
}

void
smbc_setFunctionWrite(SMBCCTX *c, smbc_write_fn fn)
{
        c->write = fn;
}

smbc_unlink_fn
smbc_getFunctionUnlink(SMBCCTX *c)
{
        return c->unlink;
}

void
smbc_setFunctionUnlink(SMBCCTX *c, smbc_unlink_fn fn)
{
        c->unlink = fn;
}

smbc_rename_fn
smbc_getFunctionRename(SMBCCTX *c)
{
        return c->rename;
}

void
smbc_setFunctionRename(SMBCCTX *c, smbc_rename_fn fn)
{
        c->rename = fn;
}

smbc_lseek_fn
smbc_getFunctionLseek(SMBCCTX *c)
{
        return c->lseek;
}

void
smbc_setFunctionLseek(SMBCCTX *c, smbc_lseek_fn fn)
{
        c->lseek = fn;
}

smbc_stat_fn
smbc_getFunctionStat(SMBCCTX *c)
{
        return c->stat;
}

void
smbc_setFunctionStat(SMBCCTX *c, smbc_stat_fn fn)
{
        c->stat = fn;
}

smbc_fstat_fn
smbc_getFunctionFstat(SMBCCTX *c)
{
        return c->fstat;
}

void
smbc_setFunctionFstat(SMBCCTX *c, smbc_fstat_fn fn)
{
        c->fstat = fn;
}

smbc_statvfs_fn
smbc_getFunctionStatVFS(SMBCCTX *c)
{
        return c->internal->posix_emu.statvfs_fn;
}

void
smbc_setFunctionStatVFS(SMBCCTX *c, smbc_statvfs_fn fn)
{
        c->internal->posix_emu.statvfs_fn = fn;
}

smbc_fstatvfs_fn
smbc_getFunctionFstatVFS(SMBCCTX *c)
{
        return c->internal->posix_emu.fstatvfs_fn;
}

void
smbc_setFunctionFstatVFS(SMBCCTX *c, smbc_fstatvfs_fn fn)
{
        c->internal->posix_emu.fstatvfs_fn = fn;
}

smbc_ftruncate_fn
smbc_getFunctionFtruncate(SMBCCTX *c)
{
        return c->internal->posix_emu.ftruncate_fn;
}

void
smbc_setFunctionFtruncate(SMBCCTX *c, smbc_ftruncate_fn fn)
{
        c->internal->posix_emu.ftruncate_fn = fn;
}

smbc_close_fn
smbc_getFunctionClose(SMBCCTX *c)
{
        return c->close_fn;
}

void
smbc_setFunctionClose(SMBCCTX *c, smbc_close_fn fn)
{
        c->close_fn = fn;
}


/**
 * Callable functions for directories.
 */

smbc_opendir_fn
smbc_getFunctionOpendir(SMBCCTX *c)
{
        return c->opendir;
}

void
smbc_setFunctionOpendir(SMBCCTX *c, smbc_opendir_fn fn)
{
        c->opendir = fn;
}

smbc_closedir_fn
smbc_getFunctionClosedir(SMBCCTX *c)
{
        return c->closedir;
}

void
smbc_setFunctionClosedir(SMBCCTX *c, smbc_closedir_fn fn)
{
        c->closedir = fn;
}

smbc_readdir_fn
smbc_getFunctionReaddir(SMBCCTX *c)
{
        return c->readdir;
}

void
smbc_setFunctionReaddir(SMBCCTX *c, smbc_readdir_fn fn)
{
        c->readdir = fn;
}

smbc_getdents_fn
smbc_getFunctionGetdents(SMBCCTX *c)
{
        return c->getdents;
}

void
smbc_setFunctionGetdents(SMBCCTX *c, smbc_getdents_fn fn)
{
        c->getdents = fn;
}

smbc_mkdir_fn
smbc_getFunctionMkdir(SMBCCTX *c)
{
        return c->mkdir;
}

void
smbc_setFunctionMkdir(SMBCCTX *c, smbc_mkdir_fn fn)
{
        c->mkdir = fn;
}

smbc_rmdir_fn
smbc_getFunctionRmdir(SMBCCTX *c)
{
        return c->rmdir;
}

void
smbc_setFunctionRmdir(SMBCCTX *c, smbc_rmdir_fn fn)
{
        c->rmdir = fn;
}

smbc_telldir_fn
smbc_getFunctionTelldir(SMBCCTX *c)
{
        return c->telldir;
}

void
smbc_setFunctionTelldir(SMBCCTX *c, smbc_telldir_fn fn)
{
        c->telldir = fn;
}

smbc_lseekdir_fn
smbc_getFunctionLseekdir(SMBCCTX *c)
{
        return c->lseekdir;
}

void
smbc_setFunctionLseekdir(SMBCCTX *c, smbc_lseekdir_fn fn)
{
        c->lseekdir = fn;
}

smbc_fstatdir_fn
smbc_getFunctionFstatdir(SMBCCTX *c)
{
        return c->fstatdir;
}

void
smbc_setFunctionFstatdir(SMBCCTX *c, smbc_fstatdir_fn fn)
{
        c->fstatdir = fn;
}


/**
 * Callable functions applicable to both files and directories.
 */

smbc_chmod_fn
smbc_getFunctionChmod(SMBCCTX *c)
{
        return c->chmod;
}

void
smbc_setFunctionChmod(SMBCCTX *c, smbc_chmod_fn fn)
{
        c->chmod = fn;
}

smbc_utimes_fn
smbc_getFunctionUtimes(SMBCCTX *c)
{
        return c->utimes;
}

void
smbc_setFunctionUtimes(SMBCCTX *c, smbc_utimes_fn fn)
{
        c->utimes = fn;
}

smbc_setxattr_fn
smbc_getFunctionSetxattr(SMBCCTX *c)
{
        return c->setxattr;
}

void
smbc_setFunctionSetxattr(SMBCCTX *c, smbc_setxattr_fn fn)
{
        c->setxattr = fn;
}

smbc_getxattr_fn
smbc_getFunctionGetxattr(SMBCCTX *c)
{
        return c->getxattr;
}

void
smbc_setFunctionGetxattr(SMBCCTX *c, smbc_getxattr_fn fn)
{
        c->getxattr = fn;
}

smbc_removexattr_fn
smbc_getFunctionRemovexattr(SMBCCTX *c)
{
        return c->removexattr;
}

void
smbc_setFunctionRemovexattr(SMBCCTX *c, smbc_removexattr_fn fn)
{
        c->removexattr = fn;
}

smbc_listxattr_fn
smbc_getFunctionListxattr(SMBCCTX *c)
{
        return c->listxattr;
}

void
smbc_setFunctionListxattr(SMBCCTX *c, smbc_listxattr_fn fn)
{
        c->listxattr = fn;
}


/**
 * Callable functions related to printing
 */

smbc_print_file_fn
smbc_getFunctionPrintFile(SMBCCTX *c)
{
        return c->print_file;
}

void
smbc_setFunctionPrintFile(SMBCCTX *c, smbc_print_file_fn fn)
{
        c->print_file = fn;
}

smbc_open_print_job_fn
smbc_getFunctionOpenPrintJob(SMBCCTX *c)
{
        return c->open_print_job;
}

void
smbc_setFunctionOpenPrintJob(SMBCCTX *c,
                             smbc_open_print_job_fn fn)
{
        c->open_print_job = fn;
}

smbc_list_print_jobs_fn
smbc_getFunctionListPrintJobs(SMBCCTX *c)
{
        return c->list_print_jobs;
}

void
smbc_setFunctionListPrintJobs(SMBCCTX *c,
                              smbc_list_print_jobs_fn fn)
{
        c->list_print_jobs = fn;
}

smbc_unlink_print_job_fn
smbc_getFunctionUnlinkPrintJob(SMBCCTX *c)
{
        return c->unlink_print_job;
}

void
smbc_setFunctionUnlinkPrintJob(SMBCCTX *c,
                               smbc_unlink_print_job_fn fn)
{
        c->unlink_print_job = fn;
}

