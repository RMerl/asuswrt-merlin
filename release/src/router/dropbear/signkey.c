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
#include "signkey.h"
#include "buffer.h"
#include "ssh.h"
#include "ecdsa.h"

static const char *signkey_names[DROPBEAR_SIGNKEY_NUM_NAMED] = {
#ifdef DROPBEAR_RSA
	"ssh-rsa",
#endif
#ifdef DROPBEAR_DSS
	"ssh-dss",
#endif
#ifdef DROPBEAR_ECDSA
	"ecdsa-sha2-nistp256",
	"ecdsa-sha2-nistp384",
	"ecdsa-sha2-nistp521"
#endif /* DROPBEAR_ECDSA */
};

/* malloc a new sign_key and set the dss and rsa keys to NULL */
sign_key * new_sign_key() {

	sign_key * ret;

	ret = (sign_key*)m_malloc(sizeof(sign_key));
	ret->type = DROPBEAR_SIGNKEY_NONE;
	ret->source = SIGNKEY_SOURCE_INVALID;
	return ret;
}

/* Returns key name corresponding to the type. Exits fatally
 * if the type is invalid */
const char* signkey_name_from_type(enum signkey_type type, unsigned int *namelen) {
	if (type >= DROPBEAR_SIGNKEY_NUM_NAMED) {
		dropbear_exit("Bad key type %d", type);
	}

	if (namelen) {
		*namelen = strlen(signkey_names[type]);
	}
	return signkey_names[type];
}

/* Returns DROPBEAR_SIGNKEY_NONE if none match */
enum signkey_type signkey_type_from_name(const char* name, unsigned int namelen) {
	int i;
	for (i = 0; i < DROPBEAR_SIGNKEY_NUM_NAMED; i++) {
		const char *fixed_name = signkey_names[i];
		if (namelen == strlen(fixed_name)
			&& memcmp(fixed_name, name, namelen) == 0) {

#ifdef DROPBEAR_ECDSA
			/* Some of the ECDSA key sizes are defined even if they're not compiled in */
			if (0
#ifndef DROPBEAR_ECC_256
				|| i == DROPBEAR_SIGNKEY_ECDSA_NISTP256
#endif
#ifndef DROPBEAR_ECC_384
				|| i == DROPBEAR_SIGNKEY_ECDSA_NISTP384
#endif
#ifndef DROPBEAR_ECC_521
				|| i == DROPBEAR_SIGNKEY_ECDSA_NISTP521
#endif
				) {
				TRACE(("attempt to use ecdsa type %d not compiled in", i))
				return DROPBEAR_SIGNKEY_NONE;
			}
#endif

			return i;
		}
	}

	TRACE(("signkey_type_from_name unexpected key type."))

	return DROPBEAR_SIGNKEY_NONE;
}

/* Returns a pointer to the key part specific to "type" */
void **
signkey_key_ptr(sign_key *key, enum signkey_type type) {
	switch (type) {
#ifdef DROPBEAR_ECDSA
#ifdef DROPBEAR_ECC_256
		case DROPBEAR_SIGNKEY_ECDSA_NISTP256:
			return (void**)&key->ecckey256;
#endif
#ifdef DROPBEAR_ECC_384
		case DROPBEAR_SIGNKEY_ECDSA_NISTP384:
			return (void**)&key->ecckey384;
#endif
#ifdef DROPBEAR_ECC_521
		case DROPBEAR_SIGNKEY_ECDSA_NISTP521:
			return (void**)&key->ecckey521;
#endif
#endif /* DROPBEAR_ECDSA */
#ifdef DROPBEAR_RSA
		case DROPBEAR_SIGNKEY_RSA:
			return (void**)&key->rsakey;
#endif
#ifdef DROPBEAR_DSS
		case DROPBEAR_SIGNKEY_DSS:
			return (void**)&key->dsskey;
#endif
		default:
			return NULL;
	}
}

/* returns DROPBEAR_SUCCESS on success, DROPBEAR_FAILURE on fail.
 * type should be set by the caller to specify the type to read, and
 * on return is set to the type read (useful when type = _ANY) */
