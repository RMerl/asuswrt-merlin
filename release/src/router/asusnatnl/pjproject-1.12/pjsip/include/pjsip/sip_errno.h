/* $Id: sip_errno.h 3553 2011-05-05 06:14:19Z nanang $  */
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
#ifndef __PJSIP_SIP_ERRNO_H__
#define __PJSIP_SIP_ERRNO_H__

/**
 * @file sip_errno.h
 * @brief PJSIP Specific Error Code
 */

#include <pj/errno.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_CORE_ERRNO PJSIP Specific Error Code
 * @ingroup PJSIP_BASE
 * @brief PJSIP specific error constants.
 * @{
 */

/*
 * PJSIP error codes occupies 170000 - 219000, and mapped as follows:
 *  - 170100 - 170799: mapped to SIP status code in response msg.
 *  - 171000 - 171999: mapped to errors generated from PJSIP core.
 */

/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 */
#define PJSIP_ERRNO_START       (PJ_ERRNO_START_USER)

/**
 * Create error value from SIP status code.
 * @param code      SIP status code.
 * @return          Error code in pj_status_t namespace.
 */
#define PJSIP_ERRNO_FROM_SIP_STATUS(code)   (PJSIP_ERRNO_START+code)

/**
 * Get SIP status code from error value.
 * If conversion to SIP status code is not available, a SIP status code
 * 599 will be returned.
 *
 * @param status    Error code in pj_status_t namespace.
 * @return          SIP status code.
 */
#define PJSIP_ERRNO_TO_SIP_STATUS(status)               \
         ((status>=PJSIP_ERRNO_FROM_SIP_STATUS(100) &&  \
           status<PJSIP_ERRNO_FROM_SIP_STATUS(800)) ?   \
          status-PJSIP_ERRNO_FROM_SIP_STATUS(0) : 599)


/**
 * Start of PJSIP generated error code values.
 */
#define PJSIP_ERRNO_START_PJSIP (PJSIP_ERRNO_START + 1000)

/************************************************************
 * GENERIC/GENERAL SIP ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * SIP object is busy.
 */
#define PJSIP_EBUSY		(PJSIP_ERRNO_START_PJSIP + 1)	/* 171001 */
/**
 * @hideinitializer
 * SIP object with the same type already exists.
 */
#define PJSIP_ETYPEEXISTS	(PJSIP_ERRNO_START_PJSIP + 2)	/* 171002 */
/**
 * @hideinitializer
 * SIP stack is shutting down.
 */
#define PJSIP_ESHUTDOWN		(PJSIP_ERRNO_START_PJSIP + 3)	/* 171003 */
/**
 * @hideinitializer
 * SIP object is not initialized.
 */
#define PJSIP_ENOTINITIALIZED	(PJSIP_ERRNO_START_PJSIP + 4)	/* 171004 */
/**
 * @hideinitializer
 * Missing route set (for tel: URI)
 */
#define PJSIP_ENOROUTESET	(PJSIP_ERRNO_START_PJSIP + 5)	/* 171005 */


/************************************************************
 * MESSAGING ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * General invalid message error (e.g. syntax error)
 */
#define PJSIP_EINVALIDMSG       (PJSIP_ERRNO_START_PJSIP + 20)	/* 171020 */
/**
 * @hideinitializer
 * Expecting request message.
 */
#define PJSIP_ENOTREQUESTMSG	(PJSIP_ERRNO_START_PJSIP + 21)	/* 171021 */
/**
 * @hideinitializer
 * Expecting response message.
 */
#define PJSIP_ENOTRESPONSEMSG	(PJSIP_ERRNO_START_PJSIP + 22)	/* 171022 */
/**
 * @hideinitializer
 * Message too long. See also PJSIP_ERXOVERFLOW.
 */
#define PJSIP_EMSGTOOLONG	(PJSIP_ERRNO_START_PJSIP + 23)	/* 171023 */
/**
 * @hideinitializer
 * Message not completely received.
 */
#define PJSIP_EPARTIALMSG       (PJSIP_ERRNO_START_PJSIP + 24)	/* 171024 */

/**
 * @hideinitializer
 * Status code is invalid.
 */
#define PJSIP_EINVALIDSTATUS	(PJSIP_ERRNO_START_PJSIP + 30)	/* 171030 */

/**
 * @hideinitializer
 * General Invalid URI error.
 */
#define PJSIP_EINVALIDURI	(PJSIP_ERRNO_START_PJSIP + 39)	/* 171039 */
/**
 * @hideinitializer
 * Unsupported URL scheme.
 */
#define PJSIP_EINVALIDSCHEME    (PJSIP_ERRNO_START_PJSIP + 40)	/* 171040 */
/**
 * @hideinitializer
 * Missing Request-URI.
 */
#define PJSIP_EMISSINGREQURI    (PJSIP_ERRNO_START_PJSIP + 41)	/* 171041 */
/**
 * @hideinitializer
 * Invalid request URI.
 */
#define PJSIP_EINVALIDREQURI	(PJSIP_ERRNO_START_PJSIP + 42)	/* 171042 */
/**
 * @hideinitializer
 * URI is too long.
 */
#define PJSIP_EURITOOLONG	(PJSIP_ERRNO_START_PJSIP + 43)	/* 171043 */

/**
 * @hideinitializer
 * Missing required header(s).
 */
#define PJSIP_EMISSINGHDR       (PJSIP_ERRNO_START_PJSIP + 50)	/* 171050 */
/**
 * @hideinitializer
 * Invalid header field.
 */
#define PJSIP_EINVALIDHDR	(PJSIP_ERRNO_START_PJSIP + 51)	/* 171051 */
/**
 * @hideinitializer
 * Invalid Via header in response (sent-by, etc).
 */
#define PJSIP_EINVALIDVIA	(PJSIP_ERRNO_START_PJSIP + 52)	/* 171052 */
/**
 * @hideinitializer
 * Multiple Via headers in response.
 */
#define PJSIP_EMULTIPLEVIA	(PJSIP_ERRNO_START_PJSIP + 53)	/* 171053 */
/**
 * @hideinitializer
 * Missing message body.
 */
#define PJSIP_EMISSINGBODY	(PJSIP_ERRNO_START_PJSIP + 54)	/* 171054 */
/**
 * @hideinitializer
 * Invalid/unexpected method.
 */
#define PJSIP_EINVALIDMETHOD	(PJSIP_ERRNO_START_PJSIP + 55)	/* 171055 */


/************************************************************
 * TRANSPORT ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Unsupported transport type.
 */
#define PJSIP_EUNSUPTRANSPORT	(PJSIP_ERRNO_START_PJSIP + 60)	/* 171060 */
/**
 * @hideinitializer
 * Buffer is being sent, operation still pending.
 */
#define PJSIP_EPENDINGTX	(PJSIP_ERRNO_START_PJSIP + 61)	/* 171061 */
/**
 * @hideinitializer
 * Rx buffer overflow. See also PJSIP_EMSGTOOLONG.
 */
#define PJSIP_ERXOVERFLOW       (PJSIP_ERRNO_START_PJSIP + 62)	/* 171062 */
/**
 * @hideinitializer
 * This is not really an error, it just informs application that
 * transmit data has been deleted on return of pjsip_tx_data_dec_ref().
 */
#define PJSIP_EBUFDESTROYED     (PJSIP_ERRNO_START_PJSIP + 63)	/* 171063 */
/**
 * @hideinitializer
 * Unsuitable transport selected. This error occurs when application
 * has explicitly requested to use a particular transport/listener,
 * but the selected transport is not suitable to send request to
 * the specified destination.
 */
#define PJSIP_ETPNOTSUITABLE	(PJSIP_ERRNO_START_PJSIP + 64)	/* 171064 */
/**
 * @hideinitializer
 * Transport not available. This error occurs for example when the SIP stack 
 * is trying to use a SIP transport while the transport is being paused by 
 * application.
 */
#define PJSIP_ETPNOTAVAIL	(PJSIP_ERRNO_START_PJSIP + 65)	/* 171065 */

/************************************************************
 * TRANSACTION ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Transaction has just been destroyed.
 */
#define PJSIP_ETSXDESTROYED     (PJSIP_ERRNO_START_PJSIP + 70)	/* 171070 */
/**
 * @hideinitializer
 * No transaction.
 */
#define PJSIP_ENOTSX		(PJSIP_ERRNO_START_PJSIP + 71)	/* 171071 */


/************************************************************
 * URI COMPARISON RESULTS
 ***********************************************************/
/**
 * @hideinitializer
 * Scheme mismatch.
 */
#define PJSIP_ECMPSCHEME	(PJSIP_ERRNO_START_PJSIP + 80)	/* 171080 */
/**
 * @hideinitializer
 * User part mismatch.
 */
#define PJSIP_ECMPUSER		(PJSIP_ERRNO_START_PJSIP + 81)	/* 171081 */
/**
 * @hideinitializer
 * Password part mismatch.
 */
#define PJSIP_ECMPPASSWD	(PJSIP_ERRNO_START_PJSIP + 82)	/* 171082 */
/**
 * @hideinitializer
 * Host part mismatch.
 */
#define PJSIP_ECMPHOST		(PJSIP_ERRNO_START_PJSIP + 83)	/* 171083 */
/**
 * @hideinitializer
 * Port part mismatch.
 */
#define PJSIP_ECMPPORT		(PJSIP_ERRNO_START_PJSIP + 84)	/* 171084 */
/**
 * @hideinitializer
 * Transport parameter part mismatch.
 */
#define PJSIP_ECMPTRANSPORTPRM	(PJSIP_ERRNO_START_PJSIP + 85)	/* 171085 */
/**
 * @hideinitializer
 * TTL parameter part mismatch.
 */
#define PJSIP_ECMPTTLPARAM	(PJSIP_ERRNO_START_PJSIP + 86)	/* 171086 */
/**
 * @hideinitializer
 * User parameter part mismatch.
 */
#define PJSIP_ECMPUSERPARAM	(PJSIP_ERRNO_START_PJSIP + 87)	/* 171087 */
/**
 * @hideinitializer
 * Method parameter part mismatch.
 */
#define PJSIP_ECMPMETHODPARAM	(PJSIP_ERRNO_START_PJSIP + 88)	/* 171088 */
/**
 * @hideinitializer
 * Maddr parameter part mismatch.
 */
#define PJSIP_ECMPMADDRPARAM	(PJSIP_ERRNO_START_PJSIP + 89)	/* 171089 */
/**
 * @hideinitializer
 * Parameter part in other_param mismatch.
 */
#define PJSIP_ECMPOTHERPARAM	(PJSIP_ERRNO_START_PJSIP + 90)	/* 171090 */
/**
 * @hideinitializer
 * Parameter part in header_param mismatch.
 */
#define PJSIP_ECMPHEADERPARAM	(PJSIP_ERRNO_START_PJSIP + 91)	/* 171091 */


/************************************************************
 * AUTHENTICATION FRAMEWORK
 ***********************************************************/
/**
 * @hideinitializer
 * Credential failed to authenticate. For this failure, right credential
 * for the realm has been found and used to authenticate against the challenge,
 * but the server has rejected the authorization request with 401/407 response
 * (either with no stale parameter or with "stale=false" parameter). In most
 * cases, this indicates that the username/password combination is incorrect.
 */
#define PJSIP_EFAILEDCREDENTIAL	(PJSIP_ERRNO_START_PJSIP + 100)	/* 171100 */
/**
 * @hideinitializer
 * No suitable credential is found to authenticate the request against 
 * the received authentication challenge in 401/407 response. This often
 * is caused by different realm supplied in the credential than the realm
 * found in the challenge.
 */
#define PJSIP_ENOCREDENTIAL	(PJSIP_ERRNO_START_PJSIP + 101)	/* 171101 */
/**
 * @hideinitializer
 * Invalid/unsupported algorithm.
 */
#define PJSIP_EINVALIDALGORITHM	(PJSIP_ERRNO_START_PJSIP + 102)	/* 171102 */
/**
 * @hideinitializer
 * Invalid/unsupported qop.
 */
#define PJSIP_EINVALIDQOP	(PJSIP_ERRNO_START_PJSIP + 103)	/* 171103 */
/**
 * @hideinitializer
 * Invalid/unsupported authentication scheme.
 */
#define PJSIP_EINVALIDAUTHSCHEME (PJSIP_ERRNO_START_PJSIP + 104)/* 171104 */
/**
 * @hideinitializer
 * No previous challenge.
 */
#define PJSIP_EAUTHNOPREVCHAL	(PJSIP_ERRNO_START_PJSIP + 105)	/* 171105 */
/**
 * @hideinitializer
 * No authorization is found.
 */
#define PJSIP_EAUTHNOAUTH	(PJSIP_ERRNO_START_PJSIP + 106)	/* 171106 */
/**
 * @hideinitializer
 * Account not found.
 */
#define PJSIP_EAUTHACCNOTFOUND	(PJSIP_ERRNO_START_PJSIP + 107)	/* 171107 */
/**
 * @hideinitializer
 * Account is disabled.
 */
#define PJSIP_EAUTHACCDISABLED	(PJSIP_ERRNO_START_PJSIP + 108)	/* 171108 */
/**
 * @hideinitializer
 * Invalid realm.
 */
#define PJSIP_EAUTHINVALIDREALM	(PJSIP_ERRNO_START_PJSIP + 109)	/* 171109 */
/**
 * @hideinitializer
 * Invalid digest.
 */
#define PJSIP_EAUTHINVALIDDIGEST (PJSIP_ERRNO_START_PJSIP+110)	/* 171110 */
/**
 * @hideinitializer
 * Maximum number of stale retries exceeded. This happens when server 
 * keeps rejecting our authorization request with stale=true.
 */
#define PJSIP_EAUTHSTALECOUNT	(PJSIP_ERRNO_START_PJSIP + 111)	/* 171111 */
/**
 * @hideinitializer
 * Invalid nonce value in the challenge.
 */
#define PJSIP_EAUTHINNONCE	(PJSIP_ERRNO_START_PJSIP + 112)	/* 171112 */
/**
 * @hideinitializer
 * Invalid AKA credential.
 */
#define PJSIP_EAUTHINAKACRED	(PJSIP_ERRNO_START_PJSIP + 113)	/* 171113 */
/**
 * No challenge is found in the challenge.
 */
#define PJSIP_EAUTHNOCHAL	(PJSIP_ERRNO_START_PJSIP + 114)	/* 171114 */

/************************************************************
 * UA AND DIALOG ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Missing From/To tag.
 */
#define PJSIP_EMISSINGTAG	 (PJSIP_ERRNO_START_PJSIP+120)	/* 171120 */
/**
 * @hideinitializer
 * Expecting REFER method
 */
#define PJSIP_ENOTREFER		 (PJSIP_ERRNO_START_PJSIP+121)	/* 171121 */
/**
 * @hideinitializer
 * Not associated with REFER subscription
 */
#define PJSIP_ENOREFERSESSION	 (PJSIP_ERRNO_START_PJSIP+122)	/* 171122 */

/************************************************************
 * INVITE SESSIONS ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Session already terminated.
 */
#define PJSIP_ESESSIONTERMINATED (PJSIP_ERRNO_START_PJSIP+140)	/* 171140 */
/**
 * @hideinitializer
 * Invalid session state for the specified operation.
 */
#define PJSIP_ESESSIONSTATE	 (PJSIP_ERRNO_START_PJSIP+141)	/* 171141 */
/**
 * @hideinitializer
 * The feature being requested requires the use of secure session or 
 * transport.
 */
#define PJSIP_ESESSIONINSECURE	(PJSIP_ERRNO_START_PJSIP+142)	/* 171142 */

/************************************************************
 * TLS TRANSPORT ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Unknown TLS error
 */
#define PJSIP_TLS_EUNKNOWN	(PJSIP_ERRNO_START_PJSIP+160)	/* 171160 */
/**
 * @hideinitializer
 * Invalid SSL protocol method.
 */
#define PJSIP_TLS_EINVMETHOD	(PJSIP_ERRNO_START_PJSIP+161)	/* 171161 */
/**
 * @hideinitializer
 * Error loading/verifying SSL CA list file.
 */
#define PJSIP_TLS_ECACERT	(PJSIP_ERRNO_START_PJSIP+162)	/* 171162 */
/**
 * @hideinitializer
 * Error loading SSL certificate chain file.
 */
#define PJSIP_TLS_ECERTFILE	(PJSIP_ERRNO_START_PJSIP+163)	/* 171163 */
/**
 * @hideinitializer
 * Error adding private key from SSL certificate file.
 */
#define PJSIP_TLS_EKEYFILE	(PJSIP_ERRNO_START_PJSIP+164)	/* 171164 */
/**
 * @hideinitializer
 * Error setting SSL cipher list.
 */
#define PJSIP_TLS_ECIPHER	(PJSIP_ERRNO_START_PJSIP+165)	/* 171165 */
/**
 * @hideinitializer
 * Error creating SSL context.
 */
#define PJSIP_TLS_ECTX		(PJSIP_ERRNO_START_PJSIP+166)	/* 171166 */
/**
 * @hideinitializer
 * Error creating SSL connection object.
 */
#define PJSIP_TLS_ESSLCONN	(PJSIP_ERRNO_START_PJSIP+167)	/* 171167 */
/**
 * @hideinitializer
 * Unknown error when performing SSL connect().
 */
#define PJSIP_TLS_ECONNECT	(PJSIP_ERRNO_START_PJSIP+168)	/* 171168 */
/**
 * @hideinitializer
 * Unknown error when performing SSL accept().
 */
#define PJSIP_TLS_EACCEPT	(PJSIP_ERRNO_START_PJSIP+169)	/* 171169 */
/**
 * @hideinitializer
 * Unknown error when sending SSL data
 */
#define PJSIP_TLS_ESEND		(PJSIP_ERRNO_START_PJSIP+170)	/* 171170 */
/**
 * @hideinitializer
 * Unknown error when reading SSL data
 */
#define PJSIP_TLS_EREAD		(PJSIP_ERRNO_START_PJSIP+171)	/* 171171 */
/**
 * @hideinitializer
 * SSL negotiation has exceeded the maximum configured timeout.
 */
#define PJSIP_TLS_ETIMEDOUT	(PJSIP_ERRNO_START_PJSIP+172)	/* 171172 */
/**
 * @hideinitializer
 * SSL certificate verification error.
 */
#define PJSIP_TLS_ECERTVERIF	(PJSIP_ERRNO_START_PJSIP+173)	/* 171173 */


/**
 * Get error message for the specified error code. Note that this
 * function is only able to decode PJSIP specific error code.
 * Application should use pj_strerror(), which should be able to
 * decode all error codes belonging to all subsystems (e.g. pjlib,
 * pjmedia, pjsip, etc).
 *
 * @param status    The error code.
 * @param buffer    The buffer where to put the error message.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with pj_str_t.
 */
PJ_DECL(pj_str_t) pjsip_strerror(pj_status_t status, 
				 char *buffer, pj_size_t bufsize);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJSIP_SIP_ERRNO_H__ */

