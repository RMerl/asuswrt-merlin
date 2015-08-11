/* $Id: sip_auth.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_AUTH_SIP_AUTH_H__
#define __PJSIP_AUTH_SIP_AUTH_H__

/**
 * @file pjsip_auth.h
 * @brief SIP Authorization Module.
 */

#include <pjsip/sip_config.h>
#include <pjsip/sip_auth_msg.h>

PJ_BEGIN_DECL

/**
 * @addtogroup PJSIP_AUTH
 * @ingroup PJSIP_CORE
 * @brief Client and server side authentication framework.
 */

/**
 * @defgroup PJSIP_AUTH_API Authentication API's
 * @ingroup PJSIP_AUTH
 * @brief Structures and functions to perform authentication.
 * @{
 */

/** Length of digest string. */
#define PJSIP_MD5STRLEN 32


/** Type of data in the credential information in #pjsip_cred_info. */
typedef enum pjsip_cred_data_type
{
    PJSIP_CRED_DATA_PLAIN_PASSWD=0, /**< Plain text password.		*/
    PJSIP_CRED_DATA_DIGEST	=1, /**< Hashed digest.			*/

    PJSIP_CRED_DATA_EXT_AKA	=16 /**< Extended AKA info is available */

} pjsip_cred_data_type;

/** Authentication's quality of protection (qop) type. */
typedef enum pjsip_auth_qop_type
{
    PJSIP_AUTH_QOP_NONE,	    /**< No quality of protection. */
    PJSIP_AUTH_QOP_AUTH,	    /**< Authentication. */
    PJSIP_AUTH_QOP_AUTH_INT,	    /**< Authentication with integrity protection. */
    PJSIP_AUTH_QOP_UNKNOWN	    /**< Unknown protection. */
} pjsip_auth_qop_type;


/**
 * Type of callback function to create authentication response.
 * Application can specify this callback in \a cb field of the credential info
 * (#pjsip_cred_info) and specifying PJSIP_CRED_DATA_DIGEST_CALLBACK as 
 * \a data_type. When this function is called, most of the fields in the 
 * \a auth authentication response will have been filled by the framework. 
 * Application normally should just need to calculate the response digest 
 * of the authentication response.
 *
 * @param pool	    Pool to allocate memory from if application needs to.
 * @param chal	    The authentication challenge sent by server in 401
 *		    or 401 response, in either Proxy-Authenticate or
 *		    WWW-Authenticate header.
 * @param cred	    The credential that has been selected by the framework
 *		    to authenticate against the challenge.
 * @param auth	    The authentication response which application needs to
 *		    calculate the response digest.
 *
 * @return	    Application may return non-PJ_SUCCESS to abort the
 *		    authentication process. When this happens, the 
 *		    framework will return failure to the original function
 *		    that requested authentication.
 */
typedef pj_status_t (*pjsip_cred_cb)(pj_pool_t *pool,
				     const pjsip_digest_challenge *chal,
				     const pjsip_cred_info *cred,
				     const pj_str_t *method,
				     pjsip_digest_credential *auth);


/** 
 * This structure describes credential information. 
 * A credential information is a static, persistent information that identifies
 * username and password required to authorize to a specific realm.
 *
 * Note that since PJSIP 0.7.0.1, it is possible to make a credential that is
 * valid for any realms, by setting the realm to star/wildcard character,
 * i.e. realm = pj_str("*");.
 */
struct pjsip_cred_info
{
    pj_str_t    realm;		/**< Realm. Use "*" to make a credential that
				     can be used to authenticate against any
				     challenges.			    */
    pj_str_t	scheme;		/**< Scheme (e.g. "digest").		    */
    pj_str_t	username;	/**< User name.				    */
    int		data_type;	/**< Type of data (0 for plaintext passwd). */
    pj_str_t	data;		/**< The data, which can be a plaintext 
				     password or a hashed digest.	    */

    /** Extended data */
    union {
	/** Digest AKA credential information. Note that when AKA credential
	 *  is being used, the \a data field of this #pjsip_cred_info is
	 *  not used, but it still must be initialized to an empty string.
	 * Please see \ref PJSIP_AUTH_AKA_API for more information.
	 */
	struct {
	    pj_str_t	  k;	/**< Permanent subscriber key.		*/
	    pj_str_t	  op;	/**< Operator variant key.		*/
	    pj_str_t	  amf;	/**< Authentication Management Field	*/
	    pjsip_cred_cb cb;	/**< Callback to create AKA digest.	*/
	} aka;

    } ext;
};

