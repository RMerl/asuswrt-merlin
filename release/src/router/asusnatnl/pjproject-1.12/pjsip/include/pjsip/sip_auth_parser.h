/* $Id: sip_auth_parser.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_AUTH_SIP_AUTH_PARSER_H__
#define __PJSIP_AUTH_SIP_AUTH_PARSER_H__

/**
 * @file sip_auth_parser.h
 * @brief SIP Authorization Parser Module.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * Initialize and register authorization parser module.
 * This will register parser handler for various Authorization related headers
 * such as Authorization, WWW-Authenticate, Proxy-Authorizization, and 
 * Proxy-Authenticate headers.
 *
 * This function is called automatically by the main SIP parser.
 *
 * @param inst_id  The instance id of pjsua.
 * @return      PJ_SUCCESS or the appropriate status code.
 */
PJ_DECL(pj_status_t) pjsip_auth_init_parser(int inst_id);

/**
 * DeInitialize authorization parser module.
 */
PJ_DECL(void) pjsip_auth_deinit_parser();



extern const pj_str_t	pjsip_USERNAME_STR, /**< "username" string const.   */
			pjsip_REALM_STR,    /**< "realm" string const.	    */
			pjsip_NONCE_STR,    /**< "nonce" string const.	    */
			pjsip_URI_STR,	    /**< "uri" string const.	    */
			pjsip_RESPONSE_STR, /**< "response" string const.   */
			pjsip_ALGORITHM_STR,/**< "algorithm" string const.  */
			pjsip_DOMAIN_STR,   /**< "domain" string const.	    */
			pjsip_STALE_STR,    /**< "stale" string const.	    */
			pjsip_QOP_STR,	    /**< "qop" string const.	    */
			pjsip_CNONCE_STR,   /**< "cnonce" string const.	    */
			pjsip_OPAQUE_STR,   /**< "opaque" string const.	    */
			pjsip_NC_STR,	    /**< "nc" string const.	    */
			pjsip_TRUE_STR,	    /**< "true" string const.	    */
			pjsip_FALSE_STR,    /**< "false" string const.	    */
			pjsip_DIGEST_STR,   /**< "digest" string const.	    */
			pjsip_PGP_STR,	    /**< "pgp" string const.	    */
			pjsip_MD5_STR,	    /**< "md5" string const.	    */
			pjsip_AUTH_STR;	    /**< "auth" string const.	    */

PJ_END_DECL

#endif	/* __PJSIP_AUTH_SIP_AUTH_PARSER_H__ */

