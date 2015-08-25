/* $Id: errno.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjnath/errno.h>
#include <pjnath/stun_msg.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/string.h>



/* PJNATH's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 * Message must be limited to 64 chars!
 */
#if defined(PJ_HAS_ERROR_STRING) && PJ_HAS_ERROR_STRING!=0
static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    /* STUN related error codes */
    PJ_BUILD_ERR( PJNATH_EINSTUNMSG,	    "Invalid STUN message"),
    PJ_BUILD_ERR( PJNATH_EINSTUNMSGLEN,	    "Invalid STUN message length"),
    PJ_BUILD_ERR( PJNATH_EINSTUNMSGTYPE,    "Invalid or unexpected STUN message type"),
    PJ_BUILD_ERR( PJNATH_ESTUNTIMEDOUT,	    "STUN transaction has timed out"),

    PJ_BUILD_ERR( PJNATH_ESTUNTOOMANYATTR,  "Too many STUN attributes"),
    PJ_BUILD_ERR( PJNATH_ESTUNINATTRLEN,    "Invalid STUN attribute length"),
    PJ_BUILD_ERR( PJNATH_ESTUNDUPATTR,	    "Found duplicate STUN attribute"),

    PJ_BUILD_ERR( PJNATH_ESTUNFINGERPRINT,  "STUN FINGERPRINT verification failed"),
    PJ_BUILD_ERR( PJNATH_ESTUNMSGINTPOS,    "Invalid STUN attribute after MESSAGE-INTEGRITY"),
    PJ_BUILD_ERR( PJNATH_ESTUNFINGERPOS,    "Invalid STUN attribute after FINGERPRINT"),

    PJ_BUILD_ERR( PJNATH_ESTUNNOMAPPEDADDR, "STUN (XOR-)MAPPED-ADDRESS attribute not found"),
    PJ_BUILD_ERR( PJNATH_ESTUNIPV6NOTSUPP,  "STUN IPv6 attribute not supported"),
    PJ_BUILD_ERR( PJNATH_EINVAF,	    "Invalid STUN address family value"),
    PJ_BUILD_ERR( PJNATH_ESTUNINSERVER,	    "Invalid STUN server or server not configured"),

    PJ_BUILD_ERR( PJNATH_ESTUNDESTROYED,    "STUN object has been destoyed"),

    /* ICE related errors */
    PJ_BUILD_ERR( PJNATH_ENOICE,	    "ICE session not available"),
    PJ_BUILD_ERR( PJNATH_EICEINPROGRESS,    "ICE check is in progress"),
    PJ_BUILD_ERR( PJNATH_EICEFAILED,	    "All ICE checklists failed"),
    PJ_BUILD_ERR( PJNATH_EICEMISMATCH,	    "Default target doesn't match any ICE candidates"),
    PJ_BUILD_ERR( PJNATH_EICEINCOMPID,	    "Invalid ICE component ID"),
    PJ_BUILD_ERR( PJNATH_EICEINCANDID,	    "Invalid ICE candidate ID"),
    PJ_BUILD_ERR( PJNATH_EICEINSRCADDR,	    "Source address mismatch"),
    PJ_BUILD_ERR( PJNATH_EICEMISSINGSDP,    "Missing ICE SDP attribute"),
    PJ_BUILD_ERR( PJNATH_EICEINCANDSDP,	    "Invalid SDP \"candidate\" attribute"),
    PJ_BUILD_ERR( PJNATH_EICENOHOSTCAND,    "No host candidate associated with srflx"),
    PJ_BUILD_ERR( PJNATH_EICENOMTIMEOUT,    "Controlled agent timed out waiting for nomination"),

    /* TURN related errors */
    PJ_BUILD_ERR( PJNATH_ETURNINTP,	    "Invalid/unsupported transport"),

};
#endif	/* PJ_HAS_ERROR_STRING */


/*
 * pjnath_strerror()
 */
