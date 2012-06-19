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

/* Perform RSA operations on data, including reading keys, signing and
 * verification.
 *
 * The format is specified in rfc2437, Applied Cryptography or The Handbook of
 * Applied Cryptography detail the general algorithm. */

#include "includes.h"
#include "dbutil.h"
#include "bignum.h"
#include "rsa.h"
#include "buffer.h"
#include "ssh.h"
#include "random.h"

#ifdef DROPBEAR_RSA 

static void rsa_pad_em(dropbear_rsa_key * key,
		const unsigned char * data, unsigned int len,
		mp_int * rsa_em);

/* Load a public rsa key from a buffer, initialising the values.
 * The key will have the same format as buf_put_rsa_key.
 * These should be freed with rsa_key_free.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_get_rsa_pub_key(buffer* buf, dropbear_rsa_key *key) {

    int ret = DROPBEAR_FAILURE;
	TRACE(("enter buf_get_rsa_pub_key"))
	dropbear_assert(key != NULL);
	key->e = m_malloc(sizeof(mp_int));
	key->n = m_malloc(sizeof(mp_int));
	m_mp_init_multi(key->e, key->n, NULL);
	key->d = NULL;
	key->p = NULL;
	key->q = NULL;

	buf_incrpos(buf, 4+SSH_SIGNKEY_RSA_LEN); /* int + "ssh-rsa" */

	if (buf_getmpint(buf, key->e) == DROPBEAR_FAILURE
	 || buf_getmpint(buf, key->n) == DROPBEAR_FAILURE) {
		TRACE(("leave buf_get_rsa_pub_key: failure"))
	    goto out;
	}

	if (mp_count_bits(key->n) < MIN_RSA_KEYLEN) {
		dropbear_log(LOG_WARNING, "RSA key too short");
	    goto out;
	}

	TRACE(("leave buf_get_rsa_pub_key: success"))
    ret = DROPBEAR_SUCCESS;
out:
    if (ret == DROPBEAR_FAILURE) {
        m_free(key->e);
        m_free(key->n);
    }
	return ret;
}

/* Same as buf_get_rsa_pub_key, but reads private bits at the end.
 * Loads a private rsa key from a buffer
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_get_rsa_priv_key(buffer* buf, dropbear_rsa_key *key) {
    int ret = DROPBEAR_FAILURE;

	TRACE(("enter buf_get_rsa_priv_key"))
	dropbear_assert(key != NULL);

	if (buf_get_rsa_pub_key(buf, key) == DROPBEAR_FAILURE) {
		TRACE(("leave buf_get_rsa_priv_key: pub: ret == DROPBEAR_FAILURE"))
		return DROPBEAR_FAILURE;
	}
	
	key->d = NULL;
	key->p = NULL;
	key->q = NULL;

	key->d = m_malloc(sizeof(mp_int));
	m_mp_init(key->d);
	if (buf_getmpint(buf, key->d) == DROPBEAR_FAILURE) {
		TRACE(("leave buf_get_rsa_priv_key: d: ret == DROPBEAR_FAILURE"))
	    goto out;
	}

	if (buf->pos == buf->len) {
    	/* old Dropbear private keys didn't keep p and q, so we will ignore them*/
	} else {
		key->p = m_malloc(sizeof(mp_int));
		key->q = m_malloc(sizeof(mp_int));
		m_mp_init_multi(key->p, key->q, NULL);

		if (buf_getmpint(buf, key->p) == DROPBEAR_FAILURE) {
			TRACE(("leave buf_get_rsa_priv_key: p: ret == DROPBEAR_FAILURE"))
		    goto out;
		}

		if (buf_getmpint(buf, key->q) == DROPBEAR_FAILURE) {
			TRACE(("leave buf_get_rsa_priv_key: q: ret == DROPBEAR_FAILURE"))
		    goto out;
		}
	}

    ret = DROPBEAR_SUCCESS;
out:
    if (ret == DROPBEAR_FAILURE) {
        m_free(key->d);
        m_free(key->p);
        m_free(key->q);
    }
	TRACE(("leave buf_get_rsa_priv_key"))
    return ret;
}
	

/* Clear and free the memory used by a public or private key */
void rsa_key_free(dropbear_rsa_key *key) {

	TRACE(("enter rsa_key_free"))

	if (key == NULL) {
		TRACE(("leave rsa_key_free: key == NULL"))
		return;
	}
	if (key->d) {
		mp_clear(key->d);
		m_free(key->d);
	}
	if (key->e) {
		mp_clear(key->e);
		m_free(key->e);
	}
	if (key->n) {
		 mp_clear(key->n);
		 m_free(key->n);
	}
	if (key->p) {
		mp_clear(key->p);
		m_free(key->p);
	}
	if (key->q) {
		mp_clear(key->q);
		m_free(key->q);
	}
	m_free(key);
	TRACE(("leave rsa_key_free"))
}