/**
 * This structure describes cached value of previously sent Authorization
 * or Proxy-Authorization header. The authentication framework keeps a list
 * of this structure and will resend the same header to the same server
 * as long as the method, uri, and nonce stays the same.
 */
typedef struct pjsip_cached_auth_hdr
{
    /** Standard list member */
    PJ_DECL_LIST_MEMBER(struct pjsip_cached_auth_hdr);

    pjsip_method	     method;	/**< To quickly see the method. */
    pjsip_authorization_hdr *hdr;	/**< The cached header.		*/

} pjsip_cached_auth_hdr;


/**
 * This structure describes authentication information for the specified
 * realm. Each instance of this structure describes authentication "session"
 * between this endpoint and remote server. This "session" information is
 * usefull to keep information that persists for more than one challenge,
 * such as nonce-count and cnonce value.
 *
 * Other than that, this structure also keeps the last authorization headers
 * that have been sent in the cache list.
 */
typedef struct pjsip_cached_auth
{
    /** Standard list member */
    PJ_DECL_LIST_MEMBER(struct pjsip_cached_auth);

    pj_str_t			 realm;	    /**< Realm.			    */
    pj_bool_t			 is_proxy;  /**< Server type (401/407)	    */
    pjsip_auth_qop_type		 qop_value; /**< qop required by server.    */
    unsigned			 stale_cnt; /**< Number of stale retry.	    */
#if PJSIP_AUTH_QOP_SUPPORT
    pj_uint32_t			 nc;	    /**< Nonce count.		    */
    pj_str_t			 cnonce;    /**< Cnonce value.		    */
#endif
    pjsip_www_authenticate_hdr	*last_chal; /**< Last challenge seen.	    */
#if PJSIP_AUTH_HEADER_CACHING
    pjsip_cached_auth_hdr	 cached_hdr;/**< List of cached header for
						 each method.		    */
#endif

} pjsip_cached_auth;


/**
 * This structure describes client authentication session preference.
 * The preference can be set by calling #pjsip_auth_clt_set_prefs().
 */
typedef struct pjsip_auth_clt_pref
{
    /**
     * If this flag is set, the authentication client framework will
     * send an empty Authorization header in each initial request.
     * Default is no.
     */
    pj_bool_t	initial_auth;

    /**
     * Specify the algorithm to use when empty Authorization header 
     * is to be sent for each initial request (see above)
     */
    pj_str_t	algorithm;

} pjsip_auth_clt_pref;


/**
 * Duplicate a client authentication preference setting.
 *
 * @param pool	    The memory pool.
 * @param dst	    Destination client authentication preference.
 * @param src	    Source client authentication preference.
 */
PJ_DECL(void) pjsip_auth_clt_pref_dup(pj_pool_t *pool,
				      pjsip_auth_clt_pref *dst,
				      const pjsip_auth_clt_pref *src);


/**
 * This structure describes client authentication sessions. It keeps
 * all the information needed to authorize the client against all downstream 
 * servers.
 */
typedef struct pjsip_auth_clt_sess
{
    pj_pool_t		*pool;		/**< Pool to use.		    */
    pjsip_endpoint	*endpt;		/**< Endpoint where this belongs.   */
    pjsip_auth_clt_pref  pref;		/**< Preference/options.	    */
    unsigned		 cred_cnt;	/**< Number of credentials.	    */
    pjsip_cred_info	*cred_info;	/**< Array of credential information*/
    pjsip_cached_auth	 cached_auth;	/**< Cached authorization info.	    */

} pjsip_auth_clt_sess;


/**
 * Duplicate a credential info.
 *
 * @param pool	    The memory pool.
 * @param dst	    Destination credential.
 * @param src	    Source credential.
 */
PJ_DECL(void) pjsip_cred_info_dup(pj_pool_t *pool,
				  pjsip_cred_info *dst,
				  const pjsip_cred_info *src);

/**
 * Compare two credential infos.
 *
 * @param cred1	    The credential info to compare.
 * @param cred2	    The credential info to compare.
 *
 * @return	    0 if both credentials are equal.
 */
