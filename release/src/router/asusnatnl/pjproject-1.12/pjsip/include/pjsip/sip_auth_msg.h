/* $Id: sip_auth_msg.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_AUTH_SIP_AUTH_MSG_H__
#define __PJSIP_AUTH_SIP_AUTH_MSG_H__

#include <pjsip/sip_msg.h>

PJ_BEGIN_DECL

/**
 * @addtogroup PJSIP_MSG_HDR
 * @{
 */

/**
 * Common credential structure represents common credential fields
 * present in Authorization/Proxy-Authorization header.
 */
struct pjsip_common_credential
{
    pj_str_t	realm;		/**< Credential's realm.    */
    pjsip_param	other_param;	/**< Other parameters.	    */
};

/**
 * @see pjsip_common_credential
 */
typedef struct pjsip_common_credential pjsip_common_credential;


/**
 * This structure describe credential used in Authorization and
 * Proxy-Authorization header for digest authentication scheme.
 */
struct pjsip_digest_credential
{
    pj_str_t	realm;		/**< Realm of the credential	*/
    pjsip_param	other_param;	/**< Other parameters.		*/
    pj_str_t	username;	/**< Username parameter.	*/
    pj_str_t	nonce;		/**< Nonce parameter.		*/
    pj_str_t	uri;		/**< URI parameter.		*/ 
    pj_str_t	response;	/**< Response digest.		*/
    pj_str_t	algorithm;	/**< Algorithm.			*/
    pj_str_t	cnonce;		/**< Cnonce.			*/
    pj_str_t	opaque;		/**< Opaque value.		*/
    pj_str_t	qop;		/**< Quality of protection.	*/
    pj_str_t	nc;		/**< Nonce count.		*/
};

/**
 * @see pjsip_digest_credential
 */
typedef struct pjsip_digest_credential pjsip_digest_credential;

/**
 * This structure describe credential used in Authorization and
 * Proxy-Authorization header for PGP authentication scheme.
 */
struct pjsip_pgp_credential
{
    pj_str_t	realm;		/**< Realm.			*/
    pjsip_param	other_param;	/**< Other parameters.		*/
    pj_str_t	version;	/**< Version parameter.		*/
    pj_str_t	signature;	/**< Signature parameter.	*/
    pj_str_t	signed_by;	/**< Signed by parameter.	*/
    pj_str_t	nonce;		/**< Nonce parameter.		*/
};

/**
 * @see pjsip_pgp_credential
 */
typedef struct pjsip_pgp_credential pjsip_pgp_credential;

/**
 * This structure describes SIP Authorization header (and also SIP
 * Proxy-Authorization header).
 */
struct pjsip_authorization_hdr
{
    /** Standard header fiends. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_authorization_hdr);

    /** Authorization scheme.  */
    pj_str_t scheme;

    /** Type of credentials, depending on the scheme. */
    union
    {
	pjsip_common_credential common;	/**< Common fields.	    */
	pjsip_digest_credential digest;	/**< Digest credentials.    */
	pjsip_pgp_credential	pgp;	/**< PGP credentials.	    */
    } credential;
};

/**
 * @see pjsip_authorization_hdr.
 */
typedef struct pjsip_authorization_hdr pjsip_authorization_hdr;

/** SIP Proxy-Authorization header shares the same structure as SIP
    Authorization header.
 */
typedef struct pjsip_authorization_hdr pjsip_proxy_authorization_hdr;

/**
 * Create SIP Authorization header.
 * @param pool	    Pool where memory will be allocated from.
 * @return	    SIP Authorization header.
 */
PJ_DECL(pjsip_authorization_hdr*) 
pjsip_authorization_hdr_create(pj_pool_t *pool);

/**
 * Create SIP Proxy-Authorization header.
 * @param pool	    Pool where memory will be allocated from.
 * @return SIP	    Proxy-Authorization header.
 */
PJ_DECL(pjsip_proxy_authorization_hdr*) 
pjsip_proxy_authorization_hdr_create(pj_pool_t *pool);


/**
 * This structure describes common fields in authentication challenge
 * headers (WWW-Authenticate and Proxy-Authenticate).
 */
struct pjsip_common_challenge
{
    pj_str_t	realm;		/**< Realm for the challenge.	*/
    pjsip_param	other_param;	/**< Other parameters.		*/
};

/**
 * @see pjsip_common_challenge
 */
typedef struct pjsip_common_challenge pjsip_common_challenge;

/**
 * This structure describes authentication challenge used in Proxy-Authenticate
 * or WWW-Authenticate for digest authentication scheme.
 */
struct pjsip_digest_challenge
{
    pj_str_t	realm;		/**< Realm for the challenge.	*/
    pjsip_param	other_param;	/**< Other parameters.		*/
    pj_str_t	domain;		/**< Domain.			*/
    pj_str_t	nonce;		/**< Nonce challenge.		*/
    pj_str_t	opaque;		/**< Opaque value.		*/
    int		stale;		/**< Stale parameter.		*/
    pj_str_t	algorithm;	/**< Algorithm parameter.	*/
    pj_str_t	qop;		/**< Quality of protection.	*/
};

/**
 * @see pjsip_digest_challenge
 */
typedef struct pjsip_digest_challenge pjsip_digest_challenge;

/**
 * This structure describes authentication challenge used in Proxy-Authenticate
 * or WWW-Authenticate for PGP authentication scheme.
 */
struct pjsip_pgp_challenge
{
    pj_str_t	realm;		/**< Realm for the challenge.	*/
    pjsip_param	other_param;	/**< Other parameters.		*/
    pj_str_t	version;	/**< PGP version.		*/
    pj_str_t	micalgorithm;	/**< micalgorithm parameter.	*/
    pj_str_t	pubalgorithm;	/**< pubalgorithm parameter.	*/ 
    pj_str_t	nonce;		/**< Nonce challenge.		*/
};

/**
 * @see pjsip_pgp_challenge
 */
typedef struct pjsip_pgp_challenge pjsip_pgp_challenge;

/**
 * This structure describe SIP WWW-Authenticate header (Proxy-Authenticate
 * header also uses the same structure).
 */
struct pjsip_www_authenticate_hdr
{
    /** Standard header fields. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_www_authenticate_hdr);

    /** Authentication scheme  */
    pj_str_t	scheme;

    /** This union contains structures that are only relevant
        depending on the value of the scheme being used.
     */
    union
    {
	pjsip_common_challenge	common;	/**< Common fields.	*/
	pjsip_digest_challenge	digest;	/**< Digest challenge.	*/
	pjsip_pgp_challenge	pgp;	/**< PGP challenge.	*/
    } challenge;
};

/**
 * WWW-Authenticate header.
 */
typedef struct pjsip_www_authenticate_hdr pjsip_www_authenticate_hdr;

/**
 * Proxy-Authenticate header.
 */
typedef struct pjsip_www_authenticate_hdr pjsip_proxy_authenticate_hdr;


/**
 * Create SIP WWW-Authenticate header.
 *
 * @param pool	    Pool where memory will be allocated from.
 * @return	    SIP WWW-Authenticate header.
 */
PJ_DECL(pjsip_www_authenticate_hdr*) 
pjsip_www_authenticate_hdr_create(pj_pool_t *pool);

/**
 * Create SIP Proxy-Authenticate header.
 *
 * @param pool	    Pool where memory will be allocated from.
 * @return	    SIP Proxy-Authenticate header.
 */
PJ_DECL(pjsip_proxy_authenticate_hdr*) 
pjsip_proxy_authenticate_hdr_create(pj_pool_t *pool);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_AUTH_SIP_AUTH_MSG_H__ */
