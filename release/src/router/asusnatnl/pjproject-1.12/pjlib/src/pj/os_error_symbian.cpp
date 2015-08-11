/* $Id: os_error_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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

#include <e32err.h>
#include <in_sock.h>


#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING!=0)
static const struct {
    pj_os_err_type code;
    const char *msg;
} gaErrorList[] = {
    /*
     * Generic error -1 to -46
     */
    PJ_BUILD_ERR( KErrNotFound,	    "Unable to find the specified object"),
    PJ_BUILD_ERR( KErrGeneral,	    "General (unspecified) error"),
    PJ_BUILD_ERR( KErrCancel,	    "The operation was cancelled"),
    PJ_BUILD_ERR( KErrNoMemory,	    "Not enough memory"),
    PJ_BUILD_ERR( KErrNotSupported, "The operation requested is not supported"),
    PJ_BUILD_ERR( KErrArgument,	    "Bad request"),
    PJ_BUILD_ERR( KErrTotalLossOfPrecision, "Total loss of precision"),
    PJ_BUILD_ERR( KErrBadHandle,    "Bad object"),
    PJ_BUILD_ERR( KErrOverflow,	    "Overflow"),
    PJ_BUILD_ERR( KErrUnderflow,    "Underflow"),
    PJ_BUILD_ERR( KErrAlreadyExists,"Already exists"),
    PJ_BUILD_ERR( KErrPathNotFound, "Unable to find the specified folder"),
    PJ_BUILD_ERR( KErrDied,	    "Closed"),
    PJ_BUILD_ERR( KErrInUse,	    "The specified object is currently in use by another program"),
    PJ_BUILD_ERR( KErrServerTerminated,	    "Server has closed"),
    PJ_BUILD_ERR( KErrServerBusy,   "Server busy"),
    PJ_BUILD_ERR( KErrCompletion,   "Completion error"),
    PJ_BUILD_ERR( KErrNotReady,	    "Not ready"),
    PJ_BUILD_ERR( KErrUnknown,	    "Unknown error"),
    PJ_BUILD_ERR( KErrCorrupt,	    "Corrupt"),
    PJ_BUILD_ERR( KErrAccessDenied, "Access denied"),
    PJ_BUILD_ERR( KErrLocked,	    "Locked"),
    PJ_BUILD_ERR( KErrWrite,	    "Failed to write"),
    PJ_BUILD_ERR( KErrDisMounted,   "Wrong disk present"),
    PJ_BUILD_ERR( KErrEof,	    "Unexpected end of file"),
    PJ_BUILD_ERR( KErrDiskFull,	    "Disk full"),
    PJ_BUILD_ERR( KErrBadDriver,    "Bad device driver"),
    PJ_BUILD_ERR( KErrBadName,	    "Bad name"),
    PJ_BUILD_ERR( KErrCommsLineFail,"Comms line failed"),
    PJ_BUILD_ERR( KErrCommsFrame,   "Comms frame error"),
    PJ_BUILD_ERR( KErrCommsOverrun, "Comms overrun error"),
    PJ_BUILD_ERR( KErrCommsParity,  "Comms parity error"),
    PJ_BUILD_ERR( KErrTimedOut,	    "Timed out"),
    PJ_BUILD_ERR( KErrCouldNotConnect, "Failed to connect"),
    PJ_BUILD_ERR( KErrCouldNotDisconnect, "Failed to disconnect"),
    PJ_BUILD_ERR( KErrDisconnected, "Disconnected"),
    PJ_BUILD_ERR( KErrBadLibraryEntryPoint, "Bad library entry point"),
    PJ_BUILD_ERR( KErrBadDescriptor,"Bad descriptor"),
    PJ_BUILD_ERR( KErrAbort,	    "Interrupted"),
    PJ_BUILD_ERR( KErrTooBig,	    "Too big"),
    PJ_BUILD_ERR( KErrDivideByZero, "Divide by zero"),
    PJ_BUILD_ERR( KErrBadPower,	    "Batteries too low"),
    PJ_BUILD_ERR( KErrDirFull,	    "Folder full"),
    PJ_BUILD_ERR( KErrHardwareNotAvailable, ""),
    PJ_BUILD_ERR( KErrSessionClosed,	    ""),
    PJ_BUILD_ERR( KErrPermissionDenied,     ""),

    /*
     * Socket errors (-190 - -1000)
     */
    PJ_BUILD_ERR( KErrNetUnreach,   "Could not connect to the network. Currently unreachable"),
    PJ_BUILD_ERR( KErrHostUnreach,  "Could not connect to the specified server"),
    PJ_BUILD_ERR( KErrNoProtocolOpt,"The specified server refuses the selected protocol"),
    PJ_BUILD_ERR( KErrUrgentData,   ""),
    PJ_BUILD_ERR( KErrWouldBlock,   "Conflicts with KErrExtended, but cannot occur in practice"),

    {0, NULL}
};

#endif	/* PJ_HAS_ERROR_STRING */


PJ_DEF(pj_status_t) pj_get_os_error(void)
{
    return -1;
}

PJ_DEF(void) pj_set_os_error(pj_status_t code)
{
    PJ_UNUSED_ARG(code);
}

PJ_DEF(pj_status_t) pj_get_netos_error(void)
{
    return -1;
}

PJ_DEF(void) pj_set_netos_error(pj_status_t code)
{
    PJ_UNUSED_ARG(code);
}

PJ_BEGIN_DECL

    PJ_DECL(int) platform_strerror( pj_os_err_type os_errcode, 
                       		    char *buf, pj_size_t bufsize);
PJ_END_DECL

/* 
 * platform_strerror()
 *
 * Platform specific error message. This file is called by pj_strerror() 
 * in errno.c 
 */
PJ_DEF(int) platform_strerror( pj_os_err_type os_errcode, 
			       char *buf, pj_size_t bufsize)
{
    int len = 0;

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
	len = pj_ansi_snprintf( buf, bufsize, "Symbian native error %d", 
				os_errcode);
	buf[len] = '\0';
    }

    return len;
}

