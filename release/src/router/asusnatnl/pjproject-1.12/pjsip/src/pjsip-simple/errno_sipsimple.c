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
#include <pjsip-simple/errno.h>
#include <pj/string.h>

/* PJSIP-SIMPLE's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 * Message must be limited to 64 chars!
 */

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    /* Event errors */
    { PJSIP_SIMPLE_ENOPKG,	    "No SIP event package with the specified name" },
    { PJSIP_SIMPLE_EPKGEXISTS,	    "SIP event package already exist" },

    /* Presence errors */
    { PJSIP_SIMPLE_ENOTSUBSCRIBE,   "Expecting SUBSCRIBE request" },
    { PJSIP_SIMPLE_ENOPRESENCE,	    "No presence associated with the subscription" },
    { PJSIP_SIMPLE_ENOPRESENCEINFO, "No presence info in the server subscription" },
    { PJSIP_SIMPLE_EBADCONTENT,	    "Bad Content-Type for presence" },
    { PJSIP_SIMPLE_EBADPIDF,	    "Bad PIDF content for presence" },
    { PJSIP_SIMPLE_EBADXPIDF,	    "Bad XPIDF content for presence" },
    { PJSIP_SIMPLE_EBADRPID,	    "Invalid or bad RPID document"},

    /* isComposing errors. */
    { PJSIP_SIMPLE_EBADISCOMPOSE,   "Bad isComposing indication/XML message" },
};


#endif	/* PJ_HAS_ERROR_STRING */


/*
 * pjsipsimple_strerror()
 */
PJ_DEF(pj_str_t) pjsipsimple_strerror( pj_status_t statcode, 
				       char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

    if (statcode >= PJSIP_SIMPLE_ERRNO_START && 
	statcode < PJSIP_SIMPLE_ERRNO_START + PJ_ERRNO_SPACE_SIZE)
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
				   "Unknown pjsip-simple error %d",
 				   statcode);

    return errstr;
}