/* Put the public rsa key into the buffer in the required format:
 *
 * string	"ssh-rsa"
 * mp_int	e
 * mp_int	n
 */
void buf_put_rsa_pub_key(buffer* buf, dropbear_rsa_key *key) {

	TRACE(("enter buf_put_rsa_pub_key"))
	dropbear_assert(key != NULL);

	buf_putstring(buf, SSH_SIGNKEY_RSA, SSH_SIGNKEY_RSA_LEN);
	buf_putmpint(buf, key->e);
	buf_putmpint(buf, key->n);

	TRACE(("leave buf_put_rsa_pub_key"))

}

/* Same as buf_put_rsa_pub_key, but with the private "x" key appended */
void buf_put_rsa_priv_key(buffer* buf, dropbear_rsa_key *key) {

	TRACE(("enter buf_put_rsa_priv_key"))

	dropbear_assert(key != NULL);
	buf_put_rsa_pub_key(buf, key);
	buf_putmpint(buf, key->d);

	/* new versions have p and q, old versions don't */
	if (key->p) {
		buf_putmpint(buf, key->p);
	}
	if (key->q) {
		buf_putmpint(buf, key->q);
	}


	TRACE(("leave buf_put_rsa_priv_key"))

}

#ifdef DROPBEAR_SIGNKEY_VERIFY
/* Verify a signature in buf, made on data by the key given.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_rsa_verify(buffer * buf, dropbear_rsa_key *key, const unsigned char* data,
		unsigned int len) {

	unsigned int slen;
	DEF_MP_INT(rsa_s);
	DEF_MP_INT(rsa_mdash);
	DEF_MP_INT(rsa_em);
	int ret = DROPBEAR_FAILURE;

	TRACE(("enter buf_rsa_verify"))

	dropbear_assert(key != NULL);

	m_mp_init_multi(&rsa_mdash, &rsa_s, &rsa_em, NULL);

	slen = buf_getint(buf);
	if (slen != (unsigned int)mp_unsigned_bin_size(key->n)) {
		TRACE(("bad size"))
		goto out;
	}

	if (mp_read_unsigned_bin(&rsa_s, buf_getptr(buf, buf->len - buf->pos),
				buf->len - buf->pos) != MP_OKAY) {
		TRACE(("failed reading rsa_s"))
		goto out;
	}

	/* check that s <= n-1 */
	if (mp_cmp(&rsa_s, key->n) != MP_LT) {
		TRACE(("s > n-1"))
		goto out;
	}

	/* create the magic PKCS padded value */
	rsa_pad_em(key, data, len, &rsa_em);

	if (mp_exptmod(&rsa_s, key->e, key->n, &rsa_mdash) != MP_OKAY) {
		TRACE(("failed exptmod rsa_s"))
		goto out;
	}

	if (mp_cmp(&rsa_em, &rsa_mdash) == MP_EQ) {
		/* signature is valid */
		TRACE(("success!"))
		ret = DROPBEAR_SUCCESS;
	}

out:
	mp_clear_multi(&rsa_mdash, &rsa_s, &rsa_em, NULL);
	TRACE(("leave buf_rsa_verify: ret %d", ret))
	return ret;
}

#endif /* DROPBEAR_SIGNKEY_VERIFY */

/* Sign the data presented with key, writing the signature contents
 * to the buffer */
