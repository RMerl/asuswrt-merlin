#include "options.h"
#include "includes.h"
#include "dbutil.h"
#include "crypto_desc.h"
#include "ecc.h"
#include "ecdsa.h"
#include "signkey.h"

#ifdef DROPBEAR_ECDSA

int signkey_is_ecdsa(enum signkey_type type)
{
	return type == DROPBEAR_SIGNKEY_ECDSA_NISTP256
		|| type == DROPBEAR_SIGNKEY_ECDSA_NISTP384
		|| type == DROPBEAR_SIGNKEY_ECDSA_NISTP521;
}

enum signkey_type ecdsa_signkey_type(ecc_key * key) {
#ifdef DROPBEAR_ECC_256
	if (key->dp == ecc_curve_nistp256.dp) {
		return DROPBEAR_SIGNKEY_ECDSA_NISTP256;
	}
#endif
#ifdef DROPBEAR_ECC_384
	if (key->dp == ecc_curve_nistp384.dp) {
		return DROPBEAR_SIGNKEY_ECDSA_NISTP384;
	}
#endif
#ifdef DROPBEAR_ECC_521
	if (key->dp == ecc_curve_nistp521.dp) {
		return DROPBEAR_SIGNKEY_ECDSA_NISTP521;
	}
#endif
	return DROPBEAR_SIGNKEY_NONE;
}

ecc_key *gen_ecdsa_priv_key(unsigned int bit_size) {
	const ltc_ecc_set_type *dp = NULL; /* curve domain parameters */
	ecc_key *new_key = NULL;
	switch (bit_size) {
#ifdef DROPBEAR_ECC_256
		case 256:
			dp = ecc_curve_nistp256.dp;
			break;
#endif
#ifdef DROPBEAR_ECC_384
		case 384:
			dp = ecc_curve_nistp384.dp;
			break;
#endif
#ifdef DROPBEAR_ECC_521
		case 521:
			dp = ecc_curve_nistp521.dp;
			break;
#endif
	}
	if (!dp) {
		dropbear_exit("Key size %d isn't valid. Try "
#ifdef DROPBEAR_ECC_256
			"256 "
#endif
#ifdef DROPBEAR_ECC_384
			"384 "
#endif
#ifdef DROPBEAR_ECC_521
			"521 "
#endif
			, bit_size);
	}

	new_key = m_malloc(sizeof(*new_key));
	if (ecc_make_key_ex(NULL, dropbear_ltc_prng, new_key, dp) != CRYPT_OK) {
		dropbear_exit("ECC error");
	}
	return new_key;
}

ecc_key *buf_get_ecdsa_pub_key(buffer* buf) {
	unsigned char *key_ident = NULL, *identifier = NULL;
	unsigned int key_ident_len, identifier_len;
	buffer *q_buf = NULL;
	struct dropbear_ecc_curve **curve;
	ecc_key *new_key = NULL;

	/* string   "ecdsa-sha2-[identifier]" */
	key_ident = (unsigned char*)buf_getstring(buf, &key_ident_len);
	/* string   "[identifier]" */
	identifier = (unsigned char*)buf_getstring(buf, &identifier_len);

	if (key_ident_len != identifier_len + strlen("ecdsa-sha2-")) {
		TRACE(("Bad identifier lengths"))
		goto out;
	}
	if (memcmp(&key_ident[strlen("ecdsa-sha2-")], identifier, identifier_len) != 0) {
		TRACE(("mismatching identifiers"))
		goto out;
	}

	for (curve = dropbear_ecc_curves; *curve; curve++) {
		if (memcmp(identifier, (char*)(*curve)->name, strlen((char*)(*curve)->name)) == 0) {
			break;
		}
	}
	if (!*curve) {
		TRACE(("couldn't match ecc curve"))
		goto out;
	}

	/* string Q */
	q_buf = buf_getstringbuf(buf);
	new_key = buf_get_ecc_raw_pubkey(q_buf, *curve);

out:
	m_free(key_ident);
	m_free(identifier);
	if (q_buf) {
		buf_free(q_buf);
		q_buf = NULL;
	}
	TRACE(("leave buf_get_ecdsa_pub_key"))	
	return new_key;
}

ecc_key *buf_get_ecdsa_priv_key(buffer *buf) {
	ecc_key *new_key = NULL;
	TRACE(("enter buf_get_ecdsa_priv_key"))
	new_key = buf_get_ecdsa_pub_key(buf);
	if (!new_key) {
		return NULL;
	}

	if (buf_getmpint(buf, new_key->k) != DROPBEAR_SUCCESS) {
		ecc_free(new_key);
		m_free(new_key);
		return NULL;
	}

	return new_key;
}

