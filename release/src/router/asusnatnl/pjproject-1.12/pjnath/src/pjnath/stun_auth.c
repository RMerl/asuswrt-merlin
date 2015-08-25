/* $Id: stun_auth.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjnath/stun_auth.h>
#include <pjnath/errno.h>
#include <pjlib-util/hmac_sha1.h>
#include <pjlib-util/md5.h>
#include <pjlib-util/sha1.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE   "stun_auth.c"

/* Duplicate credential */
PJ_DEF(void) pj_stun_auth_cred_dup( pj_pool_t *pool,
				      pj_stun_auth_cred *dst,
				      const pj_stun_auth_cred *src)
{
    dst->type = src->type;

    switch (src->type) {
    case PJ_STUN_AUTH_CRED_STATIC:
	pj_strdup(pool, &dst->data.static_cred.realm,
			&src->data.static_cred.realm);
	pj_strdup(pool, &dst->data.static_cred.username,
			&src->data.static_cred.username);
	dst->data.static_cred.data_type = src->data.static_cred.data_type;
	pj_strdup(pool, &dst->data.static_cred.data,
			&src->data.static_cred.data);
	pj_strdup(pool, &dst->data.static_cred.nonce,
			&src->data.static_cred.nonce);
	break;
    case PJ_STUN_AUTH_CRED_DYNAMIC:
	pj_memcpy(&dst->data.dyn_cred, &src->data.dyn_cred, 
		  sizeof(src->data.dyn_cred));
	break;
    }
}


/*
 * Duplicate request credential.
 */
PJ_DEF(void) pj_stun_req_cred_info_dup( pj_pool_t *pool,
					pj_stun_req_cred_info *dst,
					const pj_stun_req_cred_info *src)
{
    pj_strdup(pool, &dst->realm, &src->realm);
    pj_strdup(pool, &dst->username, &src->username);
    pj_strdup(pool, &dst->nonce, &src->nonce);
    pj_strdup(pool, &dst->auth_key, &src->auth_key);
}


/* Calculate HMAC-SHA1 key for long term credential, by getting
 * MD5 digest of username, realm, and password. 
 */
static void calc_md5_key(pj_uint8_t digest[16],
			 const pj_str_t *realm,
			 const pj_str_t *username,
			 const pj_str_t *passwd)
{
    /* The 16-byte key for MESSAGE-INTEGRITY HMAC is formed by taking
     * the MD5 hash of the result of concatenating the following five
     * fields: (1) The username, with any quotes and trailing nulls
     * removed, (2) A single colon, (3) The realm, with any quotes and
     * trailing nulls removed, (4) A single colon, and (5) The 
     * password, with any trailing nulls removed.
     */
    pj_md5_context ctx;
    pj_str_t s;

    pj_md5_init(&ctx);

#define REMOVE_QUOTE(s)	if (s.slen && *s.ptr=='"') \
			    s.ptr++, s.slen--; \
			if (s.slen && s.ptr[s.slen-1]=='"') \
			    s.slen--;

    /* Add username */
    s = *username;
    REMOVE_QUOTE(s);
    pj_md5_update(&ctx, (pj_uint8_t*)s.ptr, s.slen);

    /* Add single colon */
    pj_md5_update(&ctx, (pj_uint8_t*)":", 1);

    /* Add realm */
    s = *realm;
    REMOVE_QUOTE(s);
    pj_md5_update(&ctx, (pj_uint8_t*)s.ptr, s.slen);

#undef REMOVE_QUOTE

    /* Another colon */
    pj_md5_update(&ctx, (pj_uint8_t*)":", 1);

    /* Add password */
    pj_md5_update(&ctx, (pj_uint8_t*)passwd->ptr, passwd->slen);

    /* Done */
    pj_md5_final(&ctx, digest);
}


/*
 * Create authentication key to be used for encoding the message with
 * MESSAGE-INTEGRITY. 
 */
