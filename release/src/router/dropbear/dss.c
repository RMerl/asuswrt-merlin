/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "dbutil.h"
#include "bignum.h"
#include "dss.h"
#include "buffer.h"
#include "ssh.h"
#include "dbrandom.h"

/* Handle DSS (Digital Signature Standard), aka DSA (D.S. Algorithm),
 * operations, such as key reading, signing, verification. Key generation
 * is in gendss.c, since it isn't required in the server itself.
 *
 * See FIPS186 or the Handbook of Applied Cryptography for details of the
 * algorithm */

#ifdef DROPBEAR_DSS 

/* Load a dss key from a buffer, initialising the values.
 * The key will have the same format as buf_put_dss_key.
 * These should be freed with dss_key_free.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_get_dss_pub_key(buffer* buf, dropbear_dss_key *key) {

	TRACE(("enter buf_get_dss_pub_key"))
	dropbear_assert(key != NULL);
	m_mp_alloc_init_multi(&key->p, &key->q, &key->g, &key->y, NULL);
	key->x = NULL;

	buf_incrpos(buf, 4+SSH_SIGNKEY_DSS_LEN); /* int + "ssh-dss" */
	if (buf_getmpint(buf, key->p) == DROPBEAR_FAILURE
	 || buf_getmpint(buf, key->q) == DROPBEAR_FAILURE
	 || buf_getmpint(buf, key->g) == DROPBEAR_FAILURE
	 || buf_getmpint(buf, key->y) == DROPBEAR_FAILURE) {
		TRACE(("leave buf_get_dss_pub_key: failed reading mpints"))
		return DROPBEAR_FAILURE;
	}

	if (mp_count_bits(key->p) < MIN_DSS_KEYLEN) {
		dropbear_log(LOG_WARNING, "DSS key too short");
		TRACE(("leave buf_get_dss_pub_key: short key"))
		return DROPBEAR_FAILURE;
	}

	TRACE(("leave buf_get_dss_pub_key: success"))
	return DROPBEAR_SUCCESS;
}

/* Same as buf_get_dss_pub_key, but reads a private "x" key at the end.
 * Loads a private dss key from a buffer
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_get_dss_priv_key(buffer* buf, dropbear_dss_key *key) {

	int ret = DROPBEAR_FAILURE;

	dropbear_assert(key != NULL);

	ret = buf_get_dss_pub_key(buf, key);
	if (ret == DROPBEAR_FAILURE) {
		return DROPBEAR_FAILURE;
	}

	m_mp_alloc_init_multi(&key->x, NULL);
	ret = buf_getmpint(buf, key->x);
	if (ret == DROPBEAR_FAILURE) {
		m_free(key->x);
	}

	return ret;
}
	

/* Clear and free the memory used by a public or private key */
void dss_key_free(dropbear_dss_key *key) {

	TRACE2(("enter dsa_key_free"))
	if (key == NULL) {
		TRACE2(("enter dsa_key_free: key == NULL"))
		return;
	}
	if (key->p) {
		mp_clear(key->p);
		m_free(key->p);
	}
	if (key->q) {
		mp_clear(key->q);
		m_free(key->q);
	}
	if (key->g) {
		mp_clear(key->g);
		m_free(key->g);
	}
	if (key->y) {
		mp_clear(key->y);
		m_free(key->y);
	}
	if (key->x) {
		mp_clear(key->x);
		m_free(key->x);
	}
	m_free(key);
	TRACE2(("leave dsa_key_free"))
}

/* put the dss public key into the buffer in the required format:
 *
 * string	"ssh-dss"
 * mpint	p
 * mpint	q
 * mpint	g
 * mpint	y
 */
void buf_put_dss_pub_key(buffer* buf, dropbear_dss_key *key) {

	dropbear_assert(key != NULL);
	buf_putstring(buf, SSH_SIGNKEY_DSS, SSH_SIGNKEY_DSS_LEN);
	buf_putmpint(buf, key->p);
	buf_putmpint(buf, key->q);
	buf_putmpint(buf, key->g);
	buf_putmpint(buf, key->y);

}