int buf_get_pub_key(buffer *buf, sign_key *key, enum signkey_type *type) {

	char *ident;
	unsigned int len;
	enum signkey_type keytype;
	int ret = DROPBEAR_FAILURE;

	TRACE2(("enter buf_get_pub_key"))

	ident = buf_getstring(buf, &len);
	keytype = signkey_type_from_name(ident, len);
	m_free(ident);

	if (*type != DROPBEAR_SIGNKEY_ANY && *type != keytype) {
		TRACE(("buf_get_pub_key bad type - got %d, expected %d", keytype, *type))
		return DROPBEAR_FAILURE;
	}
	
	TRACE2(("buf_get_pub_key keytype is %d", keytype))

	*type = keytype;

	/* Rewind the buffer back before "ssh-rsa" etc */
	buf_incrpos(buf, -len - 4);

#ifdef DROPBEAR_DSS
	if (keytype == DROPBEAR_SIGNKEY_DSS) {
		dss_key_free(key->dsskey);
		key->dsskey = m_malloc(sizeof(*key->dsskey));
		ret = buf_get_dss_pub_key(buf, key->dsskey);
		if (ret == DROPBEAR_FAILURE) {
			m_free(key->dsskey);
		}
	}
#endif
#ifdef DROPBEAR_RSA
	if (keytype == DROPBEAR_SIGNKEY_RSA) {
		rsa_key_free(key->rsakey);
		key->rsakey = m_malloc(sizeof(*key->rsakey));
		ret = buf_get_rsa_pub_key(buf, key->rsakey);
		if (ret == DROPBEAR_FAILURE) {
			m_free(key->rsakey);
		}
	}
#endif
#ifdef DROPBEAR_ECDSA
	if (signkey_is_ecdsa(keytype)) {
		ecc_key **eck = (ecc_key**)signkey_key_ptr(key, keytype);
		if (eck) {
			if (*eck) {
				ecc_free(*eck);
				m_free(*eck);
				*eck = NULL;
			}
			*eck = buf_get_ecdsa_pub_key(buf);
			if (*eck) {
				ret = DROPBEAR_SUCCESS;
			}
		}
	}
#endif

	TRACE2(("leave buf_get_pub_key"))

	return ret;
	
}

/* returns DROPBEAR_SUCCESS on success, DROPBEAR_FAILURE on fail.
 * type should be set by the caller to specify the type to read, and
 * on return is set to the type read (useful when type = _ANY) */
int buf_get_priv_key(buffer *buf, sign_key *key, enum signkey_type *type) {

	char *ident;
	unsigned int len;
	enum signkey_type keytype;
	int ret = DROPBEAR_FAILURE;

	TRACE2(("enter buf_get_priv_key"))

	ident = buf_getstring(buf, &len);
	keytype = signkey_type_from_name(ident, len);
	m_free(ident);

	if (*type != DROPBEAR_SIGNKEY_ANY && *type != keytype) {
		TRACE(("wrong key type: %d %d", *type, keytype))
		return DROPBEAR_FAILURE;
	}

	*type = keytype;

	/* Rewind the buffer back before "ssh-rsa" etc */
	buf_incrpos(buf, -len - 4);

#ifdef DROPBEAR_DSS
	if (keytype == DROPBEAR_SIGNKEY_DSS) {
		dss_key_free(key->dsskey);
		key->dsskey = m_malloc(sizeof(*key->dsskey));
		ret = buf_get_dss_priv_key(buf, key->dsskey);
		if (ret == DROPBEAR_FAILURE) {
			m_free(key->dsskey);
		}
	}
#endif
#ifdef DROPBEAR_RSA
	if (keytype == DROPBEAR_SIGNKEY_RSA) {
		rsa_key_free(key->rsakey);
		key->rsakey = m_malloc(sizeof(*key->rsakey));
		ret = buf_get_rsa_priv_key(buf, key->rsakey);
		if (ret == DROPBEAR_FAILURE) {
			m_free(key->rsakey);
		}
	}
#endif
#ifdef DROPBEAR_ECDSA
	if (signkey_is_ecdsa(keytype)) {
		ecc_key **eck = (ecc_key**)signkey_key_ptr(key, keytype);
		if (eck) {
			if (*eck) {
				ecc_free(*eck);
				m_free(*eck);
				*eck = NULL;
			}
			*eck = buf_get_ecdsa_priv_key(buf);
			if (*eck) {
				ret = DROPBEAR_SUCCESS;
			}
		}
	}
#endif

	TRACE2(("leave buf_get_priv_key"))

	return ret;
	
}