PJ_DECL(int) pjsip_cred_info_cmp(const pjsip_cred_info *cred1,
				 const pjsip_cred_info *cred2);


/**
 * Type of function to lookup credential for the specified name.
 *
 * @param pool		Pool to initialize the credential info.
 * @param realm		Realm to find the account.
 * @param acc_name	Account name to look for.
 * @param cred_info	The structure to put the credential when it's found.
 *
 * @return		The function MUST return PJ_SUCCESS when it found
 *			a correct credential for the specified account and
 *			realm. Otherwise it may return PJSIP_EAUTHACCNOTFOUND
 *			or PJSIP_EAUTHACCDISABLED.
 */
typedef pj_status_t pjsip_auth_lookup_cred( pj_pool_t *pool,
					    const pj_str_t *realm,
					    const pj_str_t *acc_name,
					    pjsip_cred_info *cred_info );

/** Flag to specify that server is a proxy. */
#define PJSIP_AUTH_SRV_IS_PROXY	    1

/**
 * This structure describes server authentication information.
 */
typedef struct pjsip_auth_srv
{
    pj_str_t		     realm;	/**< Realm to serve.		    */
    pj_bool_t		     is_proxy;	/**< Will issue 407 instead of 401  */
    pjsip_auth_lookup_cred  *lookup;	/**< Lookup function.		    */

} pjsip_auth_srv;


