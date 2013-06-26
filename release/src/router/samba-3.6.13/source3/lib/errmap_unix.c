/* 
 *  Unix SMB/CIFS implementation.
 *  map unix to NT errors, an excerpt of libsmb/errormap.c
 *  Copyright (C) Andrew Tridgell 2001
 *  Copyright (C) Andrew Bartlett 2001
 *  Copyright (C) Tim Potter 2000
 *  Copyright (C) Jeremy Allison 2007
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

/* Mapping from Unix, to NT error numbers */

const struct unix_error_map unix_dos_nt_errmap[] = {
	{ EPERM, ERRDOS, ERRnoaccess, NT_STATUS_ACCESS_DENIED },
	{ EACCES, ERRDOS, ERRnoaccess, NT_STATUS_ACCESS_DENIED },
	{ ENOENT, ERRDOS, ERRbadfile, NT_STATUS_OBJECT_NAME_NOT_FOUND },
	{ ENOTDIR, ERRDOS, ERRbadpath,  NT_STATUS_NOT_A_DIRECTORY },
	{ EIO, ERRHRD, ERRgeneral, NT_STATUS_IO_DEVICE_ERROR },
	{ EBADF, ERRSRV, ERRsrverror, NT_STATUS_INVALID_HANDLE },
	{ EINVAL, ERRSRV, ERRsrverror, NT_STATUS_INVALID_PARAMETER },
	{ EEXIST, ERRDOS, ERRfilexists, NT_STATUS_OBJECT_NAME_COLLISION},
	{ ENFILE, ERRDOS, ERRnofids, NT_STATUS_TOO_MANY_OPENED_FILES },
	{ EMFILE, ERRDOS, ERRnofids, NT_STATUS_TOO_MANY_OPENED_FILES },
	{ ENOSPC, ERRHRD, ERRdiskfull, NT_STATUS_DISK_FULL },
	{ ENOMEM, ERRDOS, ERRnomem, NT_STATUS_NO_MEMORY },
	{ EISDIR, ERRDOS, ERRnoaccess, NT_STATUS_FILE_IS_A_DIRECTORY},
	{ EMLINK, ERRDOS, ERRgeneral, NT_STATUS_TOO_MANY_LINKS },
	{ EINTR,  ERRHRD, ERRgeneral, NT_STATUS_RETRY },
	{ ENOSYS, ERRDOS, ERRunsup, NT_STATUS_NOT_SUPPORTED },
#ifdef ELOOP
	{ ELOOP, ERRDOS, ERRbadpath, NT_STATUS_OBJECT_PATH_NOT_FOUND },
#endif
#ifdef EFTYPE
	{ EFTYPE, ERRDOS, ERRbadpath, NT_STATUS_OBJECT_PATH_NOT_FOUND },
#endif
#ifdef EDQUOT
	{ EDQUOT, ERRHRD, ERRdiskfull, NT_STATUS_DISK_FULL }, /* Windows apps need this, not NT_STATUS_QUOTA_EXCEEDED */
#endif
#ifdef ENOTEMPTY
	{ ENOTEMPTY, ERRDOS, ERRnoaccess, NT_STATUS_DIRECTORY_NOT_EMPTY },
#endif
#ifdef EXDEV
	{ EXDEV, ERRDOS, ERRdiffdevice, NT_STATUS_NOT_SAME_DEVICE },
#endif
#ifdef EROFS
	{ EROFS, ERRHRD, ERRnowrite, NT_STATUS_ACCESS_DENIED },
#endif
#ifdef ENAMETOOLONG
	{ ENAMETOOLONG, ERRDOS, 206, NT_STATUS_OBJECT_NAME_INVALID },
#endif
#ifdef EFBIG
	{ EFBIG, ERRHRD, ERRdiskfull, NT_STATUS_DISK_FULL },
#endif
#ifdef ENOBUFS
	{ ENOBUFS, ERRDOS, ERRnomem, NT_STATUS_INSUFFICIENT_RESOURCES },
#endif
	{ EAGAIN, ERRDOS, 111, NT_STATUS_NETWORK_BUSY },
#ifdef EADDRINUSE
	{ EADDRINUSE, ERRDOS, 52, NT_STATUS_ADDRESS_ALREADY_ASSOCIATED},
#endif
#ifdef ENETUNREACH
	{ ENETUNREACH, ERRHRD, ERRgeneral, NT_STATUS_NETWORK_UNREACHABLE},
#endif
#ifdef EHOSTUNREACH
		{ EHOSTUNREACH, ERRHRD, ERRgeneral, NT_STATUS_HOST_UNREACHABLE},
#endif
#ifdef ECONNREFUSED
	{ ECONNREFUSED, ERRHRD, ERRgeneral, NT_STATUS_CONNECTION_REFUSED},
#endif
#ifdef ETIMEDOUT
	{ ETIMEDOUT, ERRHRD, 121, NT_STATUS_IO_TIMEOUT},
#endif
#ifdef ECONNABORTED
	{ ECONNABORTED, ERRHRD, ERRgeneral, NT_STATUS_CONNECTION_ABORTED},
#endif
#ifdef ECONNRESET
	{ ECONNRESET, ERRHRD, ERRgeneral, NT_STATUS_CONNECTION_RESET},
#endif
#ifdef ENODEV
	{ ENODEV, ERRDOS, 55, NT_STATUS_DEVICE_DOES_NOT_EXIST},
#endif
#ifdef EPIPE
	{ EPIPE, ERRDOS, 109, NT_STATUS_PIPE_BROKEN},
#endif
#ifdef EWOULDBLOCK
	{ EWOULDBLOCK, ERRDOS, 111, NT_STATUS_NETWORK_BUSY },
#endif
#ifdef ENOATTR
	{ ENOATTR, ERRDOS, ERRbadfile, NT_STATUS_NOT_FOUND },
#endif
#ifdef ECANCELED
	{ ECANCELED, ERRDOS, ERRbadfid, NT_STATUS_CANCELLED},
#endif
#ifdef ENOTSUP
        { ENOTSUP, ERRSRV, ERRnosupport, NT_STATUS_NOT_SUPPORTED},
#endif
	{ 0, 0, 0, NT_STATUS_OK }
};

