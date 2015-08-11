/* $Id: auth.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "auth.h"
#include <pjlib.h>


#define MAX_REALM	80
#define MAX_USERNAME	32
#define MAX_PASSWORD	32
#define MAX_NONCE	32

static char g_realm[MAX_REALM];

static struct cred_t
{
    char    username[MAX_USERNAME];
    char    passwd[MAX_PASSWORD];
} g_cred[] = 
{
    { "100", "100" },
    { "700", "700" },
    { "701", "701" }
};

#define THIS_FILE	"auth.c"
#define THE_NONCE	"pjnath"
#define LOG(expr)	PJ_LOG(3,expr)


/*
 * Initialize TURN authentication subsystem.
 */
PJ_DEF(pj_status_t) pj_turn_auth_init(const char *realm)
{
    PJ_ASSERT_RETURN(pj_ansi_strlen(realm) < MAX_REALM, PJ_ENAMETOOLONG);
    pj_ansi_strcpy(g_realm, realm);
    return PJ_SUCCESS;
}

/*
 * Shutdown TURN authentication subsystem.
 */
PJ_DEF(void) pj_turn_auth_dinit(void)
{
    /* Nothing to do */
}


/*
 * This function is called by pj_stun_verify_credential() when
 * server needs to challenge the request with 401 response.
 */
PJ_DEF(pj_status_t) pj_turn_get_auth(void *user_data,
				     pj_pool_t *pool,
				     pj_str_t *realm,
				     pj_str_t *nonce)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(pool);

    *realm = pj_str(g_realm);
    *nonce = pj_str(THE_NONCE);

    return PJ_SUCCESS;
}

/*
 * This function is called to get the password for the specified username.
 * This function is also used to check whether the username is valid.
 */
PJ_DEF(pj_status_t) pj_turn_get_password(const pj_stun_msg *msg,
					 void *user_data, 
					 const pj_str_t *realm,
					 const pj_str_t *username,
					 pj_pool_t *pool,
					 pj_stun_passwd_type *data_type,
					 pj_str_t *data)
{
    unsigned i;

    PJ_UNUSED_ARG(msg);
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(pool);

    if (pj_stricmp2(realm, g_realm)) {
	LOG((THIS_FILE, "auth error: invalid realm '%.*s'", 
			(int)realm->slen, realm->ptr));
	return PJ_EINVAL;
    }

    for (i=0; i<PJ_ARRAY_SIZE(g_cred); ++i) {
	if (pj_stricmp2(username, g_cred[i].username) == 0) {
	    *data_type = PJ_STUN_PASSWD_PLAIN;
	    *data = pj_str(g_cred[i].passwd);
	    return PJ_SUCCESS;
	}
    }

    LOG((THIS_FILE, "auth error: user '%.*s' not found", 
		    (int)username->slen, username->ptr));
    return PJ_ENOTFOUND;
}

/*
 * This function will be called to verify that the NONCE given
 * in the message can be accepted. If this callback returns
 * PJ_FALSE, 438 (Stale Nonce) response will be created.
 */
PJ_DEF(pj_bool_t) pj_turn_verify_nonce(const pj_stun_msg *msg,
				       void *user_data,
				       const pj_str_t *realm,
				       const pj_str_t *username,
				       const pj_str_t *nonce)
{
    PJ_UNUSED_ARG(msg);
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(realm);
    PJ_UNUSED_ARG(username);

    if (pj_stricmp2(nonce, THE_NONCE)) {
	LOG((THIS_FILE, "auth error: invalid nonce '%.*s'", 
			(int)nonce->slen, nonce->ptr));
	return PJ_FALSE;
    }

    return PJ_TRUE;
}

