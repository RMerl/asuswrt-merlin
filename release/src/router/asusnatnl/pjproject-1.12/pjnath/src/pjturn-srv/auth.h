/* $Id: auth.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_TURN_SRV_AUTH_H__
#define __PJ_TURN_SRV_AUTH_H__

#include <pjnath.h>

/**
 * Initialize TURN authentication subsystem.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_turn_auth_init(const char *realm);

/**
 * Shutdown TURN authentication subsystem.
 */
PJ_DECL(void) pj_turn_auth_dinit(void);

/**
 * This function is called by pj_stun_verify_credential() when
 * server needs to challenge the request with 401 response.
 *
 * @param user_data	Should be ignored.
 * @param pool		Pool to allocate memory.
 * @param realm		On return, the function should fill in with
 *			realm if application wants to use long term
 *			credential. Otherwise application should set
 *			empty string for the realm.
 * @param nonce		On return, if application wants to use long
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
PJ_DECL(pj_status_t) pj_turn_get_auth(void *user_data,
				      pj_pool_t *pool,
				      pj_str_t *realm,
				      pj_str_t *nonce);

/**
 * This function is called to get the password for the specified username.
 * This function is also used to check whether the username is valid.
 *
 * @param msg		The STUN message where the password will be
 *			applied to.
 * @param user_data	Should be ignored.
 * @param realm		The realm as specified in the message.
 * @param username	The username as specified in the message.
 * @param pool		Pool to allocate memory when necessary.
 * @param data_type	On return, application should fill up this
 *			argument with the type of data (which should
 *			be zero if data is a plaintext password).
 * @param data		On return, application should fill up this
 *			argument with the password according to
 *			data_type.
 *
 * @return		The callback should return PJ_SUCCESS if
 *			username has been successfully verified
 *			and password was obtained. If non-PJ_SUCCESS
 *			is returned, it is assumed that the
 *			username is not valid.
 */
PJ_DECL(pj_status_t) pj_turn_get_password(const pj_stun_msg *msg,
					  void *user_data, 
					  const pj_str_t *realm,
					  const pj_str_t *username,
					  pj_pool_t *pool,
					  pj_stun_passwd_type *data_type,
					  pj_str_t *data);

/**
 * This function will be called to verify that the NONCE given
 * in the message can be accepted. If this callback returns
 * PJ_FALSE, 438 (Stale Nonce) response will be created.
 *
 * @param msg		The STUN message where the nonce was received.
 * @param user_data	Should be ignored.
 * @param realm		The realm as specified in the message.
 * @param username	The username as specified in the message.
 * @param nonce		The nonce to be verified.
 *
 * @return		The callback MUST return non-zero if the 
 *			NONCE can be accepted.
 */
PJ_DECL(pj_bool_t) pj_turn_verify_nonce(const pj_stun_msg *msg,
					void *user_data,
					const pj_str_t *realm,
					const pj_str_t *username,
					const pj_str_t *nonce);

#endif	/* __PJ_TURN_SRV_AUTH_H__ */

