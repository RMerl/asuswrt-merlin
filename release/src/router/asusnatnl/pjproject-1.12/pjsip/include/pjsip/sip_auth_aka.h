/* $Id: sip_auth_aka.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_AUTH_SIP_AUTH_AKA_H__
#define __PJSIP_AUTH_SIP_AUTH_AKA_H__

/**
 * @file sip_auth_aka.h
 * @brief SIP Digest AKA Authorization Module.
 */

#include <pjsip/sip_auth.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_AUTH_AKA_API Digest AKAv1 and AKAv2 Authentication API
 * @ingroup PJSIP_AUTH_API
 * @brief Digest AKAv1 and AKAv2 Authentication API
 * @{
 *
 * This module implements HTTP digest authentication using Authentication
 * and Key Agreement (AKA) version 1 and version 2 (AKAv1-MD5 and AKAv2-MD5),
 * as specified in RFC 3310 and RFC 4169. SIP AKA authentication is used
 * by 3GPP and IMS systems.
 *
 * @section pjsip_aka_using Using Digest AKA Authentication
 *
 * Support for digest AKA authentication is currently made optional, so
 * application needs to declare \a PJSIP_HAS_DIGEST_AKA_AUTH to non-zero
 * in <tt>config_site.h</tt> to enable AKA support:
 *
 @code
   #define PJSIP_HAS_DIGEST_AKA_AUTH   1
 @endcode

 *
 * In addition, application would need to link with <b>libmilenage</b>
 * library from \a third_party directory.
 *
 * Application then specifies digest AKA credential by initializing the 
 * authentication credential as follows:
 *
 @code

    pjsip_cred_info cred;

    pj_bzero(&cred, sizeof(cred));

    cred.scheme = pj_str("Digest");
    cred.realm = pj_str("ims-domain.test");
    cred.username = pj_str("user@ims-domain.test");
    cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD | PJSIP_CRED_DATA_EXT_AKA;
    cred.data = pj_str("password");

    // AKA extended info
    cred.ext.aka.k = pj_str("password");
    cred.ext.aka.cb = &pjsip_auth_create_aka_response

 @endcode
 *
 * Description:
 * - To support AKA, application adds \a PJSIP_CRED_DATA_EXT_AKA flag in the
 * \a data_type field. This indicates that extended information specific to
 * AKA authentication is available in the credential, and that response 
 * digest computation will use the callback function instead of the usual MD5
 * digest computation.
 *
 * - The \a scheme for the credential is "Digest". 
 *
 * - The \a realm is the expected realm in the challenge. Application may 
 * also specify wildcard realm ("*") if it wishes to respond to any realms 
 * in the challenge.
 *
 * - The \a data field is optional. Application may fill this with the password
 * if it wants to support both MD5 and AKA MD5 in a single credential. The
 * pjsip_auth_create_aka_response() function will use this field if the
 * challenge indicates "MD5" as the algorithm instead of "AKAv1-MD5" or
 * "AKAv2-MD5".
 *
 * - The \a ext.aka.k field specifies the permanent subscriber key to be used
 * for AKA authentication. Application may specify binary password containing
 * NULL character in this key, since the length of the key is indicated in
 * the \a slen field of the string.
 *
 * - The \a ext.aka.cb field specifies the callback function to calculate the
 * response digest. Application can specify pjsip_auth_create_aka_response()
 * in this field to use PJSIP's implementation, but it's free to provide
 * it's own function.
 *
 * - Optionally application may set \a ext.aka.op and \a ext.aka.amf in the
 * credential to specify AKA Operator variant key and AKA Authentication 
 * Management Field information.
 */

/**
 * Length of Authentication Key (AK) in bytes.
 */
#define PJSIP_AKA_AKLEN		6

/**
 * Length of Authentication Management Field (AMF) in bytes.
 */
#define PJSIP_AKA_AMFLEN	2

/**
 * Length of AUTN in bytes.
 */
#define PJSIP_AKA_AUTNLEN	16

/**
 * Length of Confidentiality Key (CK) in bytes.
 */
#define PJSIP_AKA_CKLEN		16

/**
 * Length of Integrity Key (AK) in bytes.
 */
#define PJSIP_AKA_IKLEN		16

/**
 * Length of permanent/subscriber Key (K) in bytes.
 */
#define PJSIP_AKA_KLEN		16

/**
 * Length of AKA authentication code in bytes.
 */
#define PJSIP_AKA_MACLEN	8

/**
 * Length of operator key in bytes.
 */
#define PJSIP_AKA_OPLEN		16

/**
 * Length of random challenge (RAND) in bytes.
 */
#define PJSIP_AKA_RANDLEN	16

/**
 * Length of response digest in bytes.
 */
#define PJSIP_AKA_RESLEN	8

/**
 * Length of sequence number (SQN) in bytes.
 */
#define PJSIP_AKA_SQNLEN	6

/**
 * This function creates MD5, AKAv1-MD5, or AKAv2-MD5 response for
 * the specified challenge in \a chal, according to the algorithm 
 * specified in the challenge, and based on the information in the 
 * credential \a cred.
 *
 * Application may register this function as \a ext.aka.cb field of
 * #pjsip_cred_info structure to make PJSIP automatically call this
 * function to calculate the response digest. To do so, it needs to
 * add \a PJSIP_CRED_DATA_EXT_AKA flag in the \a data_type field of
 * the credential, and fills up other AKA specific information in
 * the credential.
 *
 * @param pool	    Pool to allocate memory.
 * @param chal	    The authentication challenge sent by server in 401
 *		    or 401 response, as either Proxy-Authenticate or
 *		    WWW-Authenticate header.
 * @param cred	    The credential to be used.
 * @param method    The request method.
 * @param auth	    The digest credential where the digest response
 *		    will be placed to. Upon calling this function, the
 *		    nonce, nc, cnonce, qop, uri, and realm fields of
 *		    this structure must have been set by caller. Upon
 *		    return, the \a response field will be initialized
 *		    by this function.
 *
 * @return	    PJ_SUCCESS if response has been created successfully.
 */
PJ_DECL(pj_status_t) pjsip_auth_create_aka_response(
					     pj_pool_t *pool,
					     const pjsip_digest_challenge*chal,
					     const pjsip_cred_info *cred,
					     const pj_str_t *method,
					     pjsip_digest_credential *auth);


/**
 * @}
 */



PJ_END_DECL


#endif	/* __PJSIP_AUTH_SIP_AUTH_AKA_H__ */