static pj_str_t pjnath_strerror(pj_status_t statcode, 
				char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

    if (statcode >= PJNATH_ERRNO_START && 
	statcode < PJNATH_ERRNO_START + PJ_ERRNO_SPACE_SIZE)
    {
	/* Find the error in the table.
	 * Use binary search!
	 */
	int first = 0;
	int n = PJ_ARRAY_SIZE(err_str);

	while (n > 0) {
	    int half = n/2;
	    int mid = first + half;

	    if (err_str[mid].code < statcode) {
		first = mid+1;
		n -= (half+1);
	    } else if (err_str[mid].code > statcode) {
		n = half;
	    } else {
		first = mid;
		break;
	    }
	}


	if (PJ_ARRAY_SIZE(err_str) && err_str[first].code == statcode) {
	    pj_str_t msg;
	    
	    msg.ptr = (char*)err_str[first].msg;
	    msg.slen = pj_ansi_strlen(err_str[first].msg);

	    errstr.ptr = buf;
	    pj_strncpy_with_null(&errstr, &msg, bufsize);
	    return errstr;

	} 
    }

#endif	/* PJ_HAS_ERROR_STRING */


    /* Error not found. */
    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				   "Unknown pjnath error %d",
				   statcode);
    if (errstr.slen < 0) errstr.slen = 0;
    else if (errstr.slen > (int)bufsize) errstr.slen = bufsize;

    return errstr;
}


static pj_str_t pjnath_strerror2(pj_status_t statcode, 
				 char *buf, pj_size_t bufsize )
{
    int stun_code = statcode - PJ_STATUS_FROM_STUN_CODE(0);
    const pj_str_t cmsg = pj_stun_get_err_reason(stun_code);
    pj_str_t errstr;

    buf[bufsize-1] = '\0';

    if (cmsg.slen == 0) {
	/* Not found */
	errstr.ptr = buf;
	errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				       "Unknown STUN err-code %d",
				       stun_code);
    } else {
	errstr.ptr = buf;
	pj_strncpy(&errstr, &cmsg, bufsize);
	if (errstr.slen < (int)bufsize)
	    buf[errstr.slen] = '\0';
	else
	    buf[bufsize-1] = '\0';
    }

    if (errstr.slen < 0) errstr.slen = 0;
    else if (errstr.slen > (int)bufsize) errstr.slen = bufsize;

    return errstr;
}


PJ_DEF(pj_status_t) pjnath_init(void)
{
    pj_status_t status;

    status = pj_register_strerror(PJNATH_ERRNO_START, 299, 
				  &pjnath_strerror);
    pj_assert(status == PJ_SUCCESS);

    status = pj_register_strerror(PJ_STATUS_FROM_STUN_CODE(300), 
				  699 - 300, 
				  &pjnath_strerror2);
    pj_assert(status == PJ_SUCCESS);

    return PJ_SUCCESS;
}


#if PJNATH_ERROR_LEVEL <= PJ_LOG_MAX_LEVEL

PJ_DEF(void) pjnath_perror(const char *sender, const char *title,
			   pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

#if PJNATH_ERROR_LEVEL==1
    PJ_LOG(1,(sender, "%s: %s", title, errmsg));
#elif PJNATH_ERROR_LEVEL==2
    PJ_LOG(2,(sender, "%s: %s", title, errmsg));
#elif PJNATH_ERROR_LEVEL==3
    PJ_LOG(3,(sender, "%s: %s", title, errmsg));
#elif PJNATH_ERROR_LEVEL==4
    PJ_LOG(4,(sender, "%s: %s", title, errmsg));
#elif PJNATH_ERROR_LEVEL==5
    PJ_LOG(5,(sender, "%s: %s", title, errmsg));
#else
# error Invalid PJNATH_ERROR_LEVEL value
#endif
}

#endif	/* PJNATH_ERROR_LEVEL <= PJ_LOG_MAX_LEVEL */