void buf_put_ecdsa_pub_key(buffer *buf, ecc_key *key) {
	struct dropbear_ecc_curve *curve = NULL;
	char key_ident[30];

	curve = curve_for_dp(key->dp);
	snprintf(key_ident, sizeof(key_ident), "ecdsa-sha2-%s", curve->name);
	buf_putstring(buf, key_ident, strlen(key_ident));
	buf_putstring(buf, curve->name, strlen(curve->name));
	buf_put_ecc_raw_pubkey_string(buf, key);
}

void buf_put_ecdsa_priv_key(buffer *buf, ecc_key *key) {
	buf_put_ecdsa_pub_key(buf, key);
	buf_putmpint(buf, key->k);
}

void buf_put_ecdsa_sign(buffer *buf, ecc_key *key, buffer *data_buf) {
	/* Based on libtomcrypt's ecc_sign_hash but without the asn1 */
	int err = DROPBEAR_FAILURE;
	struct dropbear_ecc_curve *curve = NULL;
	hash_state hs;
	unsigned char hash[64];
	void *e = NULL, *p = NULL, *s = NULL, *r;
	char key_ident[30];
	buffer *sigbuf = NULL;

	TRACE(("buf_put_ecdsa_sign"))
	curve = curve_for_dp(key->dp);

	if (ltc_init_multi(&r, &s, &p, &e, NULL) != CRYPT_OK) { 
		goto out;
	}

	curve->hash_desc->init(&hs);
	curve->hash_desc->process(&hs, data_buf->data, data_buf->len);
	curve->hash_desc->done(&hs, hash);

	if (ltc_mp.unsigned_read(e, hash, curve->hash_desc->hashsize) != CRYPT_OK) {
		goto out;
	}

	if (ltc_mp.read_radix(p, (char *)key->dp->order, 16) != CRYPT_OK) { 
		goto out; 
	}

	for (;;) {
		ecc_key R_key; /* ephemeral key */
		if (ecc_make_key_ex(NULL, dropbear_ltc_prng, &R_key, key->dp) != CRYPT_OK) {
			goto out;
		}
		if (ltc_mp.mpdiv(R_key.pubkey.x, p, NULL, r) != CRYPT_OK) {
			goto out;
		}
		if (ltc_mp.compare_d(r, 0) == LTC_MP_EQ) {
			/* try again */
			ecc_free(&R_key);
			continue;
		}
		/* k = 1/k */
		if (ltc_mp.invmod(R_key.k, p, R_key.k) != CRYPT_OK) {
			goto out;
		}
		/* s = xr */
		if (ltc_mp.mulmod(key->k, r, p, s) != CRYPT_OK) {
			goto out;
		}
		/* s = e +  xr */
		if (ltc_mp.add(e, s, s) != CRYPT_OK) {
			goto out;
		}
		if (ltc_mp.mpdiv(s, p, NULL, s) != CRYPT_OK) {
			goto out;
		}
		/* s = (e + xr)/k */
		if (ltc_mp.mulmod(s, R_key.k, p, s) != CRYPT_OK) {
			goto out;
		}
		ecc_free(&R_key);

		if (ltc_mp.compare_d(s, 0) != LTC_MP_EQ) {
			break;
		}
	}

	snprintf(key_ident, sizeof(key_ident), "ecdsa-sha2-%s", curve->name);
	buf_putstring(buf, key_ident, strlen(key_ident));
	/* enough for nistp521 */
	sigbuf = buf_new(200);
	buf_putmpint(sigbuf, (mp_int*)r);
	buf_putmpint(sigbuf, (mp_int*)s);
	buf_putbufstring(buf, sigbuf);

	err = DROPBEAR_SUCCESS;

out:
	if (r && s && p && e) {
		ltc_deinit_multi(r, s, p, e, NULL);
	}

	if (sigbuf) {
		buf_free(sigbuf);
	}

	if (err == DROPBEAR_FAILURE) {
		dropbear_exit("ECC error");
	}
}

