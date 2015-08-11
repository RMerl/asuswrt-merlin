/* $Id: sip_errno.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip/sip_errno.h>
#include <pjsip/sip_msg.h>
#include <pj/string.h>
#include <pj/errno.h>

/* PJSIP's own error codes/messages 
 * MUST KEEP THIS ARRAY SORTED!!
 */

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

static const struct 
{
    int code;
    const char *msg;
} err_str[] = 
{
    /* Generic SIP errors */
    PJ_BUILD_ERR( PJSIP_EBUSY,		"Object is busy" ),
    PJ_BUILD_ERR( PJSIP_ETYPEEXISTS ,	"Object with the same type exists" ),
    PJ_BUILD_ERR( PJSIP_ESHUTDOWN,	"SIP stack shutting down" ),
    PJ_BUILD_ERR( PJSIP_ENOTINITIALIZED,"SIP object is not initialized." ),
    PJ_BUILD_ERR( PJSIP_ENOROUTESET,	"Missing route set (for tel: URI)" ),

    /* Messaging errors */
    PJ_BUILD_ERR( PJSIP_EINVALIDMSG,	"Invalid message/syntax error" ),
    PJ_BUILD_ERR( PJSIP_ENOTREQUESTMSG,	"Expecting request message"),
    PJ_BUILD_ERR( PJSIP_ENOTRESPONSEMSG,"Expecting response message"),
    PJ_BUILD_ERR( PJSIP_EMSGTOOLONG,	"Message too long" ),
    PJ_BUILD_ERR( PJSIP_EPARTIALMSG,	"Partial message" ),

    PJ_BUILD_ERR( PJSIP_EINVALIDSTATUS,	"Invalid/unexpected SIP status code"),

    PJ_BUILD_ERR( PJSIP_EINVALIDURI,	"Invalid URI" ),
    PJ_BUILD_ERR( PJSIP_EINVALIDSCHEME,	"Invalid URI scheme" ),
    PJ_BUILD_ERR( PJSIP_EMISSINGREQURI,	"Missing Request-URI" ),
    PJ_BUILD_ERR( PJSIP_EINVALIDREQURI,	"Invalid Request URI" ),
    PJ_BUILD_ERR( PJSIP_EURITOOLONG,	"URI is too long" ), 

    PJ_BUILD_ERR( PJSIP_EMISSINGHDR,	"Missing required header(s)" ),
    PJ_BUILD_ERR( PJSIP_EINVALIDHDR,	"Invalid header field"),
    PJ_BUILD_ERR( PJSIP_EINVALIDVIA,	"Invalid Via header" ),
    PJ_BUILD_ERR( PJSIP_EMULTIPLEVIA,	"Multiple Via headers in response" ),

    PJ_BUILD_ERR( PJSIP_EMISSINGBODY,	"Missing message body" ),
    PJ_BUILD_ERR( PJSIP_EINVALIDMETHOD,	"Invalid/unexpected method" ),

    /* Transport errors */
    PJ_BUILD_ERR( PJSIP_EUNSUPTRANSPORT,"Unsupported transport"),
    PJ_BUILD_ERR( PJSIP_EPENDINGTX,	"Transmit buffer already pending"),
    PJ_BUILD_ERR( PJSIP_ERXOVERFLOW,	"Rx buffer overflow"),
    PJ_BUILD_ERR( PJSIP_EBUFDESTROYED,	"Buffer destroyed"),
    PJ_BUILD_ERR( PJSIP_ETPNOTSUITABLE,	"Unsuitable transport selected"),
    PJ_BUILD_ERR( PJSIP_ETPNOTAVAIL,	"Transport not available for use"),

    /* Transaction errors */
    PJ_BUILD_ERR( PJSIP_ETSXDESTROYED,	"Transaction has been destroyed"),
    PJ_BUILD_ERR( PJSIP_ENOTSX,		"No transaction is associated with the object "
					"(expecting stateful processing)" ),

    /* URI comparison status */
    PJ_BUILD_ERR( PJSIP_ECMPSCHEME,	"URI scheme mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPUSER,	"URI user part mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPPASSWD,	"URI password part mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPHOST,	"URI host part mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPPORT,	"URI port mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPTRANSPORTPRM,"URI transport param mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPTTLPARAM,	"URI ttl param mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPUSERPARAM,	"URI user param mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPMETHODPARAM,"URI method param mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPMADDRPARAM,	"URI maddr param mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPOTHERPARAM,	"URI other param mismatch" ),
    PJ_BUILD_ERR( PJSIP_ECMPHEADERPARAM,"URI header parameter mismatch" ),

    /* Authentication. */
    PJ_BUILD_ERR( PJSIP_EFAILEDCREDENTIAL, "Credential failed to authenticate"),
    PJ_BUILD_ERR( PJSIP_ENOCREDENTIAL,	   "No suitable credential"),
    PJ_BUILD_ERR( PJSIP_EINVALIDALGORITHM, "Invalid/unsupported digest algorithm" ),
    PJ_BUILD_ERR( PJSIP_EINVALIDQOP,	   "Invalid/unsupported digest qop" ),
    PJ_BUILD_ERR( PJSIP_EINVALIDAUTHSCHEME,"Unsupported authentication scheme" ),
    PJ_BUILD_ERR( PJSIP_EAUTHNOPREVCHAL,   "No previous challenge" ),
    PJ_BUILD_ERR( PJSIP_EAUTHNOAUTH,	   "No suitable authorization header" ),
    PJ_BUILD_ERR( PJSIP_EAUTHACCNOTFOUND,  "Account or credential not found" ),
    PJ_BUILD_ERR( PJSIP_EAUTHACCDISABLED,  "Account or credential is disabled" ),
    PJ_BUILD_ERR( PJSIP_EAUTHINVALIDREALM, "Invalid authorization realm"),
    PJ_BUILD_ERR( PJSIP_EAUTHINVALIDDIGEST,"Invalid authorization digest" ),
    PJ_BUILD_ERR( PJSIP_EAUTHSTALECOUNT,   "Maximum number of stale retries exceeded"),
    PJ_BUILD_ERR( PJSIP_EAUTHINNONCE,	   "Invalid nonce value in authentication challenge"),
    PJ_BUILD_ERR( PJSIP_EAUTHINAKACRED,	   "Invalid AKA credential"),
    PJ_BUILD_ERR( PJSIP_EAUTHNOCHAL,	   "No challenge is found"),

    /* UA/dialog layer. */
    PJ_BUILD_ERR( PJSIP_EMISSINGTAG,	"Missing From/To tag parameter" ),
    PJ_BUILD_ERR( PJSIP_ENOTREFER,	"Expecting REFER request") ,
    PJ_BUILD_ERR( PJSIP_ENOREFERSESSION,"Not associated with REFER subscription"),

    /* Invite session. */
    PJ_BUILD_ERR( PJSIP_ESESSIONTERMINATED, "INVITE session already terminated" ),
    PJ_BUILD_ERR( PJSIP_ESESSIONSTATE,      "Invalid INVITE session state" ),
    PJ_BUILD_ERR( PJSIP_ESESSIONINSECURE,   "Require secure session/transport"),

    /* SSL errors */
    PJ_BUILD_ERR( PJSIP_TLS_EUNKNOWN,	"Unknown TLS error" ),
    PJ_BUILD_ERR( PJSIP_TLS_EINVMETHOD,	"Invalid SSL protocol method" ),
    PJ_BUILD_ERR( PJSIP_TLS_ECACERT,	"Error loading/verifying SSL CA list file"),
    PJ_BUILD_ERR( PJSIP_TLS_ECERTFILE,	"Error loading SSL certificate chain file"),
    PJ_BUILD_ERR( PJSIP_TLS_EKEYFILE,	"Error adding private key from SSL certificate file"),
    PJ_BUILD_ERR( PJSIP_TLS_ECIPHER,	"Error setting SSL cipher list"),
    PJ_BUILD_ERR( PJSIP_TLS_ECTX,	"Error creating SSL context"),
    PJ_BUILD_ERR( PJSIP_TLS_ESSLCONN,	"Error creating SSL connection object"),
    PJ_BUILD_ERR( PJSIP_TLS_ECONNECT,	"Unknown error when performing SSL connect()"),
    PJ_BUILD_ERR( PJSIP_TLS_EACCEPT,	"Unknown error when performing SSL accept()"),
    PJ_BUILD_ERR( PJSIP_TLS_ESEND,	"Unknown error when sending SSL data"),
    PJ_BUILD_ERR( PJSIP_TLS_EREAD,	"Unknown error when reading SSL data"),
    PJ_BUILD_ERR( PJSIP_TLS_ETIMEDOUT,	"SSL negotiation has timed out"),
    PJ_BUILD_ERR( PJSIP_TLS_ECERTVERIF,	"SSL certificate verification error"),
};


#endif	/* PJ_HAS_ERROR_STRING */


/*
 * pjsip_strerror()
 */
PJ_DEF(pj_str_t) pjsip_strerror( pj_status_t statcode, 
				 char *buf, pj_size_t bufsize )
{
    pj_str_t errstr;

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

    if (statcode >= PJSIP_ERRNO_START && statcode < PJSIP_ERRNO_START+800) 
    {
	/* Status code. */
	const pj_str_t *status_text = 
	    pjsip_get_status_text(PJSIP_ERRNO_TO_SIP_STATUS(statcode));

	errstr.ptr = buf;
	pj_strncpy_with_null(&errstr, status_text, bufsize);
	return errstr;
    }
    else if (statcode >= PJSIP_ERRNO_START_PJSIP && 
	     statcode < PJSIP_ERRNO_START_PJSIP + 1000)
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
				   "Unknown pjsip error %d",
				   statcode);

    return errstr;

}