/* type is either DROPBEAR_SIGNKEY_DSS or DROPBEAR_SIGNKEY_RSA */
void buf_put_pub_key(buffer* buf, sign_key *key, enum signkey_type type) {

	buffer *pubkeys;

	TRACE2(("enter buf_put_pub_key"))
	pubkeys = buf_new(MAX_PUBKEY_SIZE);
	
#ifdef DROPBEAR_DSS
	if (type == DROPBEAR_SIGNKEY_DSS) {
		buf_put_dss_pub_key(pubkeys, key->dsskey);
	}
#endif
#ifdef DROPBEAR_RSA
	if (type == DROPBEAR_SIGNKEY_RSA) {
		buf_put_rsa_pub_key(pubkeys, key->rsakey);
	}
#endif
#ifdef DROPBEAR_ECDSA
	if (signkey_is_ecdsa(type)) {
		ecc_key **eck = (ecc_key**)signkey_key_ptr(key, type);
		if (eck) {
			buf_put_ecdsa_pub_key(pubkeys, *eck);
		}
	}
#endif
	if (pubkeys->len == 0) {
		dropbear_exit("Bad key types in buf_put_pub_key");
	}

	buf_putbufstring(buf, pubkeys);
	buf_free(pubkeys);
	TRACE2(("leave buf_put_pub_key"))
}

/* type is either DROPBEAR_SIGNKEY_DSS or DROPBEAR_SIGNKEY_RSA */
void buf_put_priv_key(buffer* buf, sign_key *key, enum signkey_type type) {

	TRACE(("enter buf_put_priv_key"))
	TRACE(("type is %d", type))

#ifdef DROPBEAR_DSS
	if (type == DROPBEAR_SIGNKEY_DSS) {
		buf_put_dss_priv_key(buf, key->dsskey);
	TRACE(("leave buf_put_priv_key: dss done"))
	return;
	}
#endif
#ifdef DROPBEAR_RSA
	if (type == DROPBEAR_SIGNKEY_RSA) {
		buf_put_rsa_priv_key(buf, key->rsakey);
	TRACE(("leave buf_put_priv_key: rsa done"))
	return;
	}
#endif
#ifdef DROPBEAR_ECDSA
	if (signkey_is_ecdsa(type)) {
		ecc_key **eck = (ecc_key**)signkey_key_ptr(key, type);
		if (eck) {
			buf_put_ecdsa_priv_key(buf, *eck);
			TRACE(("leave buf_put_priv_key: ecdsa done"))
			return;
		}
	}
#endif
	dropbear_exit("Bad key types in put pub key");
}

void sign_key_free(sign_key *key) {

	TRACE2(("enter sign_key_free"))

#ifdef DROPBEAR_DSS
	dss_key_free(key->dsskey);
	key->dsskey = NULL;
#endif
#ifdef DROPBEAR_RSA
	rsa_key_free(key->rsakey);
	key->rsakey = NULL;
#endif
#ifdef DROPBEAR_ECDSA
#ifdef DROPBEAR_ECC_256
	if (key->ecckey256) {
		ecc_free(key->ecckey256);
		m_free(key->ecckey256);
		key->ecckey256 = NULL;
	}
#endif
#ifdef DROPBEAR_ECC_384
	if (key->ecckey384) {
		ecc_free(key->ecckey384);
		m_free(key->ecckey384);
		key->ecckey384 = NULL;
	}
#endif
#ifdef DROPBEAR_ECC_521
	if (key->ecckey521) {
		ecc_free(key->ecckey521);
		m_free(key->ecckey521);
		key->ecckey521 = NULL;
	}
#endif
#endif

	m_free(key->filename);

	m_free(key);
	TRACE2(("leave sign_key_free"))
}

