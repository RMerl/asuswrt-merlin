/* $Id: stun_auth.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_STUN_AUTH_H__
#define __PJNATH_STUN_AUTH_H__

/**
 * @file stun_auth.h
 * @brief STUN authentication.
 */

#include <pjnath/stun_msg.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_STUN_AUTH STUN Authentication
 * @brief STUN authentication helper
 * @ingroup PJNATH_STUN_BASE
 * @{
 */

/**
 * Type of authentication.
 */
typedef enum pj_stun_auth_type
{ 
    /**
     * No authentication.
     */
    PJ_STUN_AUTH_NONE = 0,

    /**
     * Authentication using short term credential.
     */
    PJ_STUN_AUTH_SHORT_TERM = 1,

    /**
     * Authentication using long term credential.
     */
    PJ_STUN_AUTH_LONG_TERM = 2

} pj_stun_auth_type;


/**
 * Type of authentication data in the credential.
 */
typedef enum pj_stun_auth_cred_type
{
    /**
     * The credential data contains a static credential to be matched 
     * against the credential in the message. A static credential can be 
     * used as both client side or server side authentication.
     */
    PJ_STUN_AUTH_CRED_STATIC,

    /**
     * The credential data contains callbacks to be called to verify the 
     * credential in the message. A dynamic credential is suitable when 
     * performing server side authentication where server does not know
     * in advance the identity of the user requesting authentication.
     */
    PJ_STUN_AUTH_CRED_DYNAMIC

} pj_stun_auth_cred_type;


/**
 * Type of encoding applied to the password stored in the credential.
 */
typedef enum pj_stun_passwd_type
{
    /**
     * Plain text password.
     */
    PJ_STUN_PASSWD_PLAIN    = 0,

    /**
     * Hashed password, valid for long term credential only. The hash value
     * of the password is calculated as MD5(USERNAME ":" REALM ":" PASSWD)
     * with all quotes removed from the username and realm values.
     */
    PJ_STUN_PASSWD_HASHED = 1

} pj_stun_passwd_type;


/**
 * This structure contains the descriptions needed to perform server side
 * authentication. Depending on the \a type set in the structure, application
 * may specify a static username/password combination, or to have callbacks
 * called by the function to authenticate the credential dynamically.
 */
typedef struct pj_stun_auth_cred
{
    /**
     * The type of authentication information in this structure.
     */
    pj_stun_auth_cred_type	type;

    /**
     * This union contains the authentication data.
     */
    union 
    {
	/**
	 * This structure contains static data for performing authentication.
	 * A non-empty realm indicates whether short term or long term
	 * credential is used.
	 */
	struct
	{
	    /** 
	     * If not-empty, it indicates that this is a long term credential.
	     */
	    pj_str_t		realm;

	    /** 
	     * The username of the credential.
	     */
	    pj_str_t		username;

	    /**
	     * Data type to indicate the type of password in the \a data field.
	     */
	    pj_stun_passwd_type	data_type;

	    /** 
	     * The data, which depends depends on the value of \a data_type
	     * field. When \a data_type is zero, this field will contain the
	     * plaintext password.
	     */
	    pj_str_t		data;

	    /** 
	     * Optional NONCE.
	     */
	    pj_str_t		nonce;

	} static_cred;

	/**
	 * This structure contains callback to be called by the framework
	 * to authenticate the incoming message.
	 */
	struct
	{
	    /**
	     * User data which will be passed back to callback functions.
	     */
	    void *user_data;

	    /**
	     * This callback is called by pj_stun_verify_credential() when
	     * server needs to challenge the request with 401 response.
	     *
	     * @param user_data	The user data as specified in the credential.
	     * @param pool	Pool to allocate memory.
	     * @param realm	On return, the function should fill in with
	     *			realm if application wants to use long term
	     *			credential. Otherwise application should set
	     *			empty string for the realm.
	     * @param nonce	On return, if application wants to use long
	     *			term credential, it MUST fill in the nonce
	     *			with some value. Otherwise  if short term 
	     *			credential is wanted, it MAY set this value.
	     *			If short term credential is wanted and the
	     *			application doesn't want to include NONCE,
	     *			then it must set this to empty string.
	     *
	     * @return		The callback should return PJ_SUCCESS, or
	     *			otherwise response message will not be 
	     *			created.
	     */
	    pj_status_t (*get_auth)(void *user_data,
				    pj_pool_t *pool,
				    pj_str_t *realm,
				    pj_str_t *nonce);

	    /**
	     * Get the credential to be put in outgoing request.
	     *
	     * @param msg	The outgoing message where the credential is
	     *			to be applied.
	     * @param user_data	The user data as specified in the credential.
	     * @param pool	Pool where the callback can allocate memory
	     *			to fill in the credential.
	     * @param realm	On return, the callback may specify the realm
	     *			if long term credential is desired, otherwise
	     *			this string must be set to empty.
	     * @param username	On return, the callback must fill in with the
	     *			username.
	     * @param nonce	On return, the callback may optionally fill in
	     *			this argument with NONCE value if desired,
	     *			otherwise this argument must be set to empty.
	     * @param data_type	On return, the callback must set this argument
	     *			with the type of password in the data argument.
	     * @param data	On return, the callback must set this with
	     *			the password, encoded according to data_type
	     *			argument.
	     *
	     * @return		The callback must return PJ_SUCCESS, otherwise
	     *			the message transmission will be cancelled.
	     */
	    pj_status_t (*get_cred)(const pj_stun_msg *msg,
				    void *user_data,
				    pj_pool_t *pool,
				    pj_str_t *realm,
				    pj_str_t *username,
				    pj_str_t *nonce,
				    pj_stun_passwd_type *data_type,
				    pj_str_t *data);

	    /**
	     * Get the password for the specified username. This function 
	     * is also used to check whether the username is valid.
	     *
	     * @param msg	The STUN message where the password will be
	     *			applied to.
	     * @param user_data	The user data as specified in the credential.
	     * @param realm	The realm as specified in the message.
	     * @param username	The username as specified in the message.
	     * @param pool	Pool to allocate memory when necessary.
	     * @param data_type On return, application should fill up this
	     *			argument with the type of data (which should
	     *			be zero if data is a plaintext password).
	     * @param data	On return, application should fill up this
	     *			argument with the password according to
	     *			data_type.
	     *
	     * @return		The callback should return PJ_SUCCESS if
	     *			username has been successfully verified
	     *			and password was obtained. If non-PJ_SUCCESS
	     *			is returned, it is assumed that the
	     *			username is not valid.
	     */
	    pj_status_t	(*get_password)(const pj_stun_msg *msg,
					void *user_data, 
				        const pj_str_t *realm,
				        const pj_str_t *username,
					pj_pool_t *pool,
					pj_stun_passwd_type *data_type,
					pj_str_t *data);

	    /**
	     * This callback will be called to verify that the NONCE given
	     * in the message can be accepted. If this callback returns
	     * PJ_FALSE, 438 (Stale Nonce) response will be created.
	     *
	     * This callback is optional.
	     *
	     * @param msg	The STUN message where the nonce was received.
	     * @param user_data	The user data as specified in the credential.
	     * @param realm	The realm as specified in the message.
	     * @param username	The username as specified in the message.
	     * @param nonce	The nonce to be verified.
	     *
	     * @return		The callback MUST return non-zero if the 
	     *			NONCE can be accepted.
	     */
	    pj_bool_t	(*verify_nonce)(const pj_stun_msg *msg,
					void *user_data,
					const pj_str_t *realm,
					const pj_str_t *username,
					const pj_str_t *nonce);

	} dyn_cred;

    } data;

} pj_stun_auth_cred;


/**
 * This structure contains the credential information that is found and
 * used to authenticate incoming requests. Application may use this
 * information when generating authentication for the outgoing response.
 */
typedef struct pj_stun_req_cred_info
{
    /**
     * The REALM value found in the incoming request. If short term 
     * credential is used, the value will be empty.
     */
    pj_str_t	realm;

    /**
     * The USERNAME value found in the incoming request.
     */
    pj_str_t	username;

    /**
     * Optional NONCE.
     */
    pj_str_t	nonce;

    /**
     * Authentication key that was used to authenticate the incoming 
     * request. This key is created with #pj_stun_create_key(), and
     * it can be used to encode the credential of the outgoing
     * response.
     */
    pj_str_t	auth_key;

} pj_stun_req_cred_info;


/**
 * Duplicate authentication credential.
 *
 * @param pool		Pool to be used to allocate memory.
 * @param dst		Destination credential.
 * @param src		Source credential.
 */
PJ_DECL(void) pj_stun_auth_cred_dup(pj_pool_t *pool,
				      pj_stun_auth_cred *dst,
				      const pj_stun_auth_cred *src);

/**
 * Duplicate request credential.
 *
 * @param pool		Pool to be used to allocate memory.
 * @param dst		Destination credential.
 * @param src		Source credential.
 */
PJ_DECL(void) pj_stun_req_cred_info_dup(pj_pool_t *pool,
					pj_stun_req_cred_info *dst,
					const pj_stun_req_cred_info *src);


/**
 * Create authentication key to be used for encoding the message with
 * MESSAGE-INTEGRITY. If short term credential is used (i.e. the realm
 * argument is NULL or empty), the key will be copied from the password.
 * If long term credential is used, the key will be calculated from the
 * MD5 hash of the realm, username, and password.
 *
 * @param pool		Pool to allocate memory for the key.
 * @param key		String to receive the key.
 * @param realm		The realm of the credential, if long term credential
 *			is to be used. If short term credential is wanted,
 *			application can put NULL or empty string here.
 * @param username	The username.
 * @param data_type	Password encoding.
 * @param data		The password.
 */
PJ_DECL(void) pj_stun_create_key(pj_pool_t *pool,
				 pj_str_t *key,
				 const pj_str_t *realm,
				 const pj_str_t *username,
				 pj_stun_passwd_type data_type,
				 const pj_str_t *data);

/**
 * Verify credential in the STUN request. Note that before calling this
 * function, application must have checked that the message contains
 * PJ_STUN_ATTR_MESSAGE_INTEGRITY attribute by calling pj_stun_msg_find_attr()
 * function, because this function will reject the message with 401 error
 * if it doesn't contain PJ_STUN_ATTR_MESSAGE_INTEGRITY attribute.
 *
 * @param pkt		The original packet which has been parsed into
 *			the message. This packet MUST NOT have been modified
 *			after the parsing.
 * @param pkt_len	The length of the packet.
 * @param msg		The parsed message to be verified.
 * @param cred		Pointer to credential to be used to authenticate
 *			the message.
 * @param pool		If response is to be created, then memory will
 *			be allocated from this pool.
 * @param info		Optional pointer to receive authentication information
 *			found in the request and the credential that is used
 *			to authenticate the request.
 * @param p_response	Optional pointer to receive the response message
 *			then the credential in the request fails to
 *			authenticate.
 *
 * @return		PJ_SUCCESS if credential is verified successfully.
 *			If the verification fails and \a p_response is not
 *			NULL, an appropriate response will be returned in
 *			\a p_response.
 */
PJ_DECL(pj_status_t) pj_stun_authenticate_request(const pj_uint8_t *pkt,
					          unsigned pkt_len,
					          const pj_stun_msg *msg,
					          pj_stun_auth_cred *cred,
					          pj_pool_t *pool,
						  pj_stun_req_cred_info *info,
					          pj_stun_msg **p_response);


/**
 * Determine if STUN message can be authenticated. Some STUN error
 * responses cannot be authenticated since they cannot contain STUN
 * MESSAGE-INTEGRITY attribute. STUN Indication messages also cannot
 * be authenticated.
 *
 * @param msg		The STUN message.
 *
 * @return		Non-zero if the STUN message can be authenticated.
 */
PJ_DECL(pj_bool_t) pj_stun_auth_valid_for_msg(const pj_stun_msg *msg);


/**
 * Verify credential in the STUN response. Note that before calling this
 * function, application must have checked that the message contains
 * PJ_STUN_ATTR_MESSAGE_INTEGRITY attribute by calling pj_stun_msg_find_attr()
 * function, because otherwise this function will report authentication
 * failure.
 *
 * @param pkt		The original packet which has been parsed into
 *			the message. This packet MUST NOT have been modified
 *			after the parsing.
 * @param pkt_len	The length of the packet.
 * @param msg		The parsed message to be verified.
 * @param key		Authentication key to calculate MESSAGE-INTEGRITY
 *			value. Application can create this key by using
 *			#pj_stun_create_key() function.
 *
 * @return		PJ_SUCCESS if credential is verified successfully.
 */
PJ_DECL(pj_status_t) pj_stun_authenticate_response(const pj_uint8_t *pkt,
					           unsigned pkt_len,
					           const pj_stun_msg *msg,
					           const pj_str_t *key);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_STUN_AUTH_H__ */