PJ_DEF(void) pj_stun_create_key(pj_pool_t *pool,
				pj_str_t *key,
				const pj_str_t *realm,
				const pj_str_t *username,
				pj_stun_passwd_type data_type,
				const pj_str_t *data)
{
    PJ_ASSERT_ON_FAIL(pool && key && username && data, return);

    if (realm && realm->slen) {
	if (data_type == PJ_STUN_PASSWD_PLAIN) {
	    key->ptr = (char*) pj_pool_alloc(pool, 16);
	    calc_md5_key((pj_uint8_t*)key->ptr, realm, username, data);
	    key->slen = 16;
	} else {
	    pj_strdup(pool, key, data);
	}
    } else {
	pj_assert(data_type == PJ_STUN_PASSWD_PLAIN);
	pj_strdup(pool, key, data);
    }
}


PJ_INLINE(pj_uint16_t) GET_VAL16(const pj_uint8_t *pdu, unsigned pos)
{
    return (pj_uint16_t) ((pdu[pos] << 8) + pdu[pos+1]);
}


PJ_INLINE(void) PUT_VAL16(pj_uint8_t *buf, unsigned pos, pj_uint16_t hval)
{
    buf[pos+0] = (pj_uint8_t) ((hval & 0xFF00) >> 8);
    buf[pos+1] = (pj_uint8_t) ((hval & 0x00FF) >> 0);
}


/* Send 401 response */
static pj_status_t create_challenge(pj_pool_t *pool,
				    const pj_stun_msg *msg,
				    int err_code,
				    const char *errstr,
				    const pj_str_t *realm,
				    const pj_str_t *nonce,
				    pj_stun_msg **p_response)
{
    pj_stun_msg *response;
    pj_str_t tmp_nonce;
    pj_str_t err_msg;
    pj_status_t rc;

    rc = pj_stun_msg_create_response(pool, msg, err_code, 
			             (errstr?pj_cstr(&err_msg, errstr):NULL), 
				     &response);
    if (rc != PJ_SUCCESS)
	return rc;

    /* SHOULD NOT add REALM, NONCE, USERNAME, and M-I on 400 response */
    if (err_code!=400 && realm && realm->slen) {
	rc = pj_stun_msg_add_string_attr(pool, response,
					 PJ_STUN_ATTR_REALM, 
					 realm);
	if (rc != PJ_SUCCESS)
	    return rc;

	/* long term must include nonce */
	if (!nonce || nonce->slen == 0) {
	    tmp_nonce = pj_str("pjstun");
	    nonce = &tmp_nonce;
	}
    }

    if (err_code!=400 && nonce && nonce->slen) {
	rc = pj_stun_msg_add_string_attr(pool, response,
					 PJ_STUN_ATTR_NONCE, 
					 nonce);
	if (rc != PJ_SUCCESS)
	    return rc;
    }

    *p_response = response;

    return PJ_SUCCESS;
}