/* Same as buf_put_dss_pub_key, but with the private "x" key appended */
void buf_put_dss_priv_key(buffer* buf, dropbear_dss_key *key) {

	dropbear_assert(key != NULL);
	buf_put_dss_pub_key(buf, key);
	buf_putmpint(buf, key->x);

}

#ifdef DROPBEAR_SIGNKEY_VERIFY
/* Verify a DSS signature (in buf) made on data by the key given. 
 * returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_dss_verify(buffer* buf, dropbear_dss_key *key, buffer *data_buf) {
	unsigned char msghash[SHA1_HASH_SIZE];
	hash_state hs;
	int ret = DROPBEAR_FAILURE;
	DEF_MP_INT(val1);
	DEF_MP_INT(val2);
	DEF_MP_INT(val3);
	DEF_MP_INT(val4);
	char * string = NULL;
	int stringlen;

	TRACE(("enter buf_dss_verify"))
	dropbear_assert(key != NULL);

	m_mp_init_multi(&val1, &val2, &val3, &val4, NULL);

	/* get blob, check length */
	string = buf_getstring(buf, &stringlen);
	if (stringlen != 2*SHA1_HASH_SIZE) {
		goto out;
	}

	/* hash the data */
	sha1_init(&hs);
	sha1_process(&hs, data_buf->data, data_buf->len);
	sha1_done(&hs, msghash);

	/* create the signature - s' and r' are the received signatures in buf */
	/* w = (s')-1 mod q */
	/* let val1 = s' */
	bytes_to_mp(&val1, &string[SHA1_HASH_SIZE], SHA1_HASH_SIZE);

	if (mp_cmp(&val1, key->q) != MP_LT) {
		TRACE(("verify failed, s' >= q"))
		goto out;
	}
	/* let val2 = w = (s')^-1 mod q*/
	if (mp_invmod(&val1, key->q, &val2) != MP_OKAY) {
		goto out;
	}

	/* u1 = ((SHA(M')w) mod q */
	/* let val1 = SHA(M') = msghash */
	bytes_to_mp(&val1, msghash, SHA1_HASH_SIZE);

	/* let val3 = u1 = ((SHA(M')w) mod q */
	if (mp_mulmod(&val1, &val2, key->q, &val3) != MP_OKAY) {
		goto out;
	}

	/* u2 = ((r')w) mod q */
	/* let val1 = r' */
	bytes_to_mp(&val1, &string[0], SHA1_HASH_SIZE);
	if (mp_cmp(&val1, key->q) != MP_LT) {
		TRACE(("verify failed, r' >= q"))
		goto out;
	}
	/* let val4 = u2 = ((r')w) mod q */
	if (mp_mulmod(&val1, &val2, key->q, &val4) != MP_OKAY) {
		goto out;
	}

	/* v = (((g)^u1 (y)^u2) mod p) mod q */
	/* val2 = g^u1 mod p */
	if (mp_exptmod(key->g, &val3, key->p, &val2) != MP_OKAY) {
		goto out;
	}
	/* val3 = y^u2 mod p */
	if (mp_exptmod(key->y, &val4, key->p, &val3) != MP_OKAY) {
		goto out;
	}
	/* val4 = ((g)^u1 (y)^u2) mod p */
	if (mp_mulmod(&val2, &val3, key->p, &val4) != MP_OKAY) {
		goto out;
	}
	/* val2 = v = (((g)^u1 (y)^u2) mod p) mod q */
	if (mp_mod(&val4, key->q, &val2) != MP_OKAY) {
		goto out;
	}
	
	/* check whether signatures verify */
	if (mp_cmp(&val2, &val1) == MP_EQ) {
		/* good sig */
		ret = DROPBEAR_SUCCESS;
	}

out:
	mp_clear_multi(&val1, &val2, &val3, &val4, NULL);
	m_free(string);

	return ret;

}
#endif /* DROPBEAR_SIGNKEY_VERIFY */

/* Sign the data presented with key, writing the signature contents
 * to the buffer */