void buf_put_rsa_sign(buffer* buf, dropbear_rsa_key *key, const unsigned char* data,
		unsigned int len) {

	unsigned int nsize, ssize;
	unsigned int i;
	DEF_MP_INT(rsa_s);
	DEF_MP_INT(rsa_tmp1);
	DEF_MP_INT(rsa_tmp2);
	DEF_MP_INT(rsa_tmp3);
	
	TRACE(("enter buf_put_rsa_sign"))
	dropbear_assert(key != NULL);

	m_mp_init_multi(&rsa_s, &rsa_tmp1, &rsa_tmp2, &rsa_tmp3, NULL);

	rsa_pad_em(key, data, len, &rsa_tmp1);

	/* the actual signing of the padded data */

#ifdef RSA_BLINDING

	/* With blinding, s = (r^(-1))((em)*r^e)^d mod n */

	/* generate the r blinding value */
	/* rsa_tmp2 is r */
	gen_random_mpint(key->n, &rsa_tmp2);

	/* rsa_tmp1 is em */
	/* em' = em * r^e mod n */

	/* rsa_s used as a temp var*/
	if (mp_exptmod(&rsa_tmp2, key->e, key->n, &rsa_s) != MP_OKAY) {
		dropbear_exit("RSA error");
	}
	if (mp_invmod(&rsa_tmp2, key->n, &rsa_tmp3) != MP_OKAY) {
		dropbear_exit("RSA error");
	}
	if (mp_mulmod(&rsa_tmp1, &rsa_s, key->n, &rsa_tmp2) != MP_OKAY) {
		dropbear_exit("RSA error");
	}

	/* rsa_tmp2 is em' */
	/* s' = (em')^d mod n */
	if (mp_exptmod(&rsa_tmp2, key->d, key->n, &rsa_tmp1) != MP_OKAY) {
		dropbear_exit("RSA error");
	}

	/* rsa_tmp1 is s' */
	/* rsa_tmp3 is r^(-1) mod n */
	/* s = (s')r^(-1) mod n */
	if (mp_mulmod(&rsa_tmp1, &rsa_tmp3, key->n, &rsa_s) != MP_OKAY) {
		dropbear_exit("RSA error");
	}

#else

	/* s = em^d mod n */
	/* rsa_tmp1 is em */
	if (mp_exptmod(&rsa_tmp1, key->d, key->n, &rsa_s) != MP_OKAY) {
		dropbear_exit("RSA error");
	}

#endif /* RSA_BLINDING */

	mp_clear_multi(&rsa_tmp1, &rsa_tmp2, &rsa_tmp3, NULL);
	
	/* create the signature to return */
	buf_putstring(buf, SSH_SIGNKEY_RSA, SSH_SIGNKEY_RSA_LEN);

	nsize = mp_unsigned_bin_size(key->n);

	/* string rsa_signature_blob length */
	buf_putint(buf, nsize);
	/* pad out s to same length as n */
	ssize = mp_unsigned_bin_size(&rsa_s);
	dropbear_assert(ssize <= nsize);
	for (i = 0; i < nsize-ssize; i++) {
		buf_putbyte(buf, 0x00);
	}

	if (mp_to_unsigned_bin(&rsa_s, buf_getwriteptr(buf, ssize)) != MP_OKAY) {
		dropbear_exit("RSA error");
	}
	buf_incrwritepos(buf, ssize);
	mp_clear(&rsa_s);

#if defined(DEBUG_RSA) && defined(DEBUG_TRACE)
	printhex("RSA sig", buf->data, buf->len);
#endif
	

	TRACE(("leave buf_put_rsa_sign"))
}

/* Creates the message value as expected by PKCS, see rfc2437 etc */
/* format to be padded to is:
 * EM = 01 | FF* | 00 | prefix | hash
 *
 * where FF is repeated enough times to make EM one byte
 * shorter than the size of key->n
 *
 * prefix is the ASN1 designator prefix,
 * hex 30 21 30 09 06 05 2B 0E 03 02 1A 05 00 04 14
 *
 * rsa_em must be a pointer to an initialised mp_int.
 */
static void rsa_pad_em(dropbear_rsa_key * key,
		const unsigned char * data, unsigned int len, 
		mp_int * rsa_em) {

	/* ASN1 designator (including the 0x00 preceding) */
	const unsigned char rsa_asn1_magic[] = 
		{0x00, 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 
		 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
	const unsigned int RSA_ASN1_MAGIC_LEN = 16;

	buffer * rsa_EM = NULL;
	hash_state hs;
	unsigned int nsize;
	
	dropbear_assert(key != NULL);
	dropbear_assert(data != NULL);
	nsize = mp_unsigned_bin_size(key->n);

	rsa_EM = buf_new(nsize-1);
	/* type byte */
	buf_putbyte(rsa_EM, 0x01);
	/* Padding with 0xFF bytes */
	while(rsa_EM->pos != rsa_EM->size - RSA_ASN1_MAGIC_LEN - SHA1_HASH_SIZE) {
		buf_putbyte(rsa_EM, 0xff);
	}
	/* Magic ASN1 stuff */
	memcpy(buf_getwriteptr(rsa_EM, RSA_ASN1_MAGIC_LEN),
			rsa_asn1_magic, RSA_ASN1_MAGIC_LEN);
	buf_incrwritepos(rsa_EM, RSA_ASN1_MAGIC_LEN);

	/* The hash of the data */
	sha1_init(&hs);
	sha1_process(&hs, data, len);
	sha1_done(&hs, buf_getwriteptr(rsa_EM, SHA1_HASH_SIZE));
	buf_incrwritepos(rsa_EM, SHA1_HASH_SIZE);

	dropbear_assert(rsa_EM->pos == rsa_EM->size);

	/* Create the mp_int from the encoded bytes */
	buf_setpos(rsa_EM, 0);
	bytes_to_mp(rsa_em, buf_getptr(rsa_EM, rsa_EM->size),
			rsa_EM->size);
	buf_free(rsa_EM);
}

#endif /* DROPBEAR_RSA */