/* Verify credential in the request */
PJ_DEF(pj_status_t) pj_stun_authenticate_request(const pj_uint8_t *pkt,
					         unsigned pkt_len,
					         const pj_stun_msg *msg,
					         pj_stun_auth_cred *cred,
					         pj_pool_t *pool,
						 pj_stun_req_cred_info *p_info,
					         pj_stun_msg **p_response)
{
    pj_stun_req_cred_info tmp_info;
    const pj_stun_msgint_attr *amsgi;
    unsigned i, amsgi_pos;
    pj_bool_t has_attr_beyond_mi;
    const pj_stun_username_attr *auser;
    const pj_stun_realm_attr *arealm;
    const pj_stun_realm_attr *anonce;
    pj_hmac_sha1_context ctx;
    pj_uint8_t digest[PJ_SHA1_DIGEST_SIZE];
    pj_stun_status err_code;
    const char *err_text = NULL;
    pj_status_t status;

    /* msg and credential MUST be specified */
    PJ_ASSERT_RETURN(pkt && pkt_len && msg && cred, PJ_EINVAL);

    /* If p_response is specified, pool MUST be specified. */
    PJ_ASSERT_RETURN(!p_response || pool, PJ_EINVAL);

    if (p_response)
	*p_response = NULL;

    if (!PJ_STUN_IS_REQUEST(msg->hdr.type))
	p_response = NULL;

    if (p_info == NULL)
	p_info = &tmp_info;

    pj_bzero(p_info, sizeof(pj_stun_req_cred_info));

    /* Get realm and nonce from credential */
    p_info->realm.slen = p_info->nonce.slen = 0;
    if (cred->type == PJ_STUN_AUTH_CRED_STATIC) {
	p_info->realm = cred->data.static_cred.realm;
	p_info->nonce = cred->data.static_cred.nonce;
    } else if (cred->type == PJ_STUN_AUTH_CRED_DYNAMIC) {
	status = cred->data.dyn_cred.get_auth(cred->data.dyn_cred.user_data,
					      pool, &p_info->realm, 
					      &p_info->nonce);
	if (status != PJ_SUCCESS)
	    return status;
    } else {
	pj_assert(!"Invalid credential type");
	return PJ_EBUG;
    }

    /* Look for MESSAGE-INTEGRITY while counting the position */
    amsgi_pos = 0;
    has_attr_beyond_mi = PJ_FALSE;
    amsgi = NULL;
    for (i=0; i<msg->attr_count; ++i) {
	if (msg->attr[i]->type == PJ_STUN_ATTR_MESSAGE_INTEGRITY) {
	    amsgi = (const pj_stun_msgint_attr*) msg->attr[i];
	} else if (amsgi) {
	    has_attr_beyond_mi = PJ_TRUE;
	    break;
	} else {
	    amsgi_pos += ((msg->attr[i]->length+3) & ~0x03) + 4;
	}
    }

    if (amsgi == NULL) {
	/* According to rfc3489bis-10 Sec 10.1.2/10.2.2, we should return 400
	   for short term, and 401 for long term.
	   The rule has been changed from rfc3489bis-06
	*/
	err_code = p_info->realm.slen ? PJ_STUN_SC_UNAUTHORIZED : 
		    PJ_STUN_SC_BAD_REQUEST;
	goto on_auth_failed;
    }

    /* Next check that USERNAME is present */
    auser = (const pj_stun_username_attr*)
	    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_USERNAME, 0);
	PJ_LOG(4, ("", "pj_stun_authenticate_request() [un=%.*s] [tid=%08x%08x%08x]", 
		auser->value.slen, 
		auser->value.ptr,
		pj_ntohl(*(pj_uint32_t*)&msg->hdr.tsx_id[0]),
		pj_ntohl(*(pj_uint32_t*)&msg->hdr.tsx_id[4]),
		pj_ntohl(*(pj_uint32_t*)&msg->hdr.tsx_id[8])));
    if (auser == NULL) {
	/* According to rfc3489bis-10 Sec 10.1.2/10.2.2, we should return 400
	   for both short and long term, since M-I is present.
	   The rule has been changed from rfc3489bis-06
	*/
	err_code = PJ_STUN_SC_BAD_REQUEST;
	err_text = "Missing USERNAME";
	goto on_auth_failed;
    }

    /* Get REALM, if any */
    arealm = (const pj_stun_realm_attr*)
	     pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REALM, 0);

    /* Reject with 400 if we have long term credential and the request
     * is missing REALM attribute.
     */
    if (p_info->realm.slen && arealm==NULL) {
	err_code = PJ_STUN_SC_BAD_REQUEST;
	err_text = "Missing REALM";
	goto on_auth_failed;
    }

    /* Check if username match */
    if (cred->type == PJ_STUN_AUTH_CRED_STATIC) {
	pj_bool_t username_ok;
	username_ok = !pj_strcmp(&auser->value, 
				 &cred->data.static_cred.username);
	if (username_ok) {
	    pj_strdup(pool, &p_info->username, 
		      &cred->data.static_cred.username);
	    pj_stun_create_key(pool, &p_info->auth_key, &p_info->realm,
			       &auser->value, cred->data.static_cred.data_type,
			       &cred->data.static_cred.data);
	} else {
	    /* Username mismatch */
	    /* According to rfc3489bis-10 Sec 10.1.2/10.2.2, we should 
	     * return 401 
	     */
	    err_code = PJ_STUN_SC_UNAUTHORIZED;
	    goto on_auth_failed;
	}
    } else if (cred->type == PJ_STUN_AUTH_CRED_DYNAMIC) {
	pj_stun_passwd_type data_type = PJ_STUN_PASSWD_PLAIN;
	pj_str_t password;
	pj_status_t rc;

	rc = cred->data.dyn_cred.get_password(msg, 
					      cred->data.dyn_cred.user_data,
					      (arealm?&arealm->value:NULL),
					      &auser->value, pool,
					      &data_type, &password);
	if (rc == PJ_SUCCESS) {
	    pj_strdup(pool, &p_info->username, &auser->value);
	    pj_stun_create_key(pool, &p_info->auth_key, 
			       (arealm?&arealm->value:NULL), &auser->value, 
			       data_type, &password);
	} else {
	    err_code = PJ_STUN_SC_UNAUTHORIZED;
	    goto on_auth_failed;
	}
    } else {
	pj_assert(!"Invalid credential type");
	return PJ_EBUG;
    }



    /* Get NONCE attribute */
    anonce = (pj_stun_nonce_attr*)
	     pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_NONCE, 0);

    /* Check for long term/short term requirements. */
    if (p_info->realm.slen != 0 && arealm == NULL) {
	/* Long term credential is required and REALM is not present */
	err_code = PJ_STUN_SC_BAD_REQUEST;
	err_text = "Missing REALM";
	goto on_auth_failed;

    } else if (p_info->realm.slen != 0 && arealm != NULL) {
	/* We want long term, and REALM is present */

	/* NONCE must be present. */
	if (anonce == NULL && p_info->nonce.slen) {
	    err_code = PJ_STUN_SC_BAD_REQUEST;
	    err_text = "Missing NONCE";
	    goto on_auth_failed;
	}

	/* Verify REALM matches */
	if (pj_stricmp(&arealm->value, &p_info->realm)) {
	    /* REALM doesn't match */
	    err_code = PJ_STUN_SC_UNAUTHORIZED;
	    err_text = "Invalid REALM";
	    goto on_auth_failed;
	}

	/* Valid case, will validate the message integrity later */

    } else if (p_info->realm.slen == 0 && arealm != NULL) {
	/* We want to use short term credential, but client uses long
	 * term credential. The draft doesn't mention anything about
	 * switching between long term and short term.
	 */
	
	/* For now just accept the credential, anyway it will probably
	 * cause wrong message integrity value later.
	 */
    } else if (p_info->realm.slen==0 && arealm == NULL) {
	/* Short term authentication is wanted, and one is supplied */

	/* Application MAY request NONCE to be supplied */
	if (p_info->nonce.slen != 0) {
	    err_code = PJ_STUN_SC_UNAUTHORIZED;
	    err_text = "NONCE required";
	    goto on_auth_failed;
	}
    }

    /* If NONCE is present, validate it */
    if (anonce) {
	pj_bool_t ok;

	if (cred->type == PJ_STUN_AUTH_CRED_DYNAMIC &&
	    cred->data.dyn_cred.verify_nonce != NULL) 
	{
	    ok=cred->data.dyn_cred.verify_nonce(msg, 
						cred->data.dyn_cred.user_data,
						(arealm?&arealm->value:NULL),
						&auser->value,
						&anonce->value);
	} else if (cred->type == PJ_STUN_AUTH_CRED_DYNAMIC) {
	    ok = PJ_TRUE;
	} else {
	    if (p_info->nonce.slen) {
		ok = !pj_strcmp(&anonce->value, &p_info->nonce);
	    } else {
		ok = PJ_TRUE;
	    }
	}

	if (!ok) {
	    err_code = PJ_STUN_SC_STALE_NONCE;
	    goto on_auth_failed;
	}
    }

    /* Now calculate HMAC of the message. */
    pj_hmac_sha1_init(&ctx, (pj_uint8_t*)p_info->auth_key.ptr, 
		      p_info->auth_key.slen);