void buf_put_dss_sign(buffer* buf, dropbear_dss_key *key, buffer *data_buf) {
	unsigned char msghash[SHA1_HASH_SIZE];
	unsigned int writelen;
	unsigned int i;
	DEF_MP_INT(dss_k);
	DEF_MP_INT(dss_m);
	DEF_MP_INT(dss_temp1);
	DEF_MP_INT(dss_temp2);
	DEF_MP_INT(dss_r);
	DEF_MP_INT(dss_s);
	hash_state hs;
	
	TRACE(("enter buf_put_dss_sign"))
	dropbear_assert(key != NULL);
	
	/* hash the data */
	sha1_init(&hs);
	sha1_process(&hs, data_buf->data, data_buf->len);
	sha1_done(&hs, msghash);

	m_mp_init_multi(&dss_k, &dss_temp1, &dss_temp2, &dss_r, &dss_s,
			&dss_m, NULL);
	/* the random number generator's input has included the private key which
	 * avoids DSS's problem of private key exposure due to low entropy */
	gen_random_mpint(key->q, &dss_k);

	/* now generate the actual signature */
	bytes_to_mp(&dss_m, msghash, SHA1_HASH_SIZE);

	/* g^k mod p */
	if (mp_exptmod(key->g, &dss_k, key->p, &dss_temp1) !=  MP_OKAY) {
		dropbear_exit("DSS error");
	}
	/* r = (g^k mod p) mod q */
	if (mp_mod(&dss_temp1, key->q, &dss_r) != MP_OKAY) {
		dropbear_exit("DSS error");
	}

	/* x*r mod q */
	if (mp_mulmod(&dss_r, key->x, key->q, &dss_temp1) != MP_OKAY) {
		dropbear_exit("DSS error");
	}
	/* (SHA1(M) + xr) mod q) */
	if (mp_addmod(&dss_m, &dss_temp1, key->q, &dss_temp2) != MP_OKAY) {
		dropbear_exit("DSS error");
	}
	
	/* (k^-1) mod q */
	if (mp_invmod(&dss_k, key->q, &dss_temp1) != MP_OKAY) {
		dropbear_exit("DSS error");
	}

	/* s = (k^-1(SHA1(M) + xr)) mod q */
	if (mp_mulmod(&dss_temp1, &dss_temp2, key->q, &dss_s) != MP_OKAY) {
		dropbear_exit("DSS error");
	}

	buf_putstring(buf, SSH_SIGNKEY_DSS, SSH_SIGNKEY_DSS_LEN);
	buf_putint(buf, 2*SHA1_HASH_SIZE);

	writelen = mp_unsigned_bin_size(&dss_r);
	dropbear_assert(writelen <= SHA1_HASH_SIZE);
	/* need to pad to 160 bits with leading zeros */
	for (i = 0; i < SHA1_HASH_SIZE - writelen; i++) {
		buf_putbyte(buf, 0);
	}
	if (mp_to_unsigned_bin(&dss_r, buf_getwriteptr(buf, writelen)) 
			!= MP_OKAY) {
		dropbear_exit("DSS error");
	}
	mp_clear(&dss_r);
	buf_incrwritepos(buf, writelen);

	writelen = mp_unsigned_bin_size(&dss_s);
	dropbear_assert(writelen <= SHA1_HASH_SIZE);
	/* need to pad to 160 bits with leading zeros */
	for (i = 0; i < SHA1_HASH_SIZE - writelen; i++) {
		buf_putbyte(buf, 0);
	}
	if (mp_to_unsigned_bin(&dss_s, buf_getwriteptr(buf, writelen)) 
			!= MP_OKAY) {
		dropbear_exit("DSS error");
	}
	mp_clear(&dss_s);
	buf_incrwritepos(buf, writelen);

	mp_clear_multi(&dss_k, &dss_temp1, &dss_temp2, &dss_r, &dss_s,
			&dss_m, NULL);
	
	/* create the signature to return */

	TRACE(("leave buf_put_dss_sign"))
}

#endif /* DROPBEAR_DSS */