static char hexdig(unsigned char x) {
	if (x > 0xf)
		return 'X';

	if (x < 10)
		return '0' + x;
	else
		return 'a' + x - 10;
}

/* Since we're not sure if we'll have md5 or sha1, we present both.
 * MD5 is used in preference, but sha1 could still be useful */
#ifdef DROPBEAR_MD5_HMAC
static char * sign_key_md5_fingerprint(unsigned char* keyblob,
		unsigned int keybloblen) {

	char * ret;
	hash_state hs;
	unsigned char hash[MD5_HASH_SIZE];
	unsigned int i;
	unsigned int buflen;

	md5_init(&hs);

	/* skip the size int of the string - this is a bit messy */
	md5_process(&hs, keyblob, keybloblen);

	md5_done(&hs, hash);

	/* "md5 hexfingerprinthere\0", each hex digit is "AB:" etc */
	buflen = 4 + 3*MD5_HASH_SIZE;
	ret = (char*)m_malloc(buflen);

	memset(ret, 'Z', buflen);
	strcpy(ret, "md5 ");

	for (i = 0; i < MD5_HASH_SIZE; i++) {
		unsigned int pos = 4 + i*3;
		ret[pos] = hexdig(hash[i] >> 4);
		ret[pos+1] = hexdig(hash[i] & 0x0f);
		ret[pos+2] = ':';
	}
	ret[buflen-1] = 0x0;

	return ret;
}

#else /* use SHA1 rather than MD5 for fingerprint */
static char * sign_key_sha1_fingerprint(unsigned char* keyblob, 
		unsigned int keybloblen) {

	char * ret;
	hash_state hs;
	unsigned char hash[SHA1_HASH_SIZE];
	unsigned int i;
	unsigned int buflen;

	sha1_init(&hs);

	/* skip the size int of the string - this is a bit messy */
	sha1_process(&hs, keyblob, keybloblen);

	sha1_done(&hs, hash);

	/* "sha1!! hexfingerprinthere\0", each hex digit is "AB:" etc */
	buflen = 7 + 3*SHA1_HASH_SIZE;
	ret = (char*)m_malloc(buflen);

	strcpy(ret, "sha1!! ");

	for (i = 0; i < SHA1_HASH_SIZE; i++) {
		unsigned int pos = 7 + 3*i;
		ret[pos] = hexdig(hash[i] >> 4);
		ret[pos+1] = hexdig(hash[i] & 0x0f);
		ret[pos+2] = ':';
	}
	ret[buflen-1] = 0x0;

	return ret;
}

#endif /* MD5/SHA1 switch */

/* This will return a freshly malloced string, containing a fingerprint
 * in either sha1 or md5 */
char * sign_key_fingerprint(unsigned char* keyblob, unsigned int keybloblen) {

#ifdef DROPBEAR_MD5_HMAC
	return sign_key_md5_fingerprint(keyblob, keybloblen);
#else
	return sign_key_sha1_fingerprint(keyblob, keybloblen);
#endif
}

void buf_put_sign(buffer* buf, sign_key *key, enum signkey_type type, 
	buffer *data_buf) {
	buffer *sigblob;
	sigblob = buf_new(MAX_PUBKEY_SIZE);

#ifdef DROPBEAR_DSS
	if (type == DROPBEAR_SIGNKEY_DSS) {
		buf_put_dss_sign(sigblob, key->dsskey, data_buf);
	}
#endif
#ifdef DROPBEAR_RSA
	if (type == DROPBEAR_SIGNKEY_RSA) {
		buf_put_rsa_sign(sigblob, key->rsakey, data_buf);
	}
#endif
#ifdef DROPBEAR_ECDSA
	if (signkey_is_ecdsa(type)) {
		ecc_key **eck = (ecc_key**)signkey_key_ptr(key, type);
		if (eck) {
			buf_put_ecdsa_sign(sigblob, *eck, data_buf);
		}
	}
#endif
	if (sigblob->len == 0) {
		dropbear_exit("Non-matching signing type");
	}
	buf_putbufstring(buf, sigblob);
	buf_free(sigblob);

}

