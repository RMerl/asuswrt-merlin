/* $Id: sip_auth_aka.c 3916 2011-12-20 09:56:26Z bennylp $ */
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
#include <pjsip/sip_auth_aka.h>
#include <pjsip/sip_errno.h>
#include <pjlib-util/base64.h>
#include <pjlib-util/md5.h>
#include <pjlib-util/hmac_md5.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#if PJSIP_HAS_DIGEST_AKA_AUTH

#include "../../third_party/milenage/milenage.h"

/*
 * Create MD5-AKA1 digest response.
 */
PJ_DEF(pj_status_t) pjsip_auth_create_aka_response( 
					     pj_pool_t *pool,
					     const pjsip_digest_challenge*chal,
					     const pjsip_cred_info *cred,
					     const pj_str_t *method,
					     pjsip_digest_credential *auth)
{
    pj_str_t nonce_bin;
    int aka_version;
    const pj_str_t pjsip_AKAv1_MD5 = { "AKAv1-MD5", 9 };
    const pj_str_t pjsip_AKAv2_MD5 = { "AKAv2-MD5", 9 };
    pj_uint8_t *chal_rand, *chal_sqnxoraka, *chal_mac;
    pj_uint8_t k[PJSIP_AKA_KLEN];
    pj_uint8_t op[PJSIP_AKA_OPLEN];
    pj_uint8_t amf[PJSIP_AKA_AMFLEN];
    pj_uint8_t res[PJSIP_AKA_RESLEN];
    pj_uint8_t ck[PJSIP_AKA_CKLEN];
    pj_uint8_t ik[PJSIP_AKA_IKLEN];
    pj_uint8_t ak[PJSIP_AKA_AKLEN];
    pj_uint8_t sqn[PJSIP_AKA_SQNLEN];
    pj_uint8_t xmac[PJSIP_AKA_MACLEN];
    pjsip_cred_info aka_cred;
    int i, len;
    pj_status_t status;

    /* Check the algorithm is supported. */
    if (chal->algorithm.slen==0 || pj_stricmp2(&chal->algorithm, "md5") == 0) {
	/*
	 * A normal MD5 authentication is requested. Fallbackt to the usual
	 * MD5 digest creation.
	 */
	pjsip_auth_create_digest(&auth->response, &auth->nonce, &auth->nc,
				 &auth->cnonce, &auth->qop, &auth->uri,
				 &auth->realm, cred, method);
	return PJ_SUCCESS;

    } else if (pj_stricmp(&chal->algorithm, &pjsip_AKAv1_MD5) == 0) {
	/*
	 * AKA version 1 is requested.
	 */
	aka_version = 1;

    } else if (pj_stricmp(&chal->algorithm, &pjsip_AKAv2_MD5) == 0) {
	/*
	 * AKA version 2 is requested.
	 */
	aka_version = 2;

    } else {
	/* Unsupported algorithm */
	return PJSIP_EINVALIDALGORITHM;
    }

    /* Decode nonce */
    nonce_bin.slen = len = PJ_BASE64_TO_BASE256_LEN(chal->nonce.slen);
    nonce_bin.ptr = pj_pool_alloc(pool, nonce_bin.slen + 1);
    status = pj_base64_decode(&chal->nonce, (pj_uint8_t*)nonce_bin.ptr, &len);
    nonce_bin.slen = len;
    if (status != PJ_SUCCESS)
	return PJSIP_EAUTHINNONCE;

    if (nonce_bin.slen < PJSIP_AKA_RANDLEN + PJSIP_AKA_AUTNLEN)
	return PJSIP_EAUTHINNONCE;

    /* Get RAND, AUTN, and MAC */
    chal_rand = (pj_uint8_t*)(nonce_bin.ptr + 0);
    chal_sqnxoraka = (pj_uint8_t*) (nonce_bin.ptr + PJSIP_AKA_RANDLEN);
    chal_mac = (pj_uint8_t*) (nonce_bin.ptr + PJSIP_AKA_RANDLEN + 
			      PJSIP_AKA_SQNLEN + PJSIP_AKA_AMFLEN);

    /* Copy k. op, and amf */
    pj_bzero(k, sizeof(k));
    pj_bzero(op, sizeof(op));
    pj_bzero(amf, sizeof(amf));

    if (cred->ext.aka.k.slen)
	pj_memcpy(k, cred->ext.aka.k.ptr, cred->ext.aka.k.slen);
    if (cred->ext.aka.op.slen)
	pj_memcpy(op, cred->ext.aka.op.ptr, cred->ext.aka.op.slen);
    if (cred->ext.aka.amf.slen)
	pj_memcpy(amf, cred->ext.aka.amf.ptr, cred->ext.aka.amf.slen);

    /* Given key K and random challenge RAND, compute response RES,
     * confidentiality key CK, integrity key IK and anonymity key AK.
     */
    f2345(k, chal_rand, res, ck, ik, ak, op);

    /* Compute sequence number SQN */
    for (i=0; i<PJSIP_AKA_SQNLEN; ++i)
	sqn[i] = (pj_uint8_t) (chal_sqnxoraka[i] ^ ak[i]);

    /* Verify MAC in the challenge */
    /* Compute XMAC */
    f1(k, chal_rand, sqn, amf, xmac, op);

    if (pj_memcmp(chal_mac, xmac, PJSIP_AKA_MACLEN) != 0) {
	return PJSIP_EAUTHINNONCE;
    }

    /* Build a temporary credential info to create MD5 digest, using
     * "res" as the password. 
     */
    pj_memcpy(&aka_cred, cred, sizeof(aka_cred));
    aka_cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;

    /* Create a response */
    if (aka_version == 1) {
	/*
	 * For AKAv1, the password is RES
	 */
	aka_cred.data.ptr = (char*)res;
	aka_cred.data.slen = PJSIP_AKA_RESLEN;

	pjsip_auth_create_digest(&auth->response, &chal->nonce, 
				 &auth->nc, &auth->cnonce, &auth->qop, 
				 &auth->uri, &chal->realm, &aka_cred, method);

    } else if (aka_version == 2) {

	/*
	 * For AKAv2, password is base64 encoded [1] parameters:
	 *    PRF(RES||IK||CK,"http-digest-akav2-password")
	 *
	 * The pseudo-random function (PRF) is HMAC-MD5 in this case.
	 */
	
	pj_str_t resikck;
	const pj_str_t AKAv2_Passwd = { "http-digest-akav2-password", 26 };
	pj_uint8_t hmac_digest[16];
	char tmp_buf[48];
	int hmac64_len;

	resikck.slen = PJSIP_AKA_RESLEN + PJSIP_AKA_IKLEN + PJSIP_AKA_CKLEN;
	pj_assert(resikck.slen <= PJ_ARRAY_SIZE(tmp_buf));
	resikck.ptr = tmp_buf;
	pj_memcpy(resikck.ptr + 0, res, PJSIP_AKA_RESLEN);
	pj_memcpy(resikck.ptr + PJSIP_AKA_RESLEN, ik, PJSIP_AKA_IKLEN);
	pj_memcpy(resikck.ptr + PJSIP_AKA_RESLEN + PJSIP_AKA_IKLEN,
		  ck, PJSIP_AKA_CKLEN);

	pj_hmac_md5((const pj_uint8_t*)AKAv2_Passwd.ptr, AKAv2_Passwd.slen,
	            (const pj_uint8_t*)resikck.ptr, resikck.slen,
	            hmac_digest);

	aka_cred.data.slen = hmac64_len =
		PJ_BASE256_TO_BASE64_LEN(PJ_ARRAY_SIZE(hmac_digest));
	pj_assert(aka_cred.data.slen+1 <= PJ_ARRAY_SIZE(tmp_buf));
	aka_cred.data.ptr = tmp_buf;
	pj_base64_encode(hmac_digest, PJ_ARRAY_SIZE(hmac_digest),
	                 aka_cred.data.ptr, &len);
	aka_cred.data.slen = hmac64_len;

	pjsip_auth_create_digest(&auth->response, &chal->nonce, 
				 &auth->nc, &auth->cnonce, &auth->qop, 
				 &auth->uri, &chal->realm, &aka_cred, method);

    } else {
	pj_assert(!"Bug!");
	return PJ_EBUG;
    }

    /* Done */
    return PJ_SUCCESS;
}


#endif	/* PJSIP_HAS_DIGEST_AKA_AUTH */