/*********************************************************************
 Map an NT error code from a Unix error code.
*********************************************************************/

NTSTATUS map_nt_error_from_unix(int unix_error)
{
	int i = 0;

	if (unix_error == 0) {
		/* we map this to an error, not success, as this
		   function is only called in an error path. Lots of
		   our virtualised functions may fail without making a
		   unix system call that fails (such as when they are
		   checking for some handle existing), so unix_error
		   may be unset
		*/
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Look through list */
	while(unix_dos_nt_errmap[i].unix_error != 0) {
		if (unix_dos_nt_errmap[i].unix_error == unix_error)
			return unix_dos_nt_errmap[i].nt_error;
		i++;
	}

	/* Default return */
	return NT_STATUS_ACCESS_DENIED;
}

/* Convert a Unix error code to a WERROR. */
WERROR unix_to_werror(int unix_error)
{
	return ntstatus_to_werror(map_nt_error_from_unix(unix_error));
}

/* Return a UNIX errno from a NT status code */
static const struct {
	NTSTATUS status;
	int error;
} nt_errno_map[] = {
        {NT_STATUS_ACCESS_VIOLATION, EACCES},
        {NT_STATUS_INVALID_HANDLE, EBADF},
        {NT_STATUS_ACCESS_DENIED, EACCES},
        {NT_STATUS_OBJECT_NAME_NOT_FOUND, ENOENT},
        {NT_STATUS_OBJECT_PATH_NOT_FOUND, ENOENT},
        {NT_STATUS_SHARING_VIOLATION, EBUSY},
        {NT_STATUS_OBJECT_PATH_INVALID, ENOTDIR},
        {NT_STATUS_OBJECT_NAME_COLLISION, EEXIST},
        {NT_STATUS_PATH_NOT_COVERED, ENOENT},
	{NT_STATUS_UNSUCCESSFUL, EINVAL},
	{NT_STATUS_NOT_IMPLEMENTED, ENOSYS},
	{NT_STATUS_IN_PAGE_ERROR, EFAULT}, 
	{NT_STATUS_BAD_NETWORK_NAME, ENOENT},
#ifdef EDQUOT
	{NT_STATUS_PAGEFILE_QUOTA, EDQUOT},
	{NT_STATUS_QUOTA_EXCEEDED, EDQUOT},
	{NT_STATUS_REGISTRY_QUOTA_LIMIT, EDQUOT},
	{NT_STATUS_LICENSE_QUOTA_EXCEEDED, EDQUOT},
#endif
#ifdef ETIME
	{NT_STATUS_TIMER_NOT_CANCELED, ETIME},
#endif
	{NT_STATUS_INVALID_PARAMETER, EINVAL},
	{NT_STATUS_NO_SUCH_DEVICE, ENODEV},
	{NT_STATUS_NO_SUCH_FILE, ENOENT},
#ifdef ENODATA
	{NT_STATUS_END_OF_FILE, ENODATA}, 
#endif
#ifdef ENOMEDIUM
	{NT_STATUS_NO_MEDIA_IN_DEVICE, ENOMEDIUM}, 
	{NT_STATUS_NO_MEDIA, ENOMEDIUM},
#endif
	{NT_STATUS_NONEXISTENT_SECTOR, ESPIPE}, 
        {NT_STATUS_NO_MEMORY, ENOMEM},
	{NT_STATUS_CONFLICTING_ADDRESSES, EADDRINUSE},
	{NT_STATUS_NOT_MAPPED_VIEW, EINVAL},
	{NT_STATUS_UNABLE_TO_FREE_VM, EADDRINUSE},
	{NT_STATUS_ACCESS_DENIED, EACCES}, 
	{NT_STATUS_BUFFER_TOO_SMALL, ENOBUFS},
	{NT_STATUS_WRONG_PASSWORD, EACCES},
	{NT_STATUS_LOGON_FAILURE, EACCES},
	{NT_STATUS_INVALID_WORKSTATION, EACCES},
	{NT_STATUS_INVALID_LOGON_HOURS, EACCES},
	{NT_STATUS_PASSWORD_EXPIRED, EACCES},
	{NT_STATUS_ACCOUNT_DISABLED, EACCES},
	{NT_STATUS_DISK_FULL, ENOSPC},
	{NT_STATUS_INVALID_PIPE_STATE, EPIPE},
	{NT_STATUS_PIPE_BUSY, EPIPE},
	{NT_STATUS_PIPE_DISCONNECTED, EPIPE},
	{NT_STATUS_PIPE_NOT_AVAILABLE, ENOSYS},
	{NT_STATUS_FILE_IS_A_DIRECTORY, EISDIR},
	{NT_STATUS_NOT_SUPPORTED, ENOSYS},
	{NT_STATUS_NOT_A_DIRECTORY, ENOTDIR},
	{NT_STATUS_DIRECTORY_NOT_EMPTY, ENOTEMPTY},
	{NT_STATUS_NETWORK_UNREACHABLE, ENETUNREACH},
	{NT_STATUS_HOST_UNREACHABLE, EHOSTUNREACH},
	{NT_STATUS_CONNECTION_ABORTED, ECONNABORTED},
	{NT_STATUS_CONNECTION_REFUSED, ECONNREFUSED},
	{NT_STATUS_TOO_MANY_LINKS, EMLINK},
	{NT_STATUS_NETWORK_BUSY, EBUSY},
	{NT_STATUS_DEVICE_DOES_NOT_EXIST, ENODEV},
#ifdef ELIBACC
	{NT_STATUS_DLL_NOT_FOUND, ELIBACC},
#endif
	{NT_STATUS_PIPE_BROKEN, EPIPE},
	{NT_STATUS_REMOTE_NOT_LISTENING, ECONNREFUSED},
	{NT_STATUS_NETWORK_ACCESS_DENIED, EACCES},
	{NT_STATUS_TOO_MANY_OPENED_FILES, EMFILE},
#ifdef EPROTO
	{NT_STATUS_DEVICE_PROTOCOL_ERROR, EPROTO},
#endif
	{NT_STATUS_FLOAT_OVERFLOW, ERANGE},
	{NT_STATUS_FLOAT_UNDERFLOW, ERANGE},
	{NT_STATUS_INTEGER_OVERFLOW, ERANGE},
	{NT_STATUS_MEDIA_WRITE_PROTECTED, EROFS},
	{NT_STATUS_PIPE_CONNECTED, EISCONN},
	{NT_STATUS_MEMORY_NOT_ALLOCATED, EFAULT},
	{NT_STATUS_FLOAT_INEXACT_RESULT, ERANGE},
	{NT_STATUS_ILL_FORMED_PASSWORD, EACCES},
	{NT_STATUS_PASSWORD_RESTRICTION, EACCES},
	{NT_STATUS_ACCOUNT_RESTRICTION, EACCES},
	{NT_STATUS_PORT_CONNECTION_REFUSED, ECONNREFUSED},
	{NT_STATUS_NAME_TOO_LONG, ENAMETOOLONG},
	{NT_STATUS_REMOTE_DISCONNECT, ESHUTDOWN},
	{NT_STATUS_CONNECTION_DISCONNECTED, ECONNABORTED},
	{NT_STATUS_CONNECTION_RESET, ENETRESET},
#ifdef ENOTUNIQ
	{NT_STATUS_IP_ADDRESS_CONFLICT1, ENOTUNIQ},
	{NT_STATUS_IP_ADDRESS_CONFLICT2, ENOTUNIQ},
#endif
	{NT_STATUS_PORT_MESSAGE_TOO_LONG, EMSGSIZE},
	{NT_STATUS_PROTOCOL_UNREACHABLE, ENOPROTOOPT},
	{NT_STATUS_ADDRESS_ALREADY_EXISTS, EADDRINUSE},
	{NT_STATUS_PORT_UNREACHABLE, EHOSTUNREACH},
	{NT_STATUS_IO_TIMEOUT, ETIMEDOUT},
	{NT_STATUS_RETRY, EAGAIN},
#ifdef ENOTUNIQ
	{NT_STATUS_DUPLICATE_NAME, ENOTUNIQ},
#endif
#ifdef ECOMM
	{NT_STATUS_NET_WRITE_FAULT, ECOMM},
#endif
#ifdef EXDEV
	{NT_STATUS_NOT_SAME_DEVICE, EXDEV},
#endif
	{NT_STATUS(0), 0}
};

int map_errno_from_nt_status(NTSTATUS status)
{
	int i;
	DEBUG(10,("map_errno_from_nt_status: 32 bit codes: code=%08x\n",
		NT_STATUS_V(status)));

	/* Status codes without this bit set are not errors */

	if (!(NT_STATUS_V(status) & 0xc0000000)) {
		return 0;
	}

	for (i=0;nt_errno_map[i].error;i++) {
		if (NT_STATUS_V(nt_errno_map[i].status) ==
			    NT_STATUS_V(status)) {
			return nt_errno_map[i].error;
		}
	}

	/* for all other cases - a default code */
	return EINVAL;
}