#ifdef DROPBEAR_SIGNKEY_VERIFY
/* Return DROPBEAR_SUCCESS or DROPBEAR_FAILURE.
 * If FAILURE is returned, the position of
 * buf is undefined. If SUCCESS is returned, buf will be positioned after the
 * signature blob */
int buf_verify(buffer * buf, sign_key *key, buffer *data_buf) {
	
	char *type_name = NULL;
	unsigned int type_name_len = 0;
	enum signkey_type type;

	TRACE(("enter buf_verify"))

	buf_getint(buf); /* blob length */
	type_name = buf_getstring(buf, &type_name_len);
	type = signkey_type_from_name(type_name, type_name_len);
	m_free(type_name);

#ifdef DROPBEAR_DSS
	if (type == DROPBEAR_SIGNKEY_DSS) {
		if (key->dsskey == NULL) {
			dropbear_exit("No DSS key to verify signature");
		}
		return buf_dss_verify(buf, key->dsskey, data_buf);
	}
#endif

#ifdef DROPBEAR_RSA
	if (type == DROPBEAR_SIGNKEY_RSA) {
		if (key->rsakey == NULL) {
			dropbear_exit("No RSA key to verify signature");
		}
		return buf_rsa_verify(buf, key->rsakey, data_buf);
	}
#endif
#ifdef DROPBEAR_ECDSA
	if (signkey_is_ecdsa(type)) {
		ecc_key **eck = (ecc_key**)signkey_key_ptr(key, type);
		if (eck) {
			return buf_ecdsa_verify(buf, *eck, data_buf);
		}
	}
#endif

	dropbear_exit("Non-matching signing type");
	return DROPBEAR_FAILURE;
}
#endif /* DROPBEAR_SIGNKEY_VERIFY */

#ifdef DROPBEAR_KEY_LINES /* ie we're using authorized_keys or known_hosts */

/* Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE when given a buffer containing
 * a key, a key, and a type. The buffer is positioned at the start of the
 * base64 data, and contains no trailing data */
/* If fingerprint is non-NULL, it will be set to a malloc()ed fingerprint
   of the key if it is successfully decoded */
int cmp_base64_key(const unsigned char* keyblob, unsigned int keybloblen, 
					const unsigned char* algoname, unsigned int algolen, 
					buffer * line, char ** fingerprint) {

	buffer * decodekey = NULL;
	int ret = DROPBEAR_FAILURE;
	unsigned int len, filealgolen;
	unsigned long decodekeylen;
	unsigned char* filealgo = NULL;

	/* now we have the actual data */
	len = line->len - line->pos;
	decodekeylen = len * 2; /* big to be safe */
	decodekey = buf_new(decodekeylen);

	if (base64_decode(buf_getptr(line, len), len,
				buf_getwriteptr(decodekey, decodekey->size),
				&decodekeylen) != CRYPT_OK) {
		TRACE(("checkpubkey: base64 decode failed"))
		goto out;
	}
	TRACE(("checkpubkey: base64_decode success"))
	buf_incrlen(decodekey, decodekeylen);
	
	if (fingerprint) {
		*fingerprint = sign_key_fingerprint(buf_getptr(decodekey, decodekeylen),
											decodekeylen);
	}
	
	/* compare the keys */
	if ( ( decodekeylen != keybloblen )
			|| memcmp( buf_getptr(decodekey, decodekey->len),
						keyblob, decodekey->len) != 0) {
		TRACE(("checkpubkey: compare failed"))
		goto out;
	}

	/* ... and also check that the algo specified and the algo in the key
	 * itself match */
	filealgolen = buf_getint(decodekey);
	filealgo = buf_getptr(decodekey, filealgolen);
	if (filealgolen != algolen || memcmp(filealgo, algoname, algolen) != 0) {
		TRACE(("checkpubkey: algo match failed")) 
		goto out;
	}

	/* All checks passed */
	ret = DROPBEAR_SUCCESS;

out:
	buf_free(decodekey);
	decodekey = NULL;
	return ret;
}
#endif