#if PJ_STUN_OLD_STYLE_MI_FINGERPRINT
    /* Pre rfc3489bis-06 style of calculation */
    pj_hmac_sha1_update(&ctx, pkt, 20);
#else
    /* First calculate HMAC for the header.
     * The calculation is different depending on whether FINGERPRINT attribute
     * is present in the message.
     */
    if (has_attr_beyond_mi) {
	pj_uint8_t hdr_copy[20];
	pj_memcpy(hdr_copy, pkt, 20);
	PUT_VAL16(hdr_copy, 2, (pj_uint16_t)(amsgi_pos + 24));
	pj_hmac_sha1_update(&ctx, hdr_copy, 20);
    } else {
	pj_hmac_sha1_update(&ctx, pkt, 20);
    }
#endif	/* PJ_STUN_OLD_STYLE_MI_FINGERPRINT */

    /* Now update with the message body */
    pj_hmac_sha1_update(&ctx, pkt+20, amsgi_pos);
#if PJ_STUN_OLD_STYLE_MI_FINGERPRINT
    // This is no longer necessary as per rfc3489bis-08
    if ((amsgi_pos+20) & 0x3F) {
    	pj_uint8_t zeroes[64];
    	pj_bzero(zeroes, sizeof(zeroes));
    	pj_hmac_sha1_update(&ctx, zeroes, 64-((amsgi_pos+20) & 0x3F));
    }