/**
 * Initialize client authentication session data structure, and set the 
 * session to use pool for its subsequent memory allocation. The argument 
 * options should be set to zero for this PJSIP version.
 *
 * @param sess		The client authentication session.
 * @param endpt		Endpoint where this session belongs.
 * @param pool		Pool to use.
 * @param options	Must be zero.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_init( pjsip_auth_clt_sess *sess,
					  pjsip_endpoint *endpt,
					  pj_pool_t *pool, 
					  unsigned options);


/**
 * Clone client initialization session. 
 *
 * @param pool		Pool to use.
 * @param sess		Structure to put the duplicated session.
 * @param rhs		The client session to be cloned.
 *
 * @return		PJ_SUCCESS on success;
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_clone( pj_pool_t *pool,
					   pjsip_auth_clt_sess *sess,
					   const pjsip_auth_clt_sess *rhs);

/**
 * Set the credentials to be used during the session. This will duplicate 
 * the specified credentials using client authentication's pool.
 *
 * @param sess		The client authentication session.
 * @param cred_cnt	Number of credentials.
 * @param c		Array of credentials.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_set_credentials( pjsip_auth_clt_sess *sess,
						     int cred_cnt,
						     const pjsip_cred_info *c);


/**
 * Set the preference for the client authentication session.
 *
 * @param sess		The client authentication session.
 * @param p		Preference.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_set_prefs(pjsip_auth_clt_sess *sess,
					      const pjsip_auth_clt_pref *p);


/**
 * Get the preference for the client authentication session.
 *
 * @param sess		The client authentication session.
 * @param p		Pointer to receive the preference.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_get_prefs(pjsip_auth_clt_sess *sess,
					      pjsip_auth_clt_pref *p);

/**
 * Initialize new request message with authorization headers.
 * This function will put Authorization/Proxy-Authorization headers to the
 * outgoing request message. If caching is enabled (PJSIP_AUTH_HEADER_CACHING)
 * and the session has previously sent Authorization/Proxy-Authorization header
 * with the same method, then the same Authorization/Proxy-Authorization header
 * will be resent from the cache only if qop is not present. If the stack is 
 * configured to automatically generate next Authorization/Proxy-Authorization
 * headers (PJSIP_AUTH_AUTO_SEND_NEXT flag), then new Authorization/Proxy-
 * Authorization headers are calculated and generated when they are not present
 * in the case or if authorization session has qop.
 *
 * If both PJSIP_AUTH_HEADER_CACHING flag and PJSIP_AUTH_AUTO_SEND_NEXT flag
 * are not set, this function will do nothing. The stack then will only send
 * Authorization/Proxy-Authorization to respond 401/407 response.
 *
 * @param sess		The client authentication session.
 * @param tdata		The request message to be initialized.
 *
 * @return		PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_init_req( pjsip_auth_clt_sess *sess,
					      pjsip_tx_data *tdata );


/**
 * Call this function when a transaction failed with 401 or 407 response.
 * This function will reinitialize the original request message with the
 * authentication challenge found in the response message, and add the
 * new authorization header in the authorization cache.
 *
 * Note that upon return the reference counter of the new transmit data
 * will be set to 1.
 *
 * @param sess		The client authentication session.
 * @param rdata		The response message containing 401/407 status.
 * @param old_request	The original request message, which will be re-
 *			created with authorization info.
 * @param new_request	Pointer to receive new request message which
 *			will contain all required authorization headers.
 *
 * @return		PJ_SUCCESS if new request can be successfully
 *			created to respond all the authentication
 *			challenges.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_reinit_req(	pjsip_auth_clt_sess *sess,
						const pjsip_rx_data *rdata,
						pjsip_tx_data *old_request,
						pjsip_tx_data **new_request );

/**
 * Initialize server authorization session data structure to serve the 
 * specified realm and to use lookup_func function to look for the credential 
 * info. 
 *
 * @param pool		Pool used to initialize the authentication server.
 * @param auth_srv	The authentication server structure.
 * @param realm		Realm to be served by the server.
 * @param lookup	Account lookup function.
 * @param options	Options, bitmask of:
 *			- PJSIP_AUTH_SRV_IS_PROXY: to specify that the server
 *			  will authorize clients as a proxy server (instead of
 *			  as UAS), which means that Proxy-Authenticate will 
 *			  be used instead of WWW-Authenticate.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_init( pj_pool_t *pool,
					  pjsip_auth_srv *auth_srv,
					  const pj_str_t *realm,
					  pjsip_auth_lookup_cred *lookup,
					  unsigned options );


/**
 * Request the authorization server framework to verify the authorization 
 * information in the specified request in rdata.
 *
 * @param auth_srv	The server authentication structure.
 * @param rdata		Incoming request to be authenticated.
 * @param status_code	When not null, it will be filled with suitable 
 *			status code to be sent to the client.
 *
 * @return		PJ_SUCCESS if request is successfully authenticated.
 *			Otherwise the function may return one of the
 *			following error codes:
 *			- PJSIP_EAUTHNOAUTH
 *			- PJSIP_EINVALIDAUTHSCHEME
 *			- PJSIP_EAUTHACCNOTFOUND
 *			- PJSIP_EAUTHACCDISABLED
 *			- PJSIP_EAUTHINVALIDREALM
 *			- PJSIP_EAUTHINVALIDDIGEST
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_verify( pjsip_auth_srv *auth_srv,
					    pjsip_rx_data *rdata,
					    int *status_code );


/**
 * Add authentication challenge headers to the outgoing response in tdata. 
 * Application may specify its customized nonce and opaque for the challenge, 
 * or can leave the value to NULL to make the function fills them in with 
 * random characters.
 *
 * @param auth_srv	The server authentication structure.
 * @param qop		Optional qop value.
 * @param nonce		Optional nonce value.
 * @param opaque	Optional opaque value.
 * @param stale		Stale indication.
 * @param tdata		The outgoing response message. The response must have
 *			401 or 407 response code.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_challenge( pjsip_auth_srv *auth_srv,
					       const pj_str_t *qop,
					       const pj_str_t *nonce,
					       const pj_str_t *opaque,
					       pj_bool_t stale,
					       pjsip_tx_data *tdata);

/**
 * Helper function to create MD5 digest out of the specified 
 * parameters.
 *
 * @param result	String to store the response digest. This string
 *			must have been preallocated by caller with the 
 *			buffer at least PJSIP_MD5STRLEN (32 bytes) in size.
 * @param nonce		Optional nonce.
 * @param nc		Nonce count.
 * @param cnonce	Optional cnonce.
 * @param qop		Optional qop.
 * @param uri		URI.
 * @param realm		Realm.
 * @param cred_info	Credential info.
 * @param method	SIP method.
 */
PJ_DECL(void) pjsip_auth_create_digest(pj_str_t *result,
				       const pj_str_t *nonce,
				       const pj_str_t *nc,
				       const pj_str_t *cnonce,
				       const pj_str_t *qop,
				       const pj_str_t *uri,
				       const pj_str_t *realm,
				       const pjsip_cred_info *cred_info,
				       const pj_str_t *method);

/**
 * @}
 */



PJ_END_DECL


#endif	/* __PJSIP_AUTH_SIP_AUTH_H__ */