/* returns values in s and r
   returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
static int buf_get_ecdsa_verify_params(buffer *buf,
			void *r, void* s) {
	int ret = DROPBEAR_FAILURE;
	unsigned int sig_len;
	unsigned int sig_pos;

	sig_len = buf_getint(buf);
	sig_pos = buf->pos;
	if (buf_getmpint(buf, r) != DROPBEAR_SUCCESS) {
		goto out;
	}
	if (buf_getmpint(buf, s) != DROPBEAR_SUCCESS) {
		goto out;
	}
	if (buf->pos - sig_pos != sig_len) {
		goto out;
	}
	ret = DROPBEAR_SUCCESS;

out:
	return ret;
}


int buf_ecdsa_verify(buffer *buf, ecc_key *key, buffer *data_buf) {
	/* Based on libtomcrypt's ecc_verify_hash but without the asn1 */
	int ret = DROPBEAR_FAILURE;
	hash_state hs;
	struct dropbear_ecc_curve *curve = NULL;
	unsigned char hash[64];
	ecc_point *mG = NULL, *mQ = NULL;
	void *r = NULL, *s = NULL, *v = NULL, *w = NULL, *u1 = NULL, *u2 = NULL, 
		*e = NULL, *p = NULL, *m = NULL;
	void *mp = NULL;

	/* verify 
	 *
	 * w  = s^-1 mod n
	 * u1 = xw 
	 * u2 = rw
	 * X = u1*G + u2*Q
	 * v = X_x1 mod n
	 * accept if v == r
	 */

	TRACE(("buf_ecdsa_verify"))
	curve = curve_for_dp(key->dp);

	mG = ltc_ecc_new_point();
	mQ = ltc_ecc_new_point();
	if (ltc_init_multi(&r, &s, &v, &w, &u1, &u2, &p, &e, &m, NULL) != CRYPT_OK
		|| !mG
		|| !mQ) {
		dropbear_exit("ECC error");
	}

	if (buf_get_ecdsa_verify_params(buf, r, s) != DROPBEAR_SUCCESS) {
		goto out;
	}

	curve->hash_desc->init(&hs);
	curve->hash_desc->process(&hs, data_buf->data, data_buf->len);
	curve->hash_desc->done(&hs, hash);

	if (ltc_mp.unsigned_read(e, hash, curve->hash_desc->hashsize) != CRYPT_OK) {
		goto out;
	}

   /* get the order */
	if (ltc_mp.read_radix(p, (char *)key->dp->order, 16) != CRYPT_OK) { 
		goto out; 
	}

   /* get the modulus */
	if (ltc_mp.read_radix(m, (char *)key->dp->prime, 16) != CRYPT_OK) { 
		goto out; 
	}

   /* check for zero */
	if (ltc_mp.compare_d(r, 0) == LTC_MP_EQ 
		|| ltc_mp.compare_d(s, 0) == LTC_MP_EQ 
		|| ltc_mp.compare(r, p) != LTC_MP_LT 
		|| ltc_mp.compare(s, p) != LTC_MP_LT) {
		goto out;
	}

   /*  w  = s^-1 mod n */
	if (ltc_mp.invmod(s, p, w) != CRYPT_OK) { 
		goto out; 
	}

   /* u1 = ew */
	if (ltc_mp.mulmod(e, w, p, u1) != CRYPT_OK) { 
		goto out; 
	}

   /* u2 = rw */
	if (ltc_mp.mulmod(r, w, p, u2) != CRYPT_OK) { 
		goto out; 
	}

   /* find mG and mQ */
	if (ltc_mp.read_radix(mG->x, (char *)key->dp->Gx, 16) != CRYPT_OK) { 
		goto out; 
	}
	if (ltc_mp.read_radix(mG->y, (char *)key->dp->Gy, 16) != CRYPT_OK) { 
		goto out; 
	}
	if (ltc_mp.set_int(mG->z, 1) != CRYPT_OK) { 
		goto out; 
	}

	if (ltc_mp.copy(key->pubkey.x, mQ->x) != CRYPT_OK
		|| ltc_mp.copy(key->pubkey.y, mQ->y) != CRYPT_OK
		|| ltc_mp.copy(key->pubkey.z, mQ->z) != CRYPT_OK) { 
		goto out; 
	}

   /* compute u1*mG + u2*mQ = mG */
	if (ltc_mp.ecc_mul2add == NULL) {
		if (ltc_mp.ecc_ptmul(u1, mG, mG, m, 0) != CRYPT_OK) { 
			goto out; 
		}
		if (ltc_mp.ecc_ptmul(u2, mQ, mQ, m, 0) != CRYPT_OK) {
			goto out; 
		}

		/* find the montgomery mp */
		if (ltc_mp.montgomery_setup(m, &mp) != CRYPT_OK) { 
			goto out; 
		}

		/* add them */
		if (ltc_mp.ecc_ptadd(mQ, mG, mG, m, mp) != CRYPT_OK) { 
			goto out; 
		}

    	/* reduce */
		if (ltc_mp.ecc_map(mG, m, mp) != CRYPT_OK) { 
			goto out; 
		}
	} else {
      /* use Shamir's trick to compute u1*mG + u2*mQ using half of the doubles */
		if (ltc_mp.ecc_mul2add(mG, u1, mQ, u2, mG, m) != CRYPT_OK) { 
			goto out; 
		}
	}

   /* v = X_x1 mod n */
	if (ltc_mp.mpdiv(mG->x, p, NULL, v) != CRYPT_OK) { 
		goto out; 
	}

   /* does v == r */
	if (ltc_mp.compare(v, r) == LTC_MP_EQ) {
		ret = DROPBEAR_SUCCESS;
	}

out:
	ltc_ecc_del_point(mG);
	ltc_ecc_del_point(mQ);
	ltc_deinit_multi(r, s, v, w, u1, u2, p, e, m, NULL);
	if (mp != NULL) { 
		ltc_mp.montgomery_deinit(mp);
	}
	return ret;
}



#endif /* DROPBEAR_ECDSA */