#endif
    pj_hmac_sha1_final(&ctx, digest);


    /* Compare HMACs */
    if (pj_memcmp(amsgi->hmac, digest, 20)) {
	/* HMAC value mismatch */
	/* According to rfc3489bis-10 Sec 10.1.2 we should return 401 */
	err_code = PJ_STUN_SC_UNAUTHORIZED;
	err_text = "MESSAGE-INTEGRITY mismatch";
	goto on_auth_failed;
    }

    /* Everything looks okay! */
    return PJ_SUCCESS;

on_auth_failed:
    if (p_response) {
	create_challenge(pool, msg, err_code, err_text,
			 &p_info->realm, &p_info->nonce, p_response);
    }
    return PJ_STATUS_FROM_STUN_CODE(err_code);
}


/* Determine if STUN message can be authenticated */
PJ_DEF(pj_bool_t) pj_stun_auth_valid_for_msg(const pj_stun_msg *msg)
{
    unsigned msg_type = msg->hdr.type;
    const pj_stun_errcode_attr *err_attr;

    /* STUN requests and success response can be authenticated */
    if (!PJ_STUN_IS_ERROR_RESPONSE(msg_type) && 
	!PJ_STUN_IS_INDICATION(msg_type))
    {
	return PJ_TRUE;
    }

    /* STUN Indication cannot be authenticated */
    if (PJ_STUN_IS_INDICATION(msg_type))
	return PJ_FALSE;

    /* Authentication for STUN error responses depend on the error
     * code.
     */
    err_attr = (const pj_stun_errcode_attr*)
	       pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_ERROR_CODE, 0);
    if (err_attr == NULL) {
	PJ_LOG(4,(THIS_FILE, "STUN error code attribute not present in "
			     "error response"));
	return PJ_TRUE;
    }

    switch (err_attr->err_code) {
    case PJ_STUN_SC_BAD_REQUEST:	    /* 400 (Bad Request)	    */
    case PJ_STUN_SC_UNAUTHORIZED:	    /* 401 (Unauthorized)	    */
    case PJ_STUN_SC_STALE_NONCE:	    /* 438 (Stale Nonce)	    */

    /* Due to the way this response is generated here, we can't really
     * authenticate 420 (Unknown Attribute) response			    */
    case PJ_STUN_SC_UNKNOWN_ATTRIBUTE:
	return PJ_FALSE;
    default:
	return PJ_TRUE;
    }
}


