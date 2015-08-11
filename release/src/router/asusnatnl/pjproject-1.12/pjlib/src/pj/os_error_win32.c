/* $Id: os_error_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/errno.h>
#include <pj/assert.h>
#include <pj/compat/stdarg.h>
#include <pj/unicode.h>
#include <pj/string.h>


#if defined(PJ_HAS_WINSOCK2_H) && PJ_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#elif defined(PJ_HAS_WINSOCK_H) && PJ_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif


/*
 * From Apache's APR:
 */
#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING!=0)

static const struct {
    pj_os_err_type code;
    const char *msg;
} gaErrorList[] = {
    PJ_BUILD_ERR( WSAEINTR,           "Interrupted system call"),
    PJ_BUILD_ERR( WSAEBADF,           "Bad file number"),
    PJ_BUILD_ERR( WSAEACCES,          "Permission denied"),
    PJ_BUILD_ERR( WSAEFAULT,          "Bad address"),
    PJ_BUILD_ERR( WSAEINVAL,          "Invalid argument"),
    PJ_BUILD_ERR( WSAEMFILE,          "Too many open sockets"),
    PJ_BUILD_ERR( WSAEWOULDBLOCK,     "Operation would block"),
    PJ_BUILD_ERR( WSAEINPROGRESS,     "Operation now in progress"),
    PJ_BUILD_ERR( WSAEALREADY,        "Operation already in progress"),
    PJ_BUILD_ERR( WSAENOTSOCK,        "Socket operation on non-socket"),
    PJ_BUILD_ERR( WSAEDESTADDRREQ,    "Destination address required"),
    PJ_BUILD_ERR( WSAEMSGSIZE,        "Message too long"),
    PJ_BUILD_ERR( WSAEPROTOTYPE,      "Protocol wrong type for socket"),
    PJ_BUILD_ERR( WSAENOPROTOOPT,     "Bad protocol option"),
    PJ_BUILD_ERR( WSAEPROTONOSUPPORT, "Protocol not supported"),
    PJ_BUILD_ERR( WSAESOCKTNOSUPPORT, "Socket type not supported"),
    PJ_BUILD_ERR( WSAEOPNOTSUPP,      "Operation not supported on socket"),
    PJ_BUILD_ERR( WSAEPFNOSUPPORT,    "Protocol family not supported"),
    PJ_BUILD_ERR( WSAEAFNOSUPPORT,    "Address family not supported"),
    PJ_BUILD_ERR( WSAEADDRINUSE,      "Address already in use"),
    PJ_BUILD_ERR( WSAEADDRNOTAVAIL,   "Can't assign requested address"),
    PJ_BUILD_ERR( WSAENETDOWN,        "Network is down"),
    PJ_BUILD_ERR( WSAENETUNREACH,     "Network is unreachable"),
    PJ_BUILD_ERR( WSAENETRESET,       "Net connection reset"),
    PJ_BUILD_ERR( WSAECONNABORTED,    "Software caused connection abort"),
    PJ_BUILD_ERR( WSAECONNRESET,      "Connection reset by peer"),
    PJ_BUILD_ERR( WSAENOBUFS,         "No buffer space available"),
    PJ_BUILD_ERR( WSAEISCONN,         "Socket is already connected"),
    PJ_BUILD_ERR( WSAENOTCONN,        "Socket is not connected"),
    PJ_BUILD_ERR( WSAESHUTDOWN,       "Can't send after socket shutdown"),
    PJ_BUILD_ERR( WSAETOOMANYREFS,    "Too many references, can't splice"),
    PJ_BUILD_ERR( WSAETIMEDOUT,       "Connection timed out"),
    PJ_BUILD_ERR( WSAECONNREFUSED,    "Connection refused"),
    PJ_BUILD_ERR( WSAELOOP,           "Too many levels of symbolic links"),
    PJ_BUILD_ERR( WSAENAMETOOLONG,    "File name too long"),
    PJ_BUILD_ERR( WSAEHOSTDOWN,       "Host is down"),
    PJ_BUILD_ERR( WSAEHOSTUNREACH,    "No route to host"),
    PJ_BUILD_ERR( WSAENOTEMPTY,       "Directory not empty"),
    PJ_BUILD_ERR( WSAEPROCLIM,        "Too many processes"),
    PJ_BUILD_ERR( WSAEUSERS,          "Too many users"),
    PJ_BUILD_ERR( WSAEDQUOT,          "Disc quota exceeded"),
    PJ_BUILD_ERR( WSAESTALE,          "Stale NFS file handle"),
    PJ_BUILD_ERR( WSAEREMOTE,         "Too many levels of remote in path"),
    PJ_BUILD_ERR( WSASYSNOTREADY,     "Network system is unavailable"),
    PJ_BUILD_ERR( WSAVERNOTSUPPORTED, "Winsock version out of range"),
    PJ_BUILD_ERR( WSANOTINITIALISED,  "WSAStartup not yet called"),
    PJ_BUILD_ERR( WSAEDISCON,         "Graceful shutdown in progress"),
/*
#define WSAENOMORE              (WSABASEERR+102)
#define WSAECANCELLED           (WSABASEERR+103)
#define WSAEINVALIDPROCTABLE    (WSABASEERR+104)
#define WSAEINVALIDPROVIDER     (WSABASEERR+105)
#define WSAEPROVIDERFAILEDINIT  (WSABASEERR+106)
#define WSASYSCALLFAILURE       (WSABASEERR+107)
#define WSASERVICE_NOT_FOUND    (WSABASEERR+108)
#define WSATYPE_NOT_FOUND       (WSABASEERR+109)
#define WSA_E_NO_MORE           (WSABASEERR+110)
#define WSA_E_CANCELLED         (WSABASEERR+111)
#define WSAEREFUSED             (WSABASEERR+112)
 */
    PJ_BUILD_ERR( WSAHOST_NOT_FOUND,  "Host not found"),
/*
#define WSATRY_AGAIN            (WSABASEERR+1002)
#define WSANO_RECOVERY          (WSABASEERR+1003)
 */
    PJ_BUILD_ERR( WSANO_DATA,         "No host data of that type was found"),
    {0, NULL}
};

#endif	/* PJ_HAS_ERROR_STRING */



PJ_DEF(pj_status_t) pj_get_os_error(void)
{
    return PJ_STATUS_FROM_OS(GetLastError());
}

PJ_DEF(void) pj_set_os_error(pj_status_t code)
{
    SetLastError(PJ_STATUS_TO_OS(code));
}

PJ_DEF(pj_status_t) pj_get_netos_error(void)
{
    return PJ_STATUS_FROM_OS(WSAGetLastError());
}

PJ_DEF(void) pj_set_netos_error(pj_status_t code)
{
    WSASetLastError(PJ_STATUS_TO_OS(code));
}

/* 
 * platform_strerror()
 *
 * Platform specific error message. This file is called by pj_strerror() 
 * in errno.c 
 */
int platform_strerror( pj_os_err_type os_errcode, 
                       char *buf, pj_size_t bufsize)
{
    int len = 0;
    PJ_DECL_UNICODE_TEMP_BUF(wbuf,128);

    pj_assert(buf != NULL);
    pj_assert(bufsize >= 0);

    /*
     * MUST NOT check stack here.
     * This function might be called from PJ_CHECK_STACK() itself!
       //PJ_CHECK_STACK();
     */

    if (!len) {
#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING!=0)
	int i;
        for (i = 0; gaErrorList[i].msg; ++i) {
            if (gaErrorList[i].code == os_errcode) {
                len = strlen(gaErrorList[i].msg);
		if ((pj_size_t)len >= bufsize) {
		    len = bufsize-1;
		}
		pj_memcpy(buf, gaErrorList[i].msg, len);
		buf[len] = '\0';
                break;
            }
        }
#endif	/* PJ_HAS_ERROR_STRING */

    }


    if (!len) {
#if PJ_NATIVE_STRING_IS_UNICODE
	len = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM 
			     | FORMAT_MESSAGE_IGNORE_INSERTS,
			     NULL,
			     os_errcode,
			     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			     wbuf,
			     sizeof(wbuf),
			     NULL);
	if (len) {
	    pj_unicode_to_ansi(wbuf, len, buf, bufsize);
	}
#else
	len = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM 
			     | FORMAT_MESSAGE_IGNORE_INSERTS,
			     NULL,
			     os_errcode,
			     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			     buf,
			     bufsize,
			     NULL);
	buf[bufsize-1] = '\0';
#endif

	if (len) {
	    /* Remove trailing newlines. */
	    while (len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
		buf[len-1] = '\0';
		--len;
	    }
	}
    }

    if (!len) {
	len = pj_ansi_snprintf( buf, bufsize, "Win32 error code %u", 
				(unsigned)os_errcode);
	if (len < 0 || len >= (int)bufsize)
	    len = bufsize-1;
	buf[len] = '\0';
    }

    return len;
}