/* Authenticate MESSAGE-INTEGRITY in the response */
PJ_DEF(pj_status_t) pj_stun_authenticate_response(const pj_uint8_t *pkt,
					          unsigned pkt_len,
					          const pj_stun_msg *msg,
					          const pj_str_t *key)
{
    const pj_stun_msgint_attr *amsgi;
    unsigned i, amsgi_pos;
    pj_bool_t has_attr_beyond_mi;
    pj_hmac_sha1_context ctx;
    pj_uint8_t digest[PJ_SHA1_DIGEST_SIZE];

    PJ_ASSERT_RETURN(pkt && pkt_len && msg && key, PJ_EINVAL);

    /* First check that MESSAGE-INTEGRITY is present */
    amsgi = (const pj_stun_msgint_attr*)
	    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 0);
    if (amsgi == NULL) {
	return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_UNAUTHORIZED);
    }


    /* Check that message length is valid */
    if (msg->hdr.length < 24) {
	return PJNATH_EINSTUNMSGLEN;
    }

    /* Look for MESSAGE-INTEGRITY while counting the position */
    amsgi_pos = 0;
    has_attr_beyond_mi = PJ_FALSE;
    amsgi = NULL;
    for (i=0; i<msg->attr_count; ++i) {
	if (msg->attr[i]->type == PJ_STUN_ATTR_MESSAGE_INTEGRITY) {
	    amsgi = (const pj_stun_msgint_attr*) msg->attr[i];
	} else if (amsgi) {
	    has_attr_beyond_mi = PJ_TRUE;
	    break;
	} else {
	    amsgi_pos += ((msg->attr[i]->length+3) & ~0x03) + 4;
	}
    }

    if (amsgi == NULL) {
	return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_BAD_REQUEST);
    }

    /* Now calculate HMAC of the message. */
    pj_hmac_sha1_init(&ctx, (pj_uint8_t*)key->ptr, key->slen);

#if PJ_STUN_OLD_STYLE_MI_FINGERPRINT
    /* Pre rfc3489bis-06 style of calculation */
    pj_hmac_sha1_update(&ctx, pkt, 20);
#else
    /* First calculate HMAC for the header.
     * The calculation is different depending on whether FINGERPRINT attribute
     * is present in the message.
     */
    if (has_attr_beyond_mi) {
	pj_uint8_t hdr_copy[20];
	pj_memcpy(hdr_copy, pkt, 20);
	PUT_VAL16(hdr_copy, 2, (pj_uint16_t)(amsgi_pos+24));
	pj_hmac_sha1_update(&ctx, hdr_copy, 20);
    } else {
	pj_hmac_sha1_update(&ctx, pkt, 20);
    }
#endif	/* PJ_STUN_OLD_STYLE_MI_FINGERPRINT */

    /* Now update with the message body */
    pj_hmac_sha1_update(&ctx, pkt+20, amsgi_pos);
#if PJ_STUN_OLD_STYLE_MI_FINGERPRINT
    // This is no longer necessary as per rfc3489bis-08
    if ((amsgi_pos+20) & 0x3F) {
    	pj_uint8_t zeroes[64];
    	pj_bzero(zeroes, sizeof(zeroes));
    	pj_hmac_sha1_update(&ctx, zeroes, 64-((amsgi_pos+20) & 0x3F));
    }
#endif
    pj_hmac_sha1_final(&ctx, digest);

    /* Compare HMACs */
    if (pj_memcmp(amsgi->hmac, digest, 20)) {
	/* HMAC value mismatch */
	return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_UNAUTHORIZED);
    }

    /* Everything looks okay! */
    return PJ_SUCCESS;
}

