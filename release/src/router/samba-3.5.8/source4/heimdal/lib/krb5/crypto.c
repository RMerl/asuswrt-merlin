/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define KRB5_DEPRECATED

#include "krb5_locl.h"
#include <pkinit_asn1.h>

#define WEAK_ENCTYPES 1

#ifndef HEIMDAL_SMALLER
#define DES3_OLD_ENCTYPE 1
#endif


#ifdef HAVE_OPENSSL /* XXX forward decl for hcrypto glue */
const EVP_CIPHER * _krb5_EVP_hcrypto_aes_128_cts(void);
const EVP_CIPHER * _krb5_EVP_hcrypto_aes_256_cts(void);
#define EVP_hcrypto_aes_128_cts _krb5_EVP_hcrypto_aes_128_cts
#define EVP_hcrypto_aes_256_cts _krb5_EVP_hcrypto_aes_256_cts
#endif

struct key_data {
    krb5_keyblock *key;
    krb5_data *schedule;
};

struct key_usage {
    unsigned usage;
    struct key_data key;
};

struct krb5_crypto_data {
    struct encryption_type *et;
    struct key_data key;
    int num_key_usage;
    struct key_usage *key_usage;
};

#define CRYPTO_ETYPE(C) ((C)->et->type)

/* bits for `flags' below */
#define F_KEYED		 1	/* checksum is keyed */
#define F_CPROOF	 2	/* checksum is collision proof */
#define F_DERIVED	 4	/* uses derived keys */
#define F_VARIANT	 8	/* uses `variant' keys (6.4.3) */
#define F_PSEUDO	16	/* not a real protocol type */
#define F_SPECIAL	32	/* backwards */
#define F_DISABLED	64	/* enctype/checksum disabled */

struct salt_type {
    krb5_salttype type;
    const char *name;
    krb5_error_code (*string_to_key)(krb5_context, krb5_enctype, krb5_data,
				     krb5_salt, krb5_data, krb5_keyblock*);
};

struct key_type {
    krb5_keytype type; /* XXX */
    const char *name;
    size_t bits;
    size_t size;
    size_t schedule_size;
    void (*random_key)(krb5_context, krb5_keyblock*);
    void (*schedule)(krb5_context, struct key_type *, struct key_data *);
    struct salt_type *string_to_key;
    void (*random_to_key)(krb5_context, krb5_keyblock*, const void*, size_t);
    void (*cleanup)(krb5_context, struct key_data *);
    const EVP_CIPHER *(*evp)(void);
};

struct checksum_type {
    krb5_cksumtype type;
    const char *name;
    size_t blocksize;
    size_t checksumsize;
    unsigned flags;
    krb5_enctype (*checksum)(krb5_context context,
			     struct key_data *key,
			     const void *buf, size_t len,
			     unsigned usage,
			     Checksum *csum);
    krb5_error_code (*verify)(krb5_context context,
			      struct key_data *key,
			      const void *buf, size_t len,
			      unsigned usage,
			      Checksum *csum);
};

struct encryption_type {
    krb5_enctype type;
    const char *name;
    size_t blocksize;
    size_t padsize;
    size_t confoundersize;
    struct key_type *keytype;
    struct checksum_type *checksum;
    struct checksum_type *keyed_checksum;
    unsigned flags;
    krb5_error_code (*encrypt)(krb5_context context,
			       struct key_data *key,
			       void *data, size_t len,
			       krb5_boolean encryptp,
			       int usage,
			       void *ivec);
    size_t prf_length;
    krb5_error_code (*prf)(krb5_context,
			   krb5_crypto, const krb5_data *, krb5_data *);
};

#define ENCRYPTION_USAGE(U) (((U) << 8) | 0xAA)
#define INTEGRITY_USAGE(U) (((U) << 8) | 0x55)
#define CHECKSUM_USAGE(U) (((U) << 8) | 0x99)

static struct checksum_type *_find_checksum(krb5_cksumtype type);
static struct encryption_type *_find_enctype(krb5_enctype type);
static krb5_error_code _get_derived_key(krb5_context, krb5_crypto,
					unsigned, struct key_data**);
static struct key_data *_new_derived_key(krb5_crypto crypto, unsigned usage);
static krb5_error_code derive_key(krb5_context context,
				  struct encryption_type *et,
				  struct key_data *key,
				  const void *constant,
				  size_t len);
static krb5_error_code hmac(krb5_context context,
			    struct checksum_type *cm,
			    const void *data,
			    size_t len,
			    unsigned usage,
			    struct key_data *keyblock,
			    Checksum *result);
static void free_key_data(krb5_context,
			  struct key_data *,
			  struct encryption_type *);
static void free_key_schedule(krb5_context,
			      struct key_data *,
			      struct encryption_type *);
static krb5_error_code usage2arcfour (krb5_context, unsigned *);
static void xor (DES_cblock *, const unsigned char *);

/************************************************************
 *                                                          *
 ************************************************************/

struct evp_schedule {
    EVP_CIPHER_CTX ectx;
    EVP_CIPHER_CTX dctx;
};


static HEIMDAL_MUTEX crypto_mutex = HEIMDAL_MUTEX_INITIALIZER;

#ifdef WEAK_ENCTYPES
static void
krb5_DES_random_key(krb5_context context,
		    krb5_keyblock *key)
{
    DES_cblock *k = key->keyvalue.data;
    do {
	krb5_generate_random_block(k, sizeof(DES_cblock));
	DES_set_odd_parity(k);
    } while(DES_is_weak_key(k));
}

static void
krb5_DES_schedule_old(krb5_context context,
		      struct key_type *kt,
		      struct key_data *key)
{
    DES_set_key_unchecked(key->key->keyvalue.data, key->schedule->data);
}

#ifdef ENABLE_AFS_STRING_TO_KEY

/* This defines the Andrew string_to_key function.  It accepts a password
 * string as input and converts it via a one-way encryption algorithm to a DES
 * encryption key.  It is compatible with the original Andrew authentication
 * service password database.
 */

/*
 * Short passwords, i.e 8 characters or less.
 */
static void
krb5_DES_AFS3_CMU_string_to_key (krb5_data pw,
				 krb5_data cell,
				 DES_cblock *key)
{
    char  password[8+1];	/* crypt is limited to 8 chars anyway */
    int   i;

    for(i = 0; i < 8; i++) {
	char c = ((i < pw.length) ? ((char*)pw.data)[i] : 0) ^
	    ((i < cell.length) ?
	     tolower(((unsigned char*)cell.data)[i]) : 0);
	password[i] = c ? c : 'X';
    }
    password[8] = '\0';

    memcpy(key, crypt(password, "p1") + 2, sizeof(DES_cblock));

    /* parity is inserted into the LSB so left shift each byte up one
       bit. This allows ascii characters with a zero MSB to retain as
       much significance as possible. */
    for (i = 0; i < sizeof(DES_cblock); i++)
	((unsigned char*)key)[i] <<= 1;
    DES_set_odd_parity (key);
}

/*
 * Long passwords, i.e 9 characters or more.
 */
static void
krb5_DES_AFS3_Transarc_string_to_key (krb5_data pw,
				      krb5_data cell,
				      DES_cblock *key)
{
    DES_key_schedule schedule;
    DES_cblock temp_key;
    DES_cblock ivec;
    char password[512];
    size_t passlen;

    memcpy(password, pw.data, min(pw.length, sizeof(password)));
    if(pw.length < sizeof(password)) {
	int len = min(cell.length, sizeof(password) - pw.length);
	int i;

	memcpy(password + pw.length, cell.data, len);
	for (i = pw.length; i < pw.length + len; ++i)
	    password[i] = tolower((unsigned char)password[i]);
    }
    passlen = min(sizeof(password), pw.length + cell.length);
    memcpy(&ivec, "kerberos", 8);
    memcpy(&temp_key, "kerberos", 8);
    DES_set_odd_parity (&temp_key);
    DES_set_key_unchecked (&temp_key, &schedule);
    DES_cbc_cksum ((void*)password, &ivec, passlen, &schedule, &ivec);

    memcpy(&temp_key, &ivec, 8);
    DES_set_odd_parity (&temp_key);
    DES_set_key_unchecked (&temp_key, &schedule);
    DES_cbc_cksum ((void*)password, key, passlen, &schedule, &ivec);
    memset(&schedule, 0, sizeof(schedule));
    memset(&temp_key, 0, sizeof(temp_key));
    memset(&ivec, 0, sizeof(ivec));
    memset(password, 0, sizeof(password));

    DES_set_odd_parity (key);
}

static krb5_error_code
DES_AFS3_string_to_key(krb5_context context,
		       krb5_enctype enctype,
		       krb5_data password,
		       krb5_salt salt,
		       krb5_data opaque,
		       krb5_keyblock *key)
{
    DES_cblock tmp;
    if(password.length > 8)
	krb5_DES_AFS3_Transarc_string_to_key(password, salt.saltvalue, &tmp);
    else
	krb5_DES_AFS3_CMU_string_to_key(password, salt.saltvalue, &tmp);
    key->keytype = enctype;
    krb5_data_copy(&key->keyvalue, tmp, sizeof(tmp));
    memset(&key, 0, sizeof(key));
    return 0;
}
#endif /* ENABLE_AFS_STRING_TO_KEY */

static void
DES_string_to_key_int(unsigned char *data, size_t length, DES_cblock *key)
{
    DES_key_schedule schedule;
    int i;
    int reverse = 0;
    unsigned char *p;

    unsigned char swap[] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
			     0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };
    memset(key, 0, 8);

    p = (unsigned char*)key;
    for (i = 0; i < length; i++) {
	unsigned char tmp = data[i];
	if (!reverse)
	    *p++ ^= (tmp << 1);
	else
	    *--p ^= (swap[tmp & 0xf] << 4) | swap[(tmp & 0xf0) >> 4];
	if((i % 8) == 7)
	    reverse = !reverse;
    }
    DES_set_odd_parity(key);
    if(DES_is_weak_key(key))
	(*key)[7] ^= 0xF0;
    DES_set_key_unchecked(key, &schedule);
    DES_cbc_cksum((void*)data, key, length, &schedule, key);
    memset(&schedule, 0, sizeof(schedule));
    DES_set_odd_parity(key);
    if(DES_is_weak_key(key))
	(*key)[7] ^= 0xF0;
}

static krb5_error_code
krb5_DES_string_to_key(krb5_context context,
		       krb5_enctype enctype,
		       krb5_data password,
		       krb5_salt salt,
		       krb5_data opaque,
		       krb5_keyblock *key)
{
    unsigned char *s;
    size_t len;
    DES_cblock tmp;

#ifdef ENABLE_AFS_STRING_TO_KEY
    if (opaque.length == 1) {
	unsigned long v;
	_krb5_get_int(opaque.data, &v, 1);
	if (v == 1)
	    return DES_AFS3_string_to_key(context, enctype, password,
					  salt, opaque, key);
    }
#endif

    len = password.length + salt.saltvalue.length;
    s = malloc(len);
    if(len > 0 && s == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    memcpy(s, password.data, password.length);
    memcpy(s + password.length, salt.saltvalue.data, salt.saltvalue.length);
    DES_string_to_key_int(s, len, &tmp);
    key->keytype = enctype;
    krb5_data_copy(&key->keyvalue, tmp, sizeof(tmp));
    memset(&tmp, 0, sizeof(tmp));
    memset(s, 0, len);
    free(s);
    return 0;
}

static void
krb5_DES_random_to_key(krb5_context context,
		       krb5_keyblock *key,
		       const void *data,
		       size_t size)
{
    DES_cblock *k = key->keyvalue.data;
    memcpy(k, data, key->keyvalue.length);
    DES_set_odd_parity(k);
    if(DES_is_weak_key(k))
	xor(k, (const unsigned char*)"\0\0\0\0\0\0\0\xf0");
}
#endif

/*
 *
 */

static void
DES3_random_key(krb5_context context,
		krb5_keyblock *key)
{
    DES_cblock *k = key->keyvalue.data;
    do {
	krb5_generate_random_block(k, 3 * sizeof(DES_cblock));
	DES_set_odd_parity(&k[0]);
	DES_set_odd_parity(&k[1]);
	DES_set_odd_parity(&k[2]);
    } while(DES_is_weak_key(&k[0]) ||
	    DES_is_weak_key(&k[1]) ||
	    DES_is_weak_key(&k[2]));
}

/*
 * A = A xor B. A & B are 8 bytes.
 */

static void
xor (DES_cblock *key, const unsigned char *b)
{
    unsigned char *a = (unsigned char*)key;
    a[0] ^= b[0];
    a[1] ^= b[1];
    a[2] ^= b[2];
    a[3] ^= b[3];
    a[4] ^= b[4];
    a[5] ^= b[5];
    a[6] ^= b[6];
    a[7] ^= b[7];
}

#ifdef DES3_OLD_ENCTYPE
static krb5_error_code
DES3_string_to_key(krb5_context context,
		   krb5_enctype enctype,
		   krb5_data password,
		   krb5_salt salt,
		   krb5_data opaque,
		   krb5_keyblock *key)
{
    char *str;
    size_t len;
    unsigned char tmp[24];
    DES_cblock keys[3];
    krb5_error_code ret;

    len = password.length + salt.saltvalue.length;
    str = malloc(len);
    if(len != 0 && str == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    memcpy(str, password.data, password.length);
    memcpy(str + password.length, salt.saltvalue.data, salt.saltvalue.length);
    {
	DES_cblock ivec;
	DES_key_schedule s[3];
	int i;
	
	ret = _krb5_n_fold(str, len, tmp, 24);
	if (ret) {
	    memset(str, 0, len);
	    free(str);
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    return ret;
	}
	
	for(i = 0; i < 3; i++){
	    memcpy(keys + i, tmp + i * 8, sizeof(keys[i]));
	    DES_set_odd_parity(keys + i);
	    if(DES_is_weak_key(keys + i))
		xor(keys + i, (const unsigned char*)"\0\0\0\0\0\0\0\xf0");
	    DES_set_key_unchecked(keys + i, &s[i]);
	}
	memset(&ivec, 0, sizeof(ivec));
	DES_ede3_cbc_encrypt(tmp,
			     tmp, sizeof(tmp),
			     &s[0], &s[1], &s[2], &ivec, DES_ENCRYPT);
	memset(s, 0, sizeof(s));
	memset(&ivec, 0, sizeof(ivec));
	for(i = 0; i < 3; i++){
	    memcpy(keys + i, tmp + i * 8, sizeof(keys[i]));
	    DES_set_odd_parity(keys + i);
	    if(DES_is_weak_key(keys + i))
		xor(keys + i, (const unsigned char*)"\0\0\0\0\0\0\0\xf0");
	}
	memset(tmp, 0, sizeof(tmp));
    }
    key->keytype = enctype;
    krb5_data_copy(&key->keyvalue, keys, sizeof(keys));
    memset(keys, 0, sizeof(keys));
    memset(str, 0, len);
    free(str);
    return 0;
}
#endif

static krb5_error_code
DES3_string_to_key_derived(krb5_context context,
			   krb5_enctype enctype,
			   krb5_data password,
			   krb5_salt salt,
			   krb5_data opaque,
			   krb5_keyblock *key)
{
    krb5_error_code ret;
    size_t len = password.length + salt.saltvalue.length;
    char *s;

    s = malloc(len);
    if(len != 0 && s == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    memcpy(s, password.data, password.length);
    memcpy(s + password.length, salt.saltvalue.data, salt.saltvalue.length);
    ret = krb5_string_to_key_derived(context,
				     s,
				     len,
				     enctype,
				     key);
    memset(s, 0, len);
    free(s);
    return ret;
}

static void
DES3_random_to_key(krb5_context context,
		   krb5_keyblock *key,
		   const void *data,
		   size_t size)
{
    unsigned char *x = key->keyvalue.data;
    const u_char *q = data;
    DES_cblock *k;
    int i, j;

    memset(x, 0, sizeof(x));
    for (i = 0; i < 3; ++i) {
	unsigned char foo;
	for (j = 0; j < 7; ++j) {
	    unsigned char b = q[7 * i + j];

	    x[8 * i + j] = b;
	}
	foo = 0;
	for (j = 6; j >= 0; --j) {
	    foo |= q[7 * i + j] & 1;
	    foo <<= 1;
	}
	x[8 * i + 7] = foo;
    }
    k = key->keyvalue.data;
    for (i = 0; i < 3; i++) {
	DES_set_odd_parity(&k[i]);
	if(DES_is_weak_key(&k[i]))
	    xor(&k[i], (const unsigned char*)"\0\0\0\0\0\0\0\xf0");
    }
}

/*
 * ARCFOUR
 */

static void
ARCFOUR_schedule(krb5_context context,
		 struct key_type *kt,
		 struct key_data *kd)
{
    RC4_set_key (kd->schedule->data,
		 kd->key->keyvalue.length, kd->key->keyvalue.data);
}

static krb5_error_code
ARCFOUR_string_to_key(krb5_context context,
		      krb5_enctype enctype,
		      krb5_data password,
		      krb5_salt salt,
		      krb5_data opaque,
		      krb5_keyblock *key)
{
    krb5_error_code ret;
    uint16_t *s = NULL;
    size_t len, i;
    EVP_MD_CTX *m;

    m = EVP_MD_CTX_create();
    if (m == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto out;
    }

    EVP_DigestInit_ex(m, EVP_md4(), NULL);

    ret = wind_utf8ucs2_length(password.data, &len);
    if (ret) {
	krb5_set_error_message (context, ret,
				N_("Password not an UCS2 string", ""));
	goto out;
    }
	
    s = malloc (len * sizeof(s[0]));
    if (len != 0 && s == NULL) {
	krb5_set_error_message (context, ENOMEM,
				N_("malloc: out of memory", ""));
	ret = ENOMEM;
	goto out;
    }

    ret = wind_utf8ucs2(password.data, s, &len);
    if (ret) {
	krb5_set_error_message (context, ret,
				N_("Password not an UCS2 string", ""));
	goto out;
    }

    /* LE encoding */
    for (i = 0; i < len; i++) {
	unsigned char p;
	p = (s[i] & 0xff);
	EVP_DigestUpdate (m, &p, 1);
	p = (s[i] >> 8) & 0xff;
	EVP_DigestUpdate (m, &p, 1);
    }

    key->keytype = enctype;
    ret = krb5_data_alloc (&key->keyvalue, 16);
    if (ret) {
	krb5_set_error_message (context, ENOMEM, N_("malloc: out of memory", ""));
	goto out;
    }
    EVP_DigestFinal_ex (m, key->keyvalue.data, NULL);

 out:
    EVP_MD_CTX_destroy(m);
    if (s)
	memset (s, 0, len);
    free (s);
    return ret;
}

/*
 * AES
 */

int _krb5_AES_string_to_default_iterator = 4096;

static krb5_error_code
AES_string_to_key(krb5_context context,
		  krb5_enctype enctype,
		  krb5_data password,
		  krb5_salt salt,
		  krb5_data opaque,
		  krb5_keyblock *key)
{
    krb5_error_code ret;
    uint32_t iter;
    struct encryption_type *et;
    struct key_data kd;

    if (opaque.length == 0)
	iter = _krb5_AES_string_to_default_iterator;
    else if (opaque.length == 4) {
	unsigned long v;
	_krb5_get_int(opaque.data, &v, 4);
	iter = ((uint32_t)v);
    } else
	return KRB5_PROG_KEYTYPE_NOSUPP; /* XXX */
	
    et = _find_enctype(enctype);
    if (et == NULL)
	return KRB5_PROG_KEYTYPE_NOSUPP;

    kd.schedule = NULL;
    ALLOC(kd.key, 1);
    if(kd.key == NULL) {
	krb5_set_error_message (context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    kd.key->keytype = enctype;
    ret = krb5_data_alloc(&kd.key->keyvalue, et->keytype->size);
    if (ret) {
	krb5_set_error_message (context, ret, N_("malloc: out of memory", ""));
	return ret;
    }

    ret = PKCS5_PBKDF2_HMAC_SHA1(password.data, password.length,
				 salt.saltvalue.data, salt.saltvalue.length,
				 iter,
				 et->keytype->size, kd.key->keyvalue.data);
    if (ret != 1) {
	free_key_data(context, &kd, et);
	krb5_set_error_message(context, KRB5_PROG_KEYTYPE_NOSUPP,
			       "Error calculating s2k");
	return KRB5_PROG_KEYTYPE_NOSUPP;
    }

    ret = derive_key(context, et, &kd, "kerberos", strlen("kerberos"));
    if (ret == 0)
	ret = krb5_copy_keyblock_contents(context, kd.key, key);
    free_key_data(context, &kd, et);

    return ret;
}

static void
evp_schedule(krb5_context context, struct key_type *kt, struct key_data *kd)
{
    struct evp_schedule *key = kd->schedule->data;
    const EVP_CIPHER *c = (*kt->evp)();

    EVP_CIPHER_CTX_init(&key->ectx);
    EVP_CIPHER_CTX_init(&key->dctx);

    EVP_CipherInit_ex(&key->ectx, c, NULL, kd->key->keyvalue.data, NULL, 1);
    EVP_CipherInit_ex(&key->dctx, c, NULL, kd->key->keyvalue.data, NULL, 0);
}

static void
evp_cleanup(krb5_context context, struct key_data *kd)
{
    struct evp_schedule *key = kd->schedule->data;
    EVP_CIPHER_CTX_cleanup(&key->ectx);
    EVP_CIPHER_CTX_cleanup(&key->dctx);
}

/*
 *
 */

#ifdef WEAK_ENCTYPES
static struct salt_type des_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	krb5_DES_string_to_key
    },
#ifdef ENABLE_AFS_STRING_TO_KEY
    {
	KRB5_AFS3_SALT,
	"afs3-salt",
	DES_AFS3_string_to_key
    },
#endif
    { 0 }
};
#endif

#ifdef DES3_OLD_ENCTYPE
static struct salt_type des3_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	DES3_string_to_key
    },
    { 0 }
};
#endif

static struct salt_type des3_salt_derived[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	DES3_string_to_key_derived
    },
    { 0 }
};

static struct salt_type AES_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	AES_string_to_key
    },
    { 0 }
};

static struct salt_type arcfour_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	ARCFOUR_string_to_key
    },
    { 0 }
};

/*
 *
 */

static struct key_type keytype_null = {
    KEYTYPE_NULL,
    "null",
    0,
    0,
    0,
    NULL,
    NULL,
    NULL
};

#ifdef WEAK_ENCTYPES
static struct key_type keytype_des_old = {
    KEYTYPE_DES,
    "des-old",
    56,
    8,
    sizeof(DES_key_schedule),
    krb5_DES_random_key,
    krb5_DES_schedule_old,
    des_salt,
    krb5_DES_random_to_key
};

static struct key_type keytype_des = {
    KEYTYPE_DES,
    "des",
    56,
    8,
    sizeof(struct evp_schedule),
    krb5_DES_random_key,
    evp_schedule,
    des_salt,
    krb5_DES_random_to_key,
    evp_cleanup,
    EVP_des_cbc
};
#endif /* WEAK_ENCTYPES */

#ifdef DES3_OLD_ENCTYPE
static struct key_type keytype_des3 = {
    KEYTYPE_DES3,
    "des3",
    168,
    24,
    sizeof(struct evp_schedule),
    DES3_random_key,
    evp_schedule,
    des3_salt,
    DES3_random_to_key,
    evp_cleanup,
    EVP_des_ede3_cbc
};
#endif

static struct key_type keytype_des3_derived = {
    KEYTYPE_DES3,
    "des3",
    168,
    24,
    sizeof(struct evp_schedule),
    DES3_random_key,
    evp_schedule,
    des3_salt_derived,
    DES3_random_to_key,
    evp_cleanup,
    EVP_des_ede3_cbc
};

static struct key_type keytype_aes128 = {
    KEYTYPE_AES128,
    "aes-128",
    128,
    16,
    sizeof(struct evp_schedule),
    NULL,
    evp_schedule,
    AES_salt,
    NULL,
    evp_cleanup,
    EVP_hcrypto_aes_128_cts
};

static struct key_type keytype_aes256 = {
    KEYTYPE_AES256,
    "aes-256",
    256,
    32,
    sizeof(struct evp_schedule),
    NULL,
    evp_schedule,
    AES_salt,
    NULL,
    evp_cleanup,
    EVP_hcrypto_aes_256_cts
};

static struct key_type keytype_arcfour = {
    KEYTYPE_ARCFOUR,
    "arcfour",
    128,
    16,
    sizeof(RC4_KEY),
    NULL,
    ARCFOUR_schedule,
    arcfour_salt
};

krb5_error_code KRB5_LIB_FUNCTION
krb5_salttype_to_string (krb5_context context,
			 krb5_enctype etype,
			 krb5_salttype stype,
			 char **string)
{
    struct encryption_type *e;
    struct salt_type *st;

    e = _find_enctype (etype);
    if (e == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       "encryption type %d not supported",
			       etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    for (st = e->keytype->string_to_key; st && st->type; st++) {
	if (st->type == stype) {
	    *string = strdup (st->name);
	    if (*string == NULL) {
		krb5_set_error_message (context, ENOMEM,
					N_("malloc: out of memory", ""));
		return ENOMEM;
	    }
	    return 0;
	}
    }
    krb5_set_error_message (context, HEIM_ERR_SALTTYPE_NOSUPP,
			    "salttype %d not supported", stype);
    return HEIM_ERR_SALTTYPE_NOSUPP;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_salttype (krb5_context context,
			 krb5_enctype etype,
			 const char *string,
			 krb5_salttype *salttype)
{
    struct encryption_type *e;
    struct salt_type *st;

    e = _find_enctype (etype);
    if (e == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    for (st = e->keytype->string_to_key; st && st->type; st++) {
	if (strcasecmp (st->name, string) == 0) {
	    *salttype = st->type;
	    return 0;
	}
    }
    krb5_set_error_message(context, HEIM_ERR_SALTTYPE_NOSUPP,
			   N_("salttype %s not supported", ""), string);
    return HEIM_ERR_SALTTYPE_NOSUPP;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_pw_salt(krb5_context context,
		 krb5_const_principal principal,
		 krb5_salt *salt)
{
    size_t len;
    int i;
    krb5_error_code ret;
    char *p;

    salt->salttype = KRB5_PW_SALT;
    len = strlen(principal->realm);
    for (i = 0; i < principal->name.name_string.len; ++i)
	len += strlen(principal->name.name_string.val[i]);
    ret = krb5_data_alloc (&salt->saltvalue, len);
    if (ret)
	return ret;
    p = salt->saltvalue.data;
    memcpy (p, principal->realm, strlen(principal->realm));
    p += strlen(principal->realm);
    for (i = 0; i < principal->name.name_string.len; ++i) {
	memcpy (p,
		principal->name.name_string.val[i],
		strlen(principal->name.name_string.val[i]));
	p += strlen(principal->name.name_string.val[i]);
    }
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_free_salt(krb5_context context,
	       krb5_salt salt)
{
    krb5_data_free(&salt.saltvalue);
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_key_data (krb5_context context,
			 krb5_enctype enctype,
			 krb5_data password,
			 krb5_principal principal,
			 krb5_keyblock *key)
{
    krb5_error_code ret;
    krb5_salt salt;

    ret = krb5_get_pw_salt(context, principal, &salt);
    if(ret)
	return ret;
    ret = krb5_string_to_key_data_salt(context, enctype, password, salt, key);
    krb5_free_salt(context, salt);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_key (krb5_context context,
		    krb5_enctype enctype,
		    const char *password,
		    krb5_principal principal,
		    krb5_keyblock *key)
{
    krb5_data pw;
    pw.data = rk_UNCONST(password);
    pw.length = strlen(password);
    return krb5_string_to_key_data(context, enctype, pw, principal, key);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_key_data_salt (krb5_context context,
			      krb5_enctype enctype,
			      krb5_data password,
			      krb5_salt salt,
			      krb5_keyblock *key)
{
    krb5_data opaque;
    krb5_data_zero(&opaque);
    return krb5_string_to_key_data_salt_opaque(context, enctype, password,
					       salt, opaque, key);
}

/*
 * Do a string -> key for encryption type `enctype' operation on
 * `password' (with salt `salt' and the enctype specific data string
 * `opaque'), returning the resulting key in `key'
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_key_data_salt_opaque (krb5_context context,
				     krb5_enctype enctype,
				     krb5_data password,
				     krb5_salt salt,
				     krb5_data opaque,
				     krb5_keyblock *key)
{
    struct encryption_type *et =_find_enctype(enctype);
    struct salt_type *st;
    if(et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       enctype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    for(st = et->keytype->string_to_key; st && st->type; st++)
	if(st->type == salt.salttype)
	    return (*st->string_to_key)(context, enctype, password,
					salt, opaque, key);
    krb5_set_error_message(context, HEIM_ERR_SALTTYPE_NOSUPP,
			   N_("salt type %d not supported", ""),
			   salt.salttype);
    return HEIM_ERR_SALTTYPE_NOSUPP;
}

/*
 * Do a string -> key for encryption type `enctype' operation on the
 * string `password' (with salt `salt'), returning the resulting key
 * in `key'
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_key_salt (krb5_context context,
			 krb5_enctype enctype,
			 const char *password,
			 krb5_salt salt,
			 krb5_keyblock *key)
{
    krb5_data pw;
    pw.data = rk_UNCONST(password);
    pw.length = strlen(password);
    return krb5_string_to_key_data_salt(context, enctype, pw, salt, key);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_key_salt_opaque (krb5_context context,
				krb5_enctype enctype,
				const char *password,
				krb5_salt salt,
				krb5_data opaque,
				krb5_keyblock *key)
{
    krb5_data pw;
    pw.data = rk_UNCONST(password);
    pw.length = strlen(password);
    return krb5_string_to_key_data_salt_opaque(context, enctype,
					       pw, salt, opaque, key);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_enctype_keysize(krb5_context context,
		     krb5_enctype type,
		     size_t *keysize)
{
    struct encryption_type *et = _find_enctype(type);
    if(et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    *keysize = et->keytype->size;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_enctype_keybits(krb5_context context,
		     krb5_enctype type,
		     size_t *keybits)
{
    struct encryption_type *et = _find_enctype(type);
    if(et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       "encryption type %d not supported",
			       type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    *keybits = et->keytype->bits;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_generate_random_keyblock(krb5_context context,
			      krb5_enctype type,
			      krb5_keyblock *key)
{
    krb5_error_code ret;
    struct encryption_type *et = _find_enctype(type);
    if(et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    ret = krb5_data_alloc(&key->keyvalue, et->keytype->size);
    if(ret)
	return ret;
    key->keytype = type;
    if(et->keytype->random_key)
	(*et->keytype->random_key)(context, key);
    else
	krb5_generate_random_block(key->keyvalue.data,
				   key->keyvalue.length);
    return 0;
}

static krb5_error_code
_key_schedule(krb5_context context,
	      struct key_data *key)
{
    krb5_error_code ret;
    struct encryption_type *et = _find_enctype(key->key->keytype);
    struct key_type *kt;

    if (et == NULL) {
	krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				N_("encryption type %d not supported", ""),
				key->key->keytype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }

    kt = et->keytype;

    if(kt->schedule == NULL)
	return 0;
    if (key->schedule != NULL)
	return 0;
    ALLOC(key->schedule, 1);
    if(key->schedule == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    ret = krb5_data_alloc(key->schedule, kt->schedule_size);
    if(ret) {
	free(key->schedule);
	key->schedule = NULL;
	return ret;
    }
    (*kt->schedule)(context, kt, key);
    return 0;
}

/************************************************************
 *                                                          *
 ************************************************************/

static krb5_error_code
NONE_checksum(krb5_context context,
	      struct key_data *key,
	      const void *data,
	      size_t len,
	      unsigned usage,
	      Checksum *C)
{
    return 0;
}

static krb5_error_code
CRC32_checksum(krb5_context context,
	       struct key_data *key,
	       const void *data,
	       size_t len,
	       unsigned usage,
	       Checksum *C)
{
    uint32_t crc;
    unsigned char *r = C->checksum.data;
    _krb5_crc_init_table ();
    crc = _krb5_crc_update (data, len, 0);
    r[0] = crc & 0xff;
    r[1] = (crc >> 8)  & 0xff;
    r[2] = (crc >> 16) & 0xff;
    r[3] = (crc >> 24) & 0xff;
    return 0;
}

static krb5_error_code
RSA_MD4_checksum(krb5_context context,
		 struct key_data *key,
		 const void *data,
		 size_t len,
		 unsigned usage,
		 Checksum *C)
{
    if (EVP_Digest(data, len, C->checksum.data, NULL, EVP_md4(), NULL) != 1)
	krb5_abortx(context, "md4 checksum failed");
    return 0;
}

static krb5_error_code
des_checksum(krb5_context context,
	     const EVP_MD *evp_md,
	     struct key_data *key,
	     const void *data,
	     size_t len,
	     Checksum *cksum)
{
    struct evp_schedule *ctx = key->schedule->data;
    EVP_MD_CTX *m;
    DES_cblock ivec;
    unsigned char *p = cksum->checksum.data;

    krb5_generate_random_block(p, 8);

    m = EVP_MD_CTX_create();
    if (m == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    EVP_DigestInit_ex(m, evp_md, NULL);
    EVP_DigestUpdate(m, p, 8);
    EVP_DigestUpdate(m, data, len);
    EVP_DigestFinal_ex (m, p + 8, NULL);
    EVP_MD_CTX_destroy(m);
    memset (&ivec, 0, sizeof(ivec));
    EVP_CipherInit_ex(&ctx->ectx, NULL, NULL, NULL, (void *)&ivec, -1);
    EVP_Cipher(&ctx->ectx, p, p, 24);

    return 0;
}

static krb5_error_code
des_verify(krb5_context context,
	   const EVP_MD *evp_md,
	   struct key_data *key,
	   const void *data,
	   size_t len,
	   Checksum *C)
{
    struct evp_schedule *ctx = key->schedule->data;
    EVP_MD_CTX *m;
    unsigned char tmp[24];
    unsigned char res[16];
    DES_cblock ivec;
    krb5_error_code ret = 0;

    m = EVP_MD_CTX_create();
    if (m == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    memset(&ivec, 0, sizeof(ivec));
    EVP_CipherInit_ex(&ctx->dctx, NULL, NULL, NULL, (void *)&ivec, -1);
    EVP_Cipher(&ctx->dctx, tmp, C->checksum.data, 24);

    EVP_DigestInit_ex(m, evp_md, NULL);
    EVP_DigestUpdate(m, tmp, 8); /* confounder */
    EVP_DigestUpdate(m, data, len);
    EVP_DigestFinal_ex (m, res, NULL);
    EVP_MD_CTX_destroy(m);
    if(memcmp(res, tmp + 8, sizeof(res)) != 0) {
	krb5_clear_error_message (context);
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    }
    memset(tmp, 0, sizeof(tmp));
    memset(res, 0, sizeof(res));
    return ret;
}

static krb5_error_code
RSA_MD4_DES_checksum(krb5_context context,
		     struct key_data *key,
		     const void *data,
		     size_t len,
		     unsigned usage,
		     Checksum *cksum)
{
    return des_checksum(context, EVP_md4(), key, data, len, cksum);
}

static krb5_error_code
RSA_MD4_DES_verify(krb5_context context,
		   struct key_data *key,
		   const void *data,
		   size_t len,
		   unsigned usage,
		   Checksum *C)
{
    return des_verify(context, EVP_md5(), key, data, len, C);
}

static krb5_error_code
RSA_MD5_checksum(krb5_context context,
		 struct key_data *key,
		 const void *data,
		 size_t len,
		 unsigned usage,
		 Checksum *C)
{
    if (EVP_Digest(data, len, C->checksum.data, NULL, EVP_md5(), NULL) != 1)
	krb5_abortx(context, "md5 checksum failed");
    return 0;
}

static krb5_error_code
RSA_MD5_DES_checksum(krb5_context context,
		     struct key_data *key,
		     const void *data,
		     size_t len,
		     unsigned usage,
		     Checksum *C)
{
    return des_checksum(context, EVP_md5(), key, data, len, C);
}

static krb5_error_code
RSA_MD5_DES_verify(krb5_context context,
		   struct key_data *key,
		   const void *data,
		   size_t len,
		   unsigned usage,
		   Checksum *C)
{
    return des_verify(context, EVP_md5(), key, data, len, C);
}

#ifdef DES3_OLD_ENCTYPE
static krb5_error_code
RSA_MD5_DES3_checksum(krb5_context context,
		      struct key_data *key,
		      const void *data,
		      size_t len,
		      unsigned usage,
		      Checksum *C)
{
    return des_checksum(context, EVP_md5(), key, data, len, C);
}

static krb5_error_code
RSA_MD5_DES3_verify(krb5_context context,
		    struct key_data *key,
		    const void *data,
		    size_t len,
		    unsigned usage,
		    Checksum *C)
{
    return des_verify(context, EVP_md5(), key, data, len, C);
}
#endif

static krb5_error_code
SHA1_checksum(krb5_context context,
	      struct key_data *key,
	      const void *data,
	      size_t len,
	      unsigned usage,
	      Checksum *C)
{
    if (EVP_Digest(data, len, C->checksum.data, NULL, EVP_sha1(), NULL) != 1)
	krb5_abortx(context, "sha1 checksum failed");
    return 0;
}

/* HMAC according to RFC2104 */
static krb5_error_code
hmac(krb5_context context,
     struct checksum_type *cm,
     const void *data,
     size_t len,
     unsigned usage,
     struct key_data *keyblock,
     Checksum *result)
{
    unsigned char *ipad, *opad;
    unsigned char *key;
    size_t key_len;
    int i;

    ipad = malloc(cm->blocksize + len);
    if (ipad == NULL)
	return ENOMEM;
    opad = malloc(cm->blocksize + cm->checksumsize);
    if (opad == NULL) {
	free(ipad);
	return ENOMEM;
    }
    memset(ipad, 0x36, cm->blocksize);
    memset(opad, 0x5c, cm->blocksize);

    if(keyblock->key->keyvalue.length > cm->blocksize){
	(*cm->checksum)(context,
			keyblock,
			keyblock->key->keyvalue.data,
			keyblock->key->keyvalue.length,
			usage,
			result);
	key = result->checksum.data;
	key_len = result->checksum.length;
    } else {
	key = keyblock->key->keyvalue.data;
	key_len = keyblock->key->keyvalue.length;
    }
    for(i = 0; i < key_len; i++){
	ipad[i] ^= key[i];
	opad[i] ^= key[i];
    }
    memcpy(ipad + cm->blocksize, data, len);
    (*cm->checksum)(context, keyblock, ipad, cm->blocksize + len,
		    usage, result);
    memcpy(opad + cm->blocksize, result->checksum.data,
	   result->checksum.length);
    (*cm->checksum)(context, keyblock, opad,
		    cm->blocksize + cm->checksumsize, usage, result);
    memset(ipad, 0, cm->blocksize + len);
    free(ipad);
    memset(opad, 0, cm->blocksize + cm->checksumsize);
    free(opad);

    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_hmac(krb5_context context,
	  krb5_cksumtype cktype,
	  const void *data,
	  size_t len,
	  unsigned usage,
	  krb5_keyblock *key,
	  Checksum *result)
{
    struct checksum_type *c = _find_checksum(cktype);
    struct key_data kd;
    krb5_error_code ret;

    if (c == NULL) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type %d not supported", ""),
				cktype);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }

    kd.key = key;
    kd.schedule = NULL;

    ret = hmac(context, c, data, len, usage, &kd, result);

    if (kd.schedule)
	krb5_free_data(context, kd.schedule);

    return ret;
}

static krb5_error_code
SP_HMAC_SHA1_checksum(krb5_context context,
		      struct key_data *key,
		      const void *data,
		      size_t len,
		      unsigned usage,
		      Checksum *result)
{
    struct checksum_type *c = _find_checksum(CKSUMTYPE_SHA1);
    Checksum res;
    char sha1_data[20];
    krb5_error_code ret;

    res.checksum.data = sha1_data;
    res.checksum.length = sizeof(sha1_data);

    ret = hmac(context, c, data, len, usage, key, &res);
    if (ret)
	krb5_abortx(context, "hmac failed");
    memcpy(result->checksum.data, res.checksum.data, result->checksum.length);
    return 0;
}

/*
 * checksum according to section 5. of draft-brezak-win2k-krb-rc4-hmac-03.txt
 */

static krb5_error_code
HMAC_MD5_checksum(krb5_context context,
		  struct key_data *key,
		  const void *data,
		  size_t len,
		  unsigned usage,
		  Checksum *result)
{
    EVP_MD_CTX *m;
    struct checksum_type *c = _find_checksum (CKSUMTYPE_RSA_MD5);
    const char signature[] = "signaturekey";
    Checksum ksign_c;
    struct key_data ksign;
    krb5_keyblock kb;
    unsigned char t[4];
    unsigned char tmp[16];
    unsigned char ksign_c_data[16];
    krb5_error_code ret;

    m = EVP_MD_CTX_create();
    if (m == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    ksign_c.checksum.length = sizeof(ksign_c_data);
    ksign_c.checksum.data   = ksign_c_data;
    ret = hmac(context, c, signature, sizeof(signature), 0, key, &ksign_c);
    if (ret) {
	EVP_MD_CTX_destroy(m);
	return ret;
    }
    ksign.key = &kb;
    kb.keyvalue = ksign_c.checksum;
    EVP_DigestInit_ex(m, EVP_md5(), NULL);
    t[0] = (usage >>  0) & 0xFF;
    t[1] = (usage >>  8) & 0xFF;
    t[2] = (usage >> 16) & 0xFF;
    t[3] = (usage >> 24) & 0xFF;
    EVP_DigestUpdate(m, t, 4);
    EVP_DigestUpdate(m, data, len);
    EVP_DigestFinal_ex (m, tmp, NULL);
    EVP_MD_CTX_destroy(m);

    ret = hmac(context, c, tmp, sizeof(tmp), 0, &ksign, result);
    if (ret)
	return ret;
    return 0;
}

static struct checksum_type checksum_none = {
    CKSUMTYPE_NONE,
    "none",
    1,
    0,
    0,
    NONE_checksum,
    NULL
};
static struct checksum_type checksum_crc32 = {
    CKSUMTYPE_CRC32,
    "crc32",
    1,
    4,
    0,
    CRC32_checksum,
    NULL
};
static struct checksum_type checksum_rsa_md4 = {
    CKSUMTYPE_RSA_MD4,
    "rsa-md4",
    64,
    16,
    F_CPROOF,
    RSA_MD4_checksum,
    NULL
};
static struct checksum_type checksum_rsa_md4_des = {
    CKSUMTYPE_RSA_MD4_DES,
    "rsa-md4-des",
    64,
    24,
    F_KEYED | F_CPROOF | F_VARIANT,
    RSA_MD4_DES_checksum,
    RSA_MD4_DES_verify
};
static struct checksum_type checksum_rsa_md5 = {
    CKSUMTYPE_RSA_MD5,
    "rsa-md5",
    64,
    16,
    F_CPROOF,
    RSA_MD5_checksum,
    NULL
};
static struct checksum_type checksum_rsa_md5_des = {
    CKSUMTYPE_RSA_MD5_DES,
    "rsa-md5-des",
    64,
    24,
    F_KEYED | F_CPROOF | F_VARIANT,
    RSA_MD5_DES_checksum,
    RSA_MD5_DES_verify
};
#ifdef DES3_OLD_ENCTYPE
static struct checksum_type checksum_rsa_md5_des3 = {
    CKSUMTYPE_RSA_MD5_DES3,
    "rsa-md5-des3",
    64,
    24,
    F_KEYED | F_CPROOF | F_VARIANT,
    RSA_MD5_DES3_checksum,
    RSA_MD5_DES3_verify
};
#endif
static struct checksum_type checksum_sha1 = {
    CKSUMTYPE_SHA1,
    "sha1",
    64,
    20,
    F_CPROOF,
    SHA1_checksum,
    NULL
};
static struct checksum_type checksum_hmac_sha1_des3 = {
    CKSUMTYPE_HMAC_SHA1_DES3,
    "hmac-sha1-des3",
    64,
    20,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA1_checksum,
    NULL
};

static struct checksum_type checksum_hmac_sha1_aes128 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_128,
    "hmac-sha1-96-aes128",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA1_checksum,
    NULL
};

static struct checksum_type checksum_hmac_sha1_aes256 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_256,
    "hmac-sha1-96-aes256",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    SP_HMAC_SHA1_checksum,
    NULL
};

static struct checksum_type checksum_hmac_md5 = {
    CKSUMTYPE_HMAC_MD5,
    "hmac-md5",
    64,
    16,
    F_KEYED | F_CPROOF,
    HMAC_MD5_checksum,
    NULL
};

static struct checksum_type *checksum_types[] = {
    &checksum_none,
    &checksum_crc32,
    &checksum_rsa_md4,
    &checksum_rsa_md4_des,
    &checksum_rsa_md5,
    &checksum_rsa_md5_des,
#ifdef DES3_OLD_ENCTYPE
    &checksum_rsa_md5_des3,
#endif
    &checksum_sha1,
    &checksum_hmac_sha1_des3,
    &checksum_hmac_sha1_aes128,
    &checksum_hmac_sha1_aes256,
    &checksum_hmac_md5
};

static int num_checksums = sizeof(checksum_types) / sizeof(checksum_types[0]);

static struct checksum_type *
_find_checksum(krb5_cksumtype type)
{
    int i;
    for(i = 0; i < num_checksums; i++)
	if(checksum_types[i]->type == type)
	    return checksum_types[i];
    return NULL;
}

static krb5_error_code
get_checksum_key(krb5_context context,
		 krb5_crypto crypto,
		 unsigned usage,  /* not krb5_key_usage */
		 struct checksum_type *ct,
		 struct key_data **key)
{
    krb5_error_code ret = 0;

    if(ct->flags & F_DERIVED)
	ret = _get_derived_key(context, crypto, usage, key);
    else if(ct->flags & F_VARIANT) {
	int i;

	*key = _new_derived_key(crypto, 0xff/* KRB5_KU_RFC1510_VARIANT */);
	if(*key == NULL) {
	    krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	    return ENOMEM;
	}
	ret = krb5_copy_keyblock(context, crypto->key.key, &(*key)->key);
	if(ret)
	    return ret;
	for(i = 0; i < (*key)->key->keyvalue.length; i++)
	    ((unsigned char*)(*key)->key->keyvalue.data)[i] ^= 0xF0;
    } else {
	*key = &crypto->key;
    }
    if(ret == 0)
	ret = _key_schedule(context, *key);
    return ret;
}

static krb5_error_code
create_checksum (krb5_context context,
		 struct checksum_type *ct,
		 krb5_crypto crypto,
		 unsigned usage,
		 void *data,
		 size_t len,
		 Checksum *result)
{
    krb5_error_code ret;
    struct key_data *dkey;
    int keyed_checksum;

    if (ct->flags & F_DISABLED) {
	krb5_clear_error_message (context);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    keyed_checksum = (ct->flags & F_KEYED) != 0;
    if(keyed_checksum && crypto == NULL) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("Checksum type %s is keyed but no "
				   "crypto context (key) was passed in", ""),
				ct->name);
	return KRB5_PROG_SUMTYPE_NOSUPP; /* XXX */
    }
    if(keyed_checksum) {
	ret = get_checksum_key(context, crypto, usage, ct, &dkey);
	if (ret)
	    return ret;
    } else
	dkey = NULL;
    result->cksumtype = ct->type;
    ret = krb5_data_alloc(&result->checksum, ct->checksumsize);
    if (ret)
	return (ret);
    return (*ct->checksum)(context, dkey, data, len, usage, result);
}

static int
arcfour_checksum_p(struct checksum_type *ct, krb5_crypto crypto)
{
    return (ct->type == CKSUMTYPE_HMAC_MD5) &&
	(crypto->key.key->keytype == KEYTYPE_ARCFOUR);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_create_checksum(krb5_context context,
		     krb5_crypto crypto,
		     krb5_key_usage usage,
		     int type,
		     void *data,
		     size_t len,
		     Checksum *result)
{
    struct checksum_type *ct = NULL;
    unsigned keyusage;

    /* type 0 -> pick from crypto */
    if (type) {
	ct = _find_checksum(type);
    } else if (crypto) {
	ct = crypto->et->keyed_checksum;
	if (ct == NULL)
	    ct = crypto->et->checksum;
    }

    if(ct == NULL) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type %d not supported", ""),
				type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }

    if (arcfour_checksum_p(ct, crypto)) {
	keyusage = usage;
	usage2arcfour(context, &keyusage);
    } else
	keyusage = CHECKSUM_USAGE(usage);

    return create_checksum(context, ct, crypto, keyusage,
			   data, len, result);
}

static krb5_error_code
verify_checksum(krb5_context context,
		krb5_crypto crypto,
		unsigned usage, /* not krb5_key_usage */
		void *data,
		size_t len,
		Checksum *cksum)
{
    krb5_error_code ret;
    struct key_data *dkey;
    int keyed_checksum;
    Checksum c;
    struct checksum_type *ct;

    ct = _find_checksum(cksum->cksumtype);
    if (ct == NULL || (ct->flags & F_DISABLED)) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type %d not supported", ""),
				cksum->cksumtype);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    if(ct->checksumsize != cksum->checksum.length) {
	krb5_clear_error_message (context);
	return KRB5KRB_AP_ERR_BAD_INTEGRITY; /* XXX */
    }
    keyed_checksum = (ct->flags & F_KEYED) != 0;
    if(keyed_checksum) {
	struct checksum_type *kct;
	if (crypto == NULL) {
	    krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				    N_("Checksum type %s is keyed but no "
				       "crypto context (key) was passed in", ""),
				    ct->name);
	    return KRB5_PROG_SUMTYPE_NOSUPP; /* XXX */
	}
	kct = crypto->et->keyed_checksum;
	if (kct != NULL && kct->type != ct->type) {
	    krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				    N_("Checksum type %s is keyed, but "
				       "the key type %s passed didnt have that checksum "
				       "type as the keyed type", ""),
				    ct->name, crypto->et->name);
	    return KRB5_PROG_SUMTYPE_NOSUPP; /* XXX */
	}

	ret = get_checksum_key(context, crypto, usage, ct, &dkey);
	if (ret)
	    return ret;
    } else
	dkey = NULL;
    if(ct->verify)
	return (*ct->verify)(context, dkey, data, len, usage, cksum);

    ret = krb5_data_alloc (&c.checksum, ct->checksumsize);
    if (ret)
	return ret;

    ret = (*ct->checksum)(context, dkey, data, len, usage, &c);
    if (ret) {
	krb5_data_free(&c.checksum);
	return ret;
    }

    if(c.checksum.length != cksum->checksum.length ||
       memcmp(c.checksum.data, cksum->checksum.data, c.checksum.length)) {
	krb5_clear_error_message (context);
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    } else {
	ret = 0;
    }
    krb5_data_free (&c.checksum);
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_verify_checksum(krb5_context context,
		     krb5_crypto crypto,
		     krb5_key_usage usage,
		     void *data,
		     size_t len,
		     Checksum *cksum)
{
    struct checksum_type *ct;
    unsigned keyusage;

    ct = _find_checksum(cksum->cksumtype);
    if(ct == NULL) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type %d not supported", ""),
				cksum->cksumtype);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }

    if (arcfour_checksum_p(ct, crypto)) {
	keyusage = usage;
	usage2arcfour(context, &keyusage);
    } else
	keyusage = CHECKSUM_USAGE(usage);

    return verify_checksum(context, crypto, keyusage,
			   data, len, cksum);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_get_checksum_type(krb5_context context,
                              krb5_crypto crypto,
			      krb5_cksumtype *type)
{
    struct checksum_type *ct = NULL;

    if (crypto != NULL) {
        ct = crypto->et->keyed_checksum;
        if (ct == NULL)
            ct = crypto->et->checksum;
    }

    if (ct == NULL) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type not found", ""));
        return KRB5_PROG_SUMTYPE_NOSUPP;
    }

    *type = ct->type;

    return 0;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_checksumsize(krb5_context context,
		  krb5_cksumtype type,
		  size_t *size)
{
    struct checksum_type *ct = _find_checksum(type);
    if(ct == NULL) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type %d not supported", ""),
				type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    *size = ct->checksumsize;
    return 0;
}

krb5_boolean KRB5_LIB_FUNCTION
krb5_checksum_is_keyed(krb5_context context,
		       krb5_cksumtype type)
{
    struct checksum_type *ct = _find_checksum(type);
    if(ct == NULL) {
	if (context)
	    krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				    N_("checksum type %d not supported", ""),
				    type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    return ct->flags & F_KEYED;
}

krb5_boolean KRB5_LIB_FUNCTION
krb5_checksum_is_collision_proof(krb5_context context,
				 krb5_cksumtype type)
{
    struct checksum_type *ct = _find_checksum(type);
    if(ct == NULL) {
	if (context)
	    krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				    N_("checksum type %d not supported", ""),
				    type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    return ct->flags & F_CPROOF;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_checksum_disable(krb5_context context,
		      krb5_cksumtype type)
{
    struct checksum_type *ct = _find_checksum(type);
    if(ct == NULL) {
	if (context)
	    krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				    N_("checksum type %d not supported", ""),
				    type);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    ct->flags |= F_DISABLED;
    return 0;
}

/************************************************************
 *                                                          *
 ************************************************************/

static krb5_error_code
NULL_encrypt(krb5_context context,
	     struct key_data *key,
	     void *data,
	     size_t len,
	     krb5_boolean encryptp,
	     int usage,
	     void *ivec)
{
    return 0;
}

static krb5_error_code
evp_encrypt(krb5_context context,
	    struct key_data *key,
	    void *data,
	    size_t len,
	    krb5_boolean encryptp,
	    int usage,
	    void *ivec)
{
    struct evp_schedule *ctx = key->schedule->data;
    EVP_CIPHER_CTX *c;
    c = encryptp ? &ctx->ectx : &ctx->dctx;
    if (ivec == NULL) {
	/* alloca ? */
	size_t len = EVP_CIPHER_CTX_iv_length(c);
	void *loiv = malloc(len);
	if (loiv == NULL) {
	    krb5_clear_error_message(context);
	    return ENOMEM;
	}
	memset(loiv, 0, len);
	EVP_CipherInit_ex(c, NULL, NULL, NULL, loiv, -1);
	free(loiv);
    } else
	EVP_CipherInit_ex(c, NULL, NULL, NULL, ivec, -1);
    EVP_Cipher(c, data, data, len);
    return 0;
}

#ifdef WEAK_ENCTYPES
static krb5_error_code
evp_des_encrypt_null_ivec(krb5_context context,
			  struct key_data *key,
			  void *data,
			  size_t len,
			  krb5_boolean encryptp,
			  int usage,
			  void *ignore_ivec)
{
    struct evp_schedule *ctx = key->schedule->data;
    EVP_CIPHER_CTX *c;
    DES_cblock ivec;
    memset(&ivec, 0, sizeof(ivec));
    c = encryptp ? &ctx->ectx : &ctx->dctx;
    EVP_CipherInit_ex(c, NULL, NULL, NULL, (void *)&ivec, -1);
    EVP_Cipher(c, data, data, len);
    return 0;
}

static krb5_error_code
evp_des_encrypt_key_ivec(krb5_context context,
			 struct key_data *key,
			 void *data,
			 size_t len,
			 krb5_boolean encryptp,
			 int usage,
			 void *ignore_ivec)
{
    struct evp_schedule *ctx = key->schedule->data;
    EVP_CIPHER_CTX *c;
    DES_cblock ivec;
    memcpy(&ivec, key->key->keyvalue.data, sizeof(ivec));
    c = encryptp ? &ctx->ectx : &ctx->dctx;
    EVP_CipherInit_ex(c, NULL, NULL, NULL, (void *)&ivec, -1);
    EVP_Cipher(c, data, data, len);
    return 0;
}

static krb5_error_code
DES_CFB64_encrypt_null_ivec(krb5_context context,
			    struct key_data *key,
			    void *data,
			    size_t len,
			    krb5_boolean encryptp,
			    int usage,
			    void *ignore_ivec)
{
    DES_cblock ivec;
    int num = 0;
    DES_key_schedule *s = key->schedule->data;
    memset(&ivec, 0, sizeof(ivec));

    DES_cfb64_encrypt(data, data, len, s, &ivec, &num, encryptp);
    return 0;
}

static krb5_error_code
DES_PCBC_encrypt_key_ivec(krb5_context context,
			  struct key_data *key,
			  void *data,
			  size_t len,
			  krb5_boolean encryptp,
			  int usage,
			  void *ignore_ivec)
{
    DES_cblock ivec;
    DES_key_schedule *s = key->schedule->data;
    memcpy(&ivec, key->key->keyvalue.data, sizeof(ivec));

    DES_pcbc_encrypt(data, data, len, s, &ivec, encryptp);
    return 0;
}
#endif

/*
 * section 6 of draft-brezak-win2k-krb-rc4-hmac-03
 *
 * warning: not for small children
 */

static krb5_error_code
ARCFOUR_subencrypt(krb5_context context,
		   struct key_data *key,
		   void *data,
		   size_t len,
		   unsigned usage,
		   void *ivec)
{
    struct checksum_type *c = _find_checksum (CKSUMTYPE_RSA_MD5);
    Checksum k1_c, k2_c, k3_c, cksum;
    struct key_data ke;
    krb5_keyblock kb;
    unsigned char t[4];
    RC4_KEY rc4_key;
    unsigned char *cdata = data;
    unsigned char k1_c_data[16], k2_c_data[16], k3_c_data[16];
    krb5_error_code ret;

    t[0] = (usage >>  0) & 0xFF;
    t[1] = (usage >>  8) & 0xFF;
    t[2] = (usage >> 16) & 0xFF;
    t[3] = (usage >> 24) & 0xFF;

    k1_c.checksum.length = sizeof(k1_c_data);
    k1_c.checksum.data   = k1_c_data;

    ret = hmac(NULL, c, t, sizeof(t), 0, key, &k1_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    memcpy (k2_c_data, k1_c_data, sizeof(k1_c_data));

    k2_c.checksum.length = sizeof(k2_c_data);
    k2_c.checksum.data   = k2_c_data;

    ke.key = &kb;
    kb.keyvalue = k2_c.checksum;

    cksum.checksum.length = 16;
    cksum.checksum.data   = data;

    ret = hmac(NULL, c, cdata + 16, len - 16, 0, &ke, &cksum);
    if (ret)
	krb5_abortx(context, "hmac failed");

    ke.key = &kb;
    kb.keyvalue = k1_c.checksum;

    k3_c.checksum.length = sizeof(k3_c_data);
    k3_c.checksum.data   = k3_c_data;

    ret = hmac(NULL, c, data, 16, 0, &ke, &k3_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    RC4_set_key (&rc4_key, k3_c.checksum.length, k3_c.checksum.data);
    RC4 (&rc4_key, len - 16, cdata + 16, cdata + 16);
    memset (k1_c_data, 0, sizeof(k1_c_data));
    memset (k2_c_data, 0, sizeof(k2_c_data));
    memset (k3_c_data, 0, sizeof(k3_c_data));
    return 0;
}

static krb5_error_code
ARCFOUR_subdecrypt(krb5_context context,
		   struct key_data *key,
		   void *data,
		   size_t len,
		   unsigned usage,
		   void *ivec)
{
    struct checksum_type *c = _find_checksum (CKSUMTYPE_RSA_MD5);
    Checksum k1_c, k2_c, k3_c, cksum;
    struct key_data ke;
    krb5_keyblock kb;
    unsigned char t[4];
    RC4_KEY rc4_key;
    unsigned char *cdata = data;
    unsigned char k1_c_data[16], k2_c_data[16], k3_c_data[16];
    unsigned char cksum_data[16];
    krb5_error_code ret;

    t[0] = (usage >>  0) & 0xFF;
    t[1] = (usage >>  8) & 0xFF;
    t[2] = (usage >> 16) & 0xFF;
    t[3] = (usage >> 24) & 0xFF;

    k1_c.checksum.length = sizeof(k1_c_data);
    k1_c.checksum.data   = k1_c_data;

    ret = hmac(NULL, c, t, sizeof(t), 0, key, &k1_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    memcpy (k2_c_data, k1_c_data, sizeof(k1_c_data));

    k2_c.checksum.length = sizeof(k2_c_data);
    k2_c.checksum.data   = k2_c_data;

    ke.key = &kb;
    kb.keyvalue = k1_c.checksum;

    k3_c.checksum.length = sizeof(k3_c_data);
    k3_c.checksum.data   = k3_c_data;

    ret = hmac(NULL, c, cdata, 16, 0, &ke, &k3_c);
    if (ret)
	krb5_abortx(context, "hmac failed");

    RC4_set_key (&rc4_key, k3_c.checksum.length, k3_c.checksum.data);
    RC4 (&rc4_key, len - 16, cdata + 16, cdata + 16);

    ke.key = &kb;
    kb.keyvalue = k2_c.checksum;

    cksum.checksum.length = 16;
    cksum.checksum.data   = cksum_data;

    ret = hmac(NULL, c, cdata + 16, len - 16, 0, &ke, &cksum);
    if (ret)
	krb5_abortx(context, "hmac failed");

    memset (k1_c_data, 0, sizeof(k1_c_data));
    memset (k2_c_data, 0, sizeof(k2_c_data));
    memset (k3_c_data, 0, sizeof(k3_c_data));

    if (memcmp (cksum.checksum.data, data, 16) != 0) {
	krb5_clear_error_message (context);
	return KRB5KRB_AP_ERR_BAD_INTEGRITY;
    } else {
	return 0;
    }
}

/*
 * convert the usage numbers used in
 * draft-ietf-cat-kerb-key-derivation-00.txt to the ones in
 * draft-brezak-win2k-krb-rc4-hmac-04.txt
 */

static krb5_error_code
usage2arcfour (krb5_context context, unsigned *usage)
{
    switch (*usage) {
    case KRB5_KU_AS_REP_ENC_PART : /* 3 */
    case KRB5_KU_TGS_REP_ENC_PART_SUB_KEY : /* 9 */
	*usage = 8;
	return 0;
    case KRB5_KU_USAGE_SEAL :  /* 22 */
	*usage = 13;
	return 0;
    case KRB5_KU_USAGE_SIGN : /* 23 */
        *usage = 15;
        return 0;
    case KRB5_KU_USAGE_SEQ: /* 24 */
	*usage = 0;
	return 0;
    default :
	return 0;
    }
}

static krb5_error_code
ARCFOUR_encrypt(krb5_context context,
		struct key_data *key,
		void *data,
		size_t len,
		krb5_boolean encryptp,
		int usage,
		void *ivec)
{
    krb5_error_code ret;
    unsigned keyusage = usage;

    if((ret = usage2arcfour (context, &keyusage)) != 0)
	return ret;

    if (encryptp)
	return ARCFOUR_subencrypt (context, key, data, len, keyusage, ivec);
    else
	return ARCFOUR_subdecrypt (context, key, data, len, keyusage, ivec);
}


/*
 *
 */

static krb5_error_code
AES_PRF(krb5_context context,
	krb5_crypto crypto,
	const krb5_data *in,
	krb5_data *out)
{
    struct checksum_type *ct = crypto->et->checksum;
    krb5_error_code ret;
    Checksum result;
    krb5_keyblock *derived;

    result.cksumtype = ct->type;
    ret = krb5_data_alloc(&result.checksum, ct->checksumsize);
    if (ret) {
	krb5_set_error_message(context, ret, N_("malloc: out memory", ""));
	return ret;
    }

    ret = (*ct->checksum)(context, NULL, in->data, in->length, 0, &result);
    if (ret) {
	krb5_data_free(&result.checksum);
	return ret;
    }

    if (result.checksum.length < crypto->et->blocksize)
	krb5_abortx(context, "internal prf error");

    derived = NULL;
    ret = krb5_derive_key(context, crypto->key.key,
			  crypto->et->type, "prf", 3, &derived);
    if (ret)
	krb5_abortx(context, "krb5_derive_key");

    ret = krb5_data_alloc(out, crypto->et->blocksize);
    if (ret)
	krb5_abortx(context, "malloc failed");

    {
	const EVP_CIPHER *c = (*crypto->et->keytype->evp)();
	EVP_CIPHER_CTX ctx;

	EVP_CIPHER_CTX_init(&ctx); /* ivec all zero */
	EVP_CipherInit_ex(&ctx, c, NULL, derived->keyvalue.data, NULL, 1);
	EVP_Cipher(&ctx, out->data, result.checksum.data,
		   crypto->et->blocksize);
	EVP_CIPHER_CTX_cleanup(&ctx);
    }

    krb5_data_free(&result.checksum);
    krb5_free_keyblock(context, derived);

    return ret;
}

/*
 * these should currently be in reverse preference order.
 * (only relevant for !F_PSEUDO) */

static struct encryption_type enctype_null = {
    ETYPE_NULL,
    "null",
    1,
    1,
    0,
    &keytype_null,
    &checksum_none,
    NULL,
    F_DISABLED,
    NULL_encrypt,
    0,
    NULL
};
static struct encryption_type enctype_arcfour_hmac_md5 = {
    ETYPE_ARCFOUR_HMAC_MD5,
    "arcfour-hmac-md5",
    1,
    1,
    8,
    &keytype_arcfour,
    &checksum_hmac_md5,
    NULL,
    F_SPECIAL,
    ARCFOUR_encrypt,
    0,
    NULL
};
#ifdef DES3_OLD_ENCTYPE
static struct encryption_type enctype_des3_cbc_md5 = {
    ETYPE_DES3_CBC_MD5,
    "des3-cbc-md5",
    8,
    8,
    8,
    &keytype_des3,
    &checksum_rsa_md5,
    &checksum_rsa_md5_des3,
    0,
    evp_encrypt,
    0,
    NULL
};
#endif
static struct encryption_type enctype_des3_cbc_sha1 = {
    ETYPE_DES3_CBC_SHA1,
    "des3-cbc-sha1",
    8,
    8,
    8,
    &keytype_des3_derived,
    &checksum_sha1,
    &checksum_hmac_sha1_des3,
    F_DERIVED,
    evp_encrypt,
    0,
    NULL
};
#ifdef DES3_OLD_ENCTYPE
static struct encryption_type enctype_old_des3_cbc_sha1 = {
    ETYPE_OLD_DES3_CBC_SHA1,
    "old-des3-cbc-sha1",
    8,
    8,
    8,
    &keytype_des3,
    &checksum_sha1,
    &checksum_hmac_sha1_des3,
    0,
    evp_encrypt,
    0,
    NULL
};
#endif
static struct encryption_type enctype_aes128_cts_hmac_sha1 = {
    ETYPE_AES128_CTS_HMAC_SHA1_96,
    "aes128-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes128,
    &checksum_sha1,
    &checksum_hmac_sha1_aes128,
    F_DERIVED,
    evp_encrypt,
    16,
    AES_PRF
};
static struct encryption_type enctype_aes256_cts_hmac_sha1 = {
    ETYPE_AES256_CTS_HMAC_SHA1_96,
    "aes256-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes256,
    &checksum_sha1,
    &checksum_hmac_sha1_aes256,
    F_DERIVED,
    evp_encrypt,
    16,
    AES_PRF
};
static struct encryption_type enctype_des3_cbc_none = {
    ETYPE_DES3_CBC_NONE,
    "des3-cbc-none",
    8,
    8,
    0,
    &keytype_des3_derived,
    &checksum_none,
    NULL,
    F_PSEUDO,
    evp_encrypt,
    0,
    NULL
};
#ifdef WEAK_ENCTYPES
static struct encryption_type enctype_des_cbc_crc = {
    ETYPE_DES_CBC_CRC,
    "des-cbc-crc",
    8,
    8,
    8,
    &keytype_des,
    &checksum_crc32,
    NULL,
    F_DISABLED,
    evp_des_encrypt_key_ivec,
    0,
    NULL
};
static struct encryption_type enctype_des_cbc_md4 = {
    ETYPE_DES_CBC_MD4,
    "des-cbc-md4",
    8,
    8,
    8,
    &keytype_des,
    &checksum_rsa_md4,
    &checksum_rsa_md4_des,
    F_DISABLED,
    evp_des_encrypt_null_ivec,
    0,
    NULL
};
static struct encryption_type enctype_des_cbc_md5 = {
    ETYPE_DES_CBC_MD5,
    "des-cbc-md5",
    8,
    8,
    8,
    &keytype_des,
    &checksum_rsa_md5,
    &checksum_rsa_md5_des,
    F_DISABLED,
    evp_des_encrypt_null_ivec,
    0,
    NULL
};
static struct encryption_type enctype_des_cbc_none = {
    ETYPE_DES_CBC_NONE,
    "des-cbc-none",
    8,
    8,
    0,
    &keytype_des,
    &checksum_none,
    NULL,
    F_PSEUDO|F_DISABLED,
    evp_des_encrypt_null_ivec,
    0,
    NULL
};
static struct encryption_type enctype_des_cfb64_none = {
    ETYPE_DES_CFB64_NONE,
    "des-cfb64-none",
    1,
    1,
    0,
    &keytype_des_old,
    &checksum_none,
    NULL,
    F_PSEUDO|F_DISABLED,
    DES_CFB64_encrypt_null_ivec,
    0,
    NULL
};
static struct encryption_type enctype_des_pcbc_none = {
    ETYPE_DES_PCBC_NONE,
    "des-pcbc-none",
    8,
    8,
    0,
    &keytype_des_old,
    &checksum_none,
    NULL,
    F_PSEUDO|F_DISABLED,
    DES_PCBC_encrypt_key_ivec,
    0,
    NULL
};
#endif /* WEAK_ENCTYPES */

static struct encryption_type *etypes[] = {
    &enctype_aes256_cts_hmac_sha1,
    &enctype_aes128_cts_hmac_sha1,
    &enctype_des3_cbc_sha1,
    &enctype_des3_cbc_none, /* used by the gss-api mech */
    &enctype_arcfour_hmac_md5,
#ifdef DES3_OLD_ENCTYPE
    &enctype_des3_cbc_md5,
    &enctype_old_des3_cbc_sha1,
#endif
#ifdef WEAK_ENCTYPES
    &enctype_des_cbc_crc,
    &enctype_des_cbc_md4,
    &enctype_des_cbc_md5,
    &enctype_des_cbc_none,
    &enctype_des_cfb64_none,
    &enctype_des_pcbc_none,
#endif
    &enctype_null
};

static unsigned num_etypes = sizeof(etypes) / sizeof(etypes[0]);


static struct encryption_type *
_find_enctype(krb5_enctype type)
{
    int i;
    for(i = 0; i < num_etypes; i++)
	if(etypes[i]->type == type)
	    return etypes[i];
    return NULL;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_enctype_to_string(krb5_context context,
		       krb5_enctype etype,
		       char **string)
{
    struct encryption_type *e;
    e = _find_enctype(etype);
    if(e == NULL) {
	krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				N_("encryption type %d not supported", ""),
				etype);
	*string = NULL;
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    *string = strdup(e->name);
    if(*string == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_enctype(krb5_context context,
		       const char *string,
		       krb5_enctype *etype)
{
    int i;
    for(i = 0; i < num_etypes; i++)
	if(strcasecmp(etypes[i]->name, string) == 0){
	    *etype = etypes[i]->type;
	    return 0;
	}
    krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
			    N_("encryption type %s not supported", ""),
			    string);
    return KRB5_PROG_ETYPE_NOSUPP;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_enctype_to_keytype(krb5_context context,
			krb5_enctype etype,
			krb5_keytype *keytype)
{
    struct encryption_type *e = _find_enctype(etype);
    if(e == NULL) {
	krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				N_("encryption type %d not supported", ""),
				etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    *keytype = e->keytype->type; /* XXX */
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_enctype_valid(krb5_context context,
		   krb5_enctype etype)
{
    struct encryption_type *e = _find_enctype(etype);
    if(e == NULL) {
	krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				N_("encryption type %d not supported", ""),
				etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    if (e->flags & F_DISABLED) {
	krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				N_("encryption type %s is disabled", ""),
				e->name);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    return 0;
}

/**
 * Return the coresponding encryption type for a checksum type.
 *
 * @param context Kerberos context
 * @param ctype The checksum type to get the result enctype for
 * @param etype The returned encryption, when the matching etype is
 * not found, etype is set to ETYPE_NULL.
 *
 * @return Return an error code for an failure or 0 on success.
 * @ingroup krb5_crypto
 */


krb5_error_code KRB5_LIB_FUNCTION
krb5_cksumtype_to_enctype(krb5_context context,
			  krb5_cksumtype ctype,
			  krb5_enctype *etype)
{
    int i;

    *etype = ETYPE_NULL;

    for(i = 0; i < num_etypes; i++) {
	if(etypes[i]->keyed_checksum &&
	   etypes[i]->keyed_checksum->type == ctype)
	    {
		*etype = etypes[i]->type;
		return 0;
	    }
    }

    krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
			    N_("checksum type %d not supported", ""),
			    (int)ctype);
    return KRB5_PROG_SUMTYPE_NOSUPP;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_cksumtype_valid(krb5_context context,
		     krb5_cksumtype ctype)
{
    struct checksum_type *c = _find_checksum(ctype);
    if (c == NULL) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type %d not supported", ""),
				ctype);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    if (c->flags & F_DISABLED) {
	krb5_set_error_message (context, KRB5_PROG_SUMTYPE_NOSUPP,
				N_("checksum type %s is disabled", ""),
				c->name);
	return KRB5_PROG_SUMTYPE_NOSUPP;
    }
    return 0;
}


static krb5_boolean
derived_crypto(krb5_context context,
	       krb5_crypto crypto)
{
    return (crypto->et->flags & F_DERIVED) != 0;
}

static krb5_boolean
special_crypto(krb5_context context,
	       krb5_crypto crypto)
{
    return (crypto->et->flags & F_SPECIAL) != 0;
}

#define CHECKSUMSIZE(C) ((C)->checksumsize)
#define CHECKSUMTYPE(C) ((C)->type)

static krb5_error_code
encrypt_internal_derived(krb5_context context,
			 krb5_crypto crypto,
			 unsigned usage,
			 const void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    size_t sz, block_sz, checksum_sz, total_sz;
    Checksum cksum;
    unsigned char *p, *q;
    krb5_error_code ret;
    struct key_data *dkey;
    const struct encryption_type *et = crypto->et;

    checksum_sz = CHECKSUMSIZE(et->keyed_checksum);

    sz = et->confoundersize + len;
    block_sz = (sz + et->padsize - 1) &~ (et->padsize - 1); /* pad */
    total_sz = block_sz + checksum_sz;
    p = calloc(1, total_sz);
    if(p == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    q = p;
    krb5_generate_random_block(q, et->confoundersize); /* XXX */
    q += et->confoundersize;
    memcpy(q, data, len);

    ret = create_checksum(context,
			  et->keyed_checksum,
			  crypto,
			  INTEGRITY_USAGE(usage),
			  p,
			  block_sz,
			  &cksum);
    if(ret == 0 && cksum.checksum.length != checksum_sz) {
	free_Checksum (&cksum);
	krb5_clear_error_message (context);
	ret = KRB5_CRYPTO_INTERNAL;
    }
    if(ret)
	goto fail;
    memcpy(p + block_sz, cksum.checksum.data, cksum.checksum.length);
    free_Checksum (&cksum);
    ret = _get_derived_key(context, crypto, ENCRYPTION_USAGE(usage), &dkey);
    if(ret)
	goto fail;
    ret = _key_schedule(context, dkey);
    if(ret)
	goto fail;
    ret = (*et->encrypt)(context, dkey, p, block_sz, 1, usage, ivec);
    if (ret)
	goto fail;
    result->data = p;
    result->length = total_sz;
    return 0;
 fail:
    memset(p, 0, total_sz);
    free(p);
    return ret;
}


static krb5_error_code
encrypt_internal(krb5_context context,
		 krb5_crypto crypto,
		 const void *data,
		 size_t len,
		 krb5_data *result,
		 void *ivec)
{
    size_t sz, block_sz, checksum_sz;
    Checksum cksum;
    unsigned char *p, *q;
    krb5_error_code ret;
    const struct encryption_type *et = crypto->et;

    checksum_sz = CHECKSUMSIZE(et->checksum);

    sz = et->confoundersize + checksum_sz + len;
    block_sz = (sz + et->padsize - 1) &~ (et->padsize - 1); /* pad */
    p = calloc(1, block_sz);
    if(p == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    q = p;
    krb5_generate_random_block(q, et->confoundersize); /* XXX */
    q += et->confoundersize;
    memset(q, 0, checksum_sz);
    q += checksum_sz;
    memcpy(q, data, len);

    ret = create_checksum(context,
			  et->checksum,
			  crypto,
			  0,
			  p,
			  block_sz,
			  &cksum);
    if(ret == 0 && cksum.checksum.length != checksum_sz) {
	krb5_clear_error_message (context);
	free_Checksum(&cksum);
	ret = KRB5_CRYPTO_INTERNAL;
    }
    if(ret)
	goto fail;
    memcpy(p + et->confoundersize, cksum.checksum.data, cksum.checksum.length);
    free_Checksum(&cksum);
    ret = _key_schedule(context, &crypto->key);
    if(ret)
	goto fail;
    ret = (*et->encrypt)(context, &crypto->key, p, block_sz, 1, 0, ivec);
    if (ret) {
	memset(p, 0, block_sz);
	free(p);
	return ret;
    }
    result->data = p;
    result->length = block_sz;
    return 0;
 fail:
    memset(p, 0, block_sz);
    free(p);
    return ret;
}

static krb5_error_code
encrypt_internal_special(krb5_context context,
			 krb5_crypto crypto,
			 int usage,
			 const void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    struct encryption_type *et = crypto->et;
    size_t cksum_sz = CHECKSUMSIZE(et->checksum);
    size_t sz = len + cksum_sz + et->confoundersize;
    char *tmp, *p;
    krb5_error_code ret;

    tmp = malloc (sz);
    if (tmp == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    p = tmp;
    memset (p, 0, cksum_sz);
    p += cksum_sz;
    krb5_generate_random_block(p, et->confoundersize);
    p += et->confoundersize;
    memcpy (p, data, len);
    ret = (*et->encrypt)(context, &crypto->key, tmp, sz, TRUE, usage, ivec);
    if (ret) {
	memset(tmp, 0, sz);
	free(tmp);
	return ret;
    }
    result->data   = tmp;
    result->length = sz;
    return 0;
}

static krb5_error_code
decrypt_internal_derived(krb5_context context,
			 krb5_crypto crypto,
			 unsigned usage,
			 void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    size_t checksum_sz;
    Checksum cksum;
    unsigned char *p;
    krb5_error_code ret;
    struct key_data *dkey;
    struct encryption_type *et = crypto->et;
    unsigned long l;

    checksum_sz = CHECKSUMSIZE(et->keyed_checksum);
    if (len < checksum_sz + et->confoundersize) {
	krb5_set_error_message(context, KRB5_BAD_MSIZE,
			       N_("Encrypted data shorter then "
				  "checksum + confunder", ""));
	return KRB5_BAD_MSIZE;
    }

    if (((len - checksum_sz) % et->padsize) != 0) {
	krb5_clear_error_message(context);
	return KRB5_BAD_MSIZE;
    }

    p = malloc(len);
    if(len != 0 && p == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    memcpy(p, data, len);

    len -= checksum_sz;

    ret = _get_derived_key(context, crypto, ENCRYPTION_USAGE(usage), &dkey);
    if(ret) {
	free(p);
	return ret;
    }
    ret = _key_schedule(context, dkey);
    if(ret) {
	free(p);
	return ret;
    }
    ret = (*et->encrypt)(context, dkey, p, len, 0, usage, ivec);
    if (ret) {
	free(p);
	return ret;
    }

    cksum.checksum.data   = p + len;
    cksum.checksum.length = checksum_sz;
    cksum.cksumtype       = CHECKSUMTYPE(et->keyed_checksum);

    ret = verify_checksum(context,
			  crypto,
			  INTEGRITY_USAGE(usage),
			  p,
			  len,
			  &cksum);
    if(ret) {
	free(p);
	return ret;
    }
    l = len - et->confoundersize;
    memmove(p, p + et->confoundersize, l);
    result->data = realloc(p, l);
    if(result->data == NULL && l != 0) {
	free(p);
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    result->length = l;
    return 0;
}

static krb5_error_code
decrypt_internal(krb5_context context,
		 krb5_crypto crypto,
		 void *data,
		 size_t len,
		 krb5_data *result,
		 void *ivec)
{
    krb5_error_code ret;
    unsigned char *p;
    Checksum cksum;
    size_t checksum_sz, l;
    struct encryption_type *et = crypto->et;

    if ((len % et->padsize) != 0) {
	krb5_clear_error_message(context);
	return KRB5_BAD_MSIZE;
    }

    checksum_sz = CHECKSUMSIZE(et->checksum);
    p = malloc(len);
    if(len != 0 && p == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    memcpy(p, data, len);

    ret = _key_schedule(context, &crypto->key);
    if(ret) {
	free(p);
	return ret;
    }
    ret = (*et->encrypt)(context, &crypto->key, p, len, 0, 0, ivec);
    if (ret) {
	free(p);
	return ret;
    }
    ret = krb5_data_copy(&cksum.checksum, p + et->confoundersize, checksum_sz);
    if(ret) {
 	free(p);
 	return ret;
    }
    memset(p + et->confoundersize, 0, checksum_sz);
    cksum.cksumtype = CHECKSUMTYPE(et->checksum);
    ret = verify_checksum(context, NULL, 0, p, len, &cksum);
    free_Checksum(&cksum);
    if(ret) {
	free(p);
	return ret;
    }
    l = len - et->confoundersize - checksum_sz;
    memmove(p, p + et->confoundersize + checksum_sz, l);
    result->data = realloc(p, l);
    if(result->data == NULL && l != 0) {
	free(p);
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    result->length = l;
    return 0;
}

static krb5_error_code
decrypt_internal_special(krb5_context context,
			 krb5_crypto crypto,
			 int usage,
			 void *data,
			 size_t len,
			 krb5_data *result,
			 void *ivec)
{
    struct encryption_type *et = crypto->et;
    size_t cksum_sz = CHECKSUMSIZE(et->checksum);
    size_t sz = len - cksum_sz - et->confoundersize;
    unsigned char *p;
    krb5_error_code ret;

    if ((len % et->padsize) != 0) {
	krb5_clear_error_message(context);
	return KRB5_BAD_MSIZE;
    }

    p = malloc (len);
    if (p == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    memcpy(p, data, len);

    ret = (*et->encrypt)(context, &crypto->key, p, len, FALSE, usage, ivec);
    if (ret) {
	free(p);
	return ret;
    }

    memmove (p, p + cksum_sz + et->confoundersize, sz);
    result->data = realloc(p, sz);
    if(result->data == NULL && sz != 0) {
	free(p);
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    result->length = sz;
    return 0;
}

static krb5_crypto_iov *
find_iv(krb5_crypto_iov *data, int num_data, int type)
{
    int i;
    for (i = 0; i < num_data; i++)
	if (data[i].flags == type)
	    return &data[i];
    return NULL;
}

/**
 * Inline encrypt a kerberos message
 *
 * @param context Kerberos context
 * @param crypto Kerberos crypto context
 * @param usage Key usage for this buffer
 * @param data array of buffers to process
 * @param num_data length of array
 * @param ivec initial cbc/cts vector
 *
 * @return Return an error code or 0.
 * @ingroup krb5_crypto
 *
 * Kerberos encrypted data look like this:
 *
 * 1. KRB5_CRYPTO_TYPE_HEADER
 * 2. array [1,...] KRB5_CRYPTO_TYPE_DATA and array [0,...]
 *    KRB5_CRYPTO_TYPE_SIGN_ONLY in any order, however the receiver
 *    have to aware of the order. KRB5_CRYPTO_TYPE_SIGN_ONLY is
 *    commonly used headers and trailers.
 * 3. KRB5_CRYPTO_TYPE_PADDING, at least on padsize long if padsize > 1
 * 4. KRB5_CRYPTO_TYPE_TRAILER
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_encrypt_iov_ivec(krb5_context context,
		      krb5_crypto crypto,
		      unsigned usage,
		      krb5_crypto_iov *data,
		      int num_data,
		      void *ivec)
{
    size_t headersz, trailersz, len;
    int i;
    size_t sz, block_sz, pad_sz;
    Checksum cksum;
    unsigned char *p, *q;
    krb5_error_code ret;
    struct key_data *dkey;
    const struct encryption_type *et = crypto->et;
    krb5_crypto_iov *tiv, *piv, *hiv;

    if (num_data < 0) {
        krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    if(!derived_crypto(context, crypto)) {
	krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    headersz = et->confoundersize;
    trailersz = CHECKSUMSIZE(et->keyed_checksum);

    for (len = 0, i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA)
	    continue;
	len += data[i].data.length;
    }

    sz = headersz + len;
    block_sz = (sz + et->padsize - 1) &~ (et->padsize - 1); /* pad */

    pad_sz = block_sz - sz;

    /* header */

    hiv = find_iv(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (hiv == NULL || hiv->data.length != headersz)
	return KRB5_BAD_MSIZE;

    krb5_generate_random_block(hiv->data.data, hiv->data.length);

    /* padding */
    piv = find_iv(data, num_data, KRB5_CRYPTO_TYPE_PADDING);
    /* its ok to have no TYPE_PADDING if there is no padding */
    if (piv == NULL && pad_sz != 0)
	return KRB5_BAD_MSIZE;
    if (piv) {
	if (piv->data.length < pad_sz)
	    return KRB5_BAD_MSIZE;
	piv->data.length = pad_sz;
	if (pad_sz)
	    memset(piv->data.data, pad_sz, pad_sz);
	else
	    piv = NULL;
    }

    /* trailer */
    tiv = find_iv(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (tiv == NULL || tiv->data.length != trailersz)
	return KRB5_BAD_MSIZE;

    /*
     * XXX replace with EVP_Sign? at least make create_checksum an iov
     * function.
     * XXX CTS EVP is broken, can't handle multi buffers :(
     */

    len = block_sz;
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	len += data[i].data.length;
    }

    p = q = malloc(len);

    memcpy(q, hiv->data.data, hiv->data.length);
    q += hiv->data.length;
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA &&
	    data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	memcpy(q, data[i].data.data, data[i].data.length);
	q += data[i].data.length;
    }
    if (piv)
	memset(q, 0, piv->data.length);

    ret = create_checksum(context,
			  et->keyed_checksum,
			  crypto,
			  INTEGRITY_USAGE(usage),
			  p,
			  len,
			  &cksum);
    free(p);
    if(ret == 0 && cksum.checksum.length != trailersz) {
	free_Checksum (&cksum);
	krb5_clear_error_message (context);
	ret = KRB5_CRYPTO_INTERNAL;
    }
    if(ret)
	return ret;

    /* save cksum at end */
    memcpy(tiv->data.data, cksum.checksum.data, cksum.checksum.length);
    free_Checksum (&cksum);

    /* XXX replace with EVP_Cipher */
    p = q = malloc(block_sz);
    if(p == NULL)
	return ENOMEM;

    memcpy(q, hiv->data.data, hiv->data.length);
    q += hiv->data.length;

    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA)
	    continue;
	memcpy(q, data[i].data.data, data[i].data.length);
	q += data[i].data.length;
    }
    if (piv)
	memset(q, 0, piv->data.length);


    ret = _get_derived_key(context, crypto, ENCRYPTION_USAGE(usage), &dkey);
    if(ret) {
	free(p);
	return ret;
    }
    ret = _key_schedule(context, dkey);
    if(ret) {
	free(p);
	return ret;
    }

    ret = (*et->encrypt)(context, dkey, p, block_sz, 1, usage, ivec);
    if (ret) {
	free(p);
	return ret;
    }

    /* now copy data back to buffers */
    q = p;

    memcpy(hiv->data.data, q, hiv->data.length);
    q += hiv->data.length;

    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA)
	    continue;
	memcpy(data[i].data.data, q, data[i].data.length);
	q += data[i].data.length;
    }
    if (piv)
	memcpy(piv->data.data, q, pad_sz);

    free(p);

    return ret;
}

/**
 * Inline decrypt a Kerberos message.
 *
 * @param context Kerberos context
 * @param crypto Kerberos crypto context
 * @param usage Key usage for this buffer
 * @param data array of buffers to process
 * @param num_data length of array
 * @param ivec initial cbc/cts vector
 *
 * @return Return an error code or 0.
 * @ingroup krb5_crypto
 *
 * 1. KRB5_CRYPTO_TYPE_HEADER
 * 2. one KRB5_CRYPTO_TYPE_DATA and array [0,...] of KRB5_CRYPTO_TYPE_SIGN_ONLY in
 *  any order, however the receiver have to aware of the
 *  order. KRB5_CRYPTO_TYPE_SIGN_ONLY is commonly used unencrypoted
 *  protocol headers and trailers. The output data will be of same
 *  size as the input data or shorter.
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_decrypt_iov_ivec(krb5_context context,
		      krb5_crypto crypto,
		      unsigned usage,
		      krb5_crypto_iov *data,
		      unsigned int num_data,
		      void *ivec)
{
    unsigned int i;
    size_t headersz, trailersz, len;
    Checksum cksum;
    unsigned char *p, *q;
    krb5_error_code ret;
    struct key_data *dkey;
    struct encryption_type *et = crypto->et;
    krb5_crypto_iov *tiv, *hiv;

    if (num_data < 0) {
        krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    if(!derived_crypto(context, crypto)) {
	krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    headersz = et->confoundersize;

    hiv = find_iv(data, num_data, KRB5_CRYPTO_TYPE_HEADER);
    if (hiv == NULL || hiv->data.length != headersz)
	return KRB5_BAD_MSIZE;

    /* trailer */
    trailersz = CHECKSUMSIZE(et->keyed_checksum);

    tiv = find_iv(data, num_data, KRB5_CRYPTO_TYPE_TRAILER);
    if (tiv->data.length != trailersz)
	return KRB5_BAD_MSIZE;

    /* Find length of data we will decrypt */

    len = headersz;
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA)
	    continue;
	len += data[i].data.length;
    }

    if ((len % et->padsize) != 0) {
	krb5_clear_error_message(context);
	return KRB5_BAD_MSIZE;
    }

    /* XXX replace with EVP_Cipher */

    p = q = malloc(len);
    if (p == NULL)
	return ENOMEM;

    memcpy(q, hiv->data.data, hiv->data.length);
    q += hiv->data.length;

    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA)
	    continue;
	memcpy(q, data[i].data.data, data[i].data.length);
	q += data[i].data.length;
    }

    ret = _get_derived_key(context, crypto, ENCRYPTION_USAGE(usage), &dkey);
    if(ret) {
	free(p);
	return ret;
    }
    ret = _key_schedule(context, dkey);
    if(ret) {
	free(p);
	return ret;
    }

    ret = (*et->encrypt)(context, dkey, p, len, 0, usage, ivec);
    if (ret) {
	free(p);
	return ret;
    }

    /* copy data back to buffers */
    memcpy(hiv->data.data, p, hiv->data.length);
    q = p + hiv->data.length;
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA)
	    continue;
	memcpy(data[i].data.data, q, data[i].data.length);
	q += data[i].data.length;
    }

    free(p);

    /* check signature */
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	len += data[i].data.length;
    }

    p = q = malloc(len);
    if (p == NULL)
	return ENOMEM;

    memcpy(q, hiv->data.data, hiv->data.length);
    q += hiv->data.length;
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA &&
	    data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	memcpy(q, data[i].data.data, data[i].data.length);
	q += data[i].data.length;
    }

    cksum.checksum.data   = tiv->data.data;
    cksum.checksum.length = tiv->data.length;
    cksum.cksumtype       = CHECKSUMTYPE(et->keyed_checksum);

    ret = verify_checksum(context,
			  crypto,
			  INTEGRITY_USAGE(usage),
			  p,
			  len,
			  &cksum);
    free(p);
    return ret;
}

/**
 * Create a Kerberos message checksum.
 *
 * @param context Kerberos context
 * @param crypto Kerberos crypto context
 * @param usage Key usage for this buffer
 * @param data array of buffers to process
 * @param num_data length of array
 * @param type output data
 *
 * @return Return an error code or 0.
 * @ingroup krb5_crypto
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_create_checksum_iov(krb5_context context,
			 krb5_crypto crypto,
			 unsigned usage,
			 krb5_crypto_iov *data,
			 unsigned int num_data,
			 krb5_cksumtype *type)
{
    Checksum cksum;
    krb5_crypto_iov *civ;
    krb5_error_code ret;
    int i;
    size_t len;
    char *p, *q;

    if (num_data < 0) {
        krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    if(!derived_crypto(context, crypto)) {
	krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    civ = find_iv(data, num_data, KRB5_CRYPTO_TYPE_CHECKSUM);
    if (civ == NULL)
	return KRB5_BAD_MSIZE;

    len = 0;
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA &&
	    data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	len += data[i].data.length;
    }

    p = q = malloc(len);

    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA &&
	    data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	memcpy(q, data[i].data.data, data[i].data.length);
	q += data[i].data.length;
    }

    ret = krb5_create_checksum(context, crypto, usage, 0, p, len, &cksum);
    free(p);
    if (ret)
	return ret;

    if (type)
	*type = cksum.cksumtype;

    if (cksum.checksum.length > civ->data.length) {
	krb5_set_error_message(context, KRB5_BAD_MSIZE,
			       N_("Checksum larger then input buffer", ""));
	free_Checksum(&cksum);
	return KRB5_BAD_MSIZE;
    }

    civ->data.length = cksum.checksum.length;
    memcpy(civ->data.data, cksum.checksum.data, civ->data.length);
    free_Checksum(&cksum);

    return 0;
}

/**
 * Verify a Kerberos message checksum.
 *
 * @param context Kerberos context
 * @param crypto Kerberos crypto context
 * @param usage Key usage for this buffer
 * @param data array of buffers to process
 * @param num_data length of array
 *
 * @return Return an error code or 0.
 * @ingroup krb5_crypto
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_verify_checksum_iov(krb5_context context,
			 krb5_crypto crypto,
			 unsigned usage,
			 krb5_crypto_iov *data,
			 unsigned int num_data,
			 krb5_cksumtype *type)
{
    struct encryption_type *et = crypto->et;
    Checksum cksum;
    krb5_crypto_iov *civ;
    krb5_error_code ret;
    int i;
    size_t len;
    char *p, *q;

    if (num_data < 0) {
        krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    if(!derived_crypto(context, crypto)) {
	krb5_clear_error_message(context);
	return KRB5_CRYPTO_INTERNAL;
    }

    civ = find_iv(data, num_data, KRB5_CRYPTO_TYPE_CHECKSUM);
    if (civ == NULL)
	return KRB5_BAD_MSIZE;

    len = 0;
    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA &&
	    data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	len += data[i].data.length;
    }

    p = q = malloc(len);

    for (i = 0; i < num_data; i++) {
	if (data[i].flags != KRB5_CRYPTO_TYPE_DATA &&
	    data[i].flags != KRB5_CRYPTO_TYPE_SIGN_ONLY)
	    continue;
	memcpy(q, data[i].data.data, data[i].data.length);
	q += data[i].data.length;
    }

    cksum.cksumtype = CHECKSUMTYPE(et->keyed_checksum);
    cksum.checksum.length = civ->data.length;
    cksum.checksum.data = civ->data.data;

    ret = krb5_verify_checksum(context, crypto, usage, p, len, &cksum);
    free(p);

    if (ret == 0 && type)
	*type = cksum.cksumtype;

    return ret;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_length(krb5_context context,
		   krb5_crypto crypto,
		   int type,
		   size_t *len)
{
    if (!derived_crypto(context, crypto)) {
	krb5_set_error_message(context, EINVAL, "not a derived crypto");
	return EINVAL;
    }
	
    switch(type) {
    case KRB5_CRYPTO_TYPE_EMPTY:
	*len = 0;
	return 0;
    case KRB5_CRYPTO_TYPE_HEADER:
	*len = crypto->et->blocksize;
	return 0;
    case KRB5_CRYPTO_TYPE_DATA:
    case KRB5_CRYPTO_TYPE_SIGN_ONLY:
	/* len must already been filled in */
	return 0;
    case KRB5_CRYPTO_TYPE_PADDING:
	if (crypto->et->padsize > 1)
	    *len = crypto->et->padsize;
	else
	    *len = 0;
	return 0;
    case KRB5_CRYPTO_TYPE_TRAILER:
	*len = CHECKSUMSIZE(crypto->et->keyed_checksum);
	return 0;
    case KRB5_CRYPTO_TYPE_CHECKSUM:
	if (crypto->et->keyed_checksum)
	    *len = CHECKSUMSIZE(crypto->et->keyed_checksum);
	else
	    *len = CHECKSUMSIZE(crypto->et->checksum);
	return 0;
    }
    krb5_set_error_message(context, EINVAL,
			   "%d not a supported type", type);
    return EINVAL;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_length_iov(krb5_context context,
		       krb5_crypto crypto,
		       krb5_crypto_iov *data,
		       unsigned int num_data)
{
    krb5_error_code ret;
    int i;

    for (i = 0; i < num_data; i++) {
	ret = krb5_crypto_length(context, crypto,
				 data[i].flags,
				 &data[i].data.length);
	if (ret)
	    return ret;
    }
    return 0;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_encrypt_ivec(krb5_context context,
		  krb5_crypto crypto,
		  unsigned usage,
		  const void *data,
		  size_t len,
		  krb5_data *result,
		  void *ivec)
{
    if(derived_crypto(context, crypto))
	return encrypt_internal_derived(context, crypto, usage,
					data, len, result, ivec);
    else if (special_crypto(context, crypto))
	return encrypt_internal_special (context, crypto, usage,
					 data, len, result, ivec);
    else
	return encrypt_internal(context, crypto, data, len, result, ivec);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_encrypt(krb5_context context,
	     krb5_crypto crypto,
	     unsigned usage,
	     const void *data,
	     size_t len,
	     krb5_data *result)
{
    return krb5_encrypt_ivec(context, crypto, usage, data, len, result, NULL);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_encrypt_EncryptedData(krb5_context context,
			   krb5_crypto crypto,
			   unsigned usage,
			   void *data,
			   size_t len,
			   int kvno,
			   EncryptedData *result)
{
    result->etype = CRYPTO_ETYPE(crypto);
    if(kvno){
	ALLOC(result->kvno, 1);
	*result->kvno = kvno;
    }else
	result->kvno = NULL;
    return krb5_encrypt(context, crypto, usage, data, len, &result->cipher);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_decrypt_ivec(krb5_context context,
		  krb5_crypto crypto,
		  unsigned usage,
		  void *data,
		  size_t len,
		  krb5_data *result,
		  void *ivec)
{
    if(derived_crypto(context, crypto))
	return decrypt_internal_derived(context, crypto, usage,
					data, len, result, ivec);
    else if (special_crypto (context, crypto))
	return decrypt_internal_special(context, crypto, usage,
					data, len, result, ivec);
    else
	return decrypt_internal(context, crypto, data, len, result, ivec);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_decrypt(krb5_context context,
	     krb5_crypto crypto,
	     unsigned usage,
	     void *data,
	     size_t len,
	     krb5_data *result)
{
    return krb5_decrypt_ivec (context, crypto, usage, data, len, result,
			      NULL);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_decrypt_EncryptedData(krb5_context context,
			   krb5_crypto crypto,
			   unsigned usage,
			   const EncryptedData *e,
			   krb5_data *result)
{
    return krb5_decrypt(context, crypto, usage,
			e->cipher.data, e->cipher.length, result);
}

/************************************************************
 *                                                          *
 ************************************************************/

#define ENTROPY_NEEDED 128

static int
seed_something(void)
{
    char buf[1024], seedfile[256];

    /* If there is a seed file, load it. But such a file cannot be trusted,
       so use 0 for the entropy estimate */
    if (RAND_file_name(seedfile, sizeof(seedfile))) {
	int fd;
	fd = open(seedfile, O_RDONLY | O_BINARY | O_CLOEXEC);
	if (fd >= 0) {
	    ssize_t ret;
	    rk_cloexec(fd);
	    ret = read(fd, buf, sizeof(buf));
	    if (ret > 0)
		RAND_add(buf, ret, 0.0);
	    close(fd);
	} else
	    seedfile[0] = '\0';
    } else
	seedfile[0] = '\0';

    /* Calling RAND_status() will try to use /dev/urandom if it exists so
       we do not have to deal with it. */
    if (RAND_status() != 1) {
	krb5_context context;
	const char *p;

	/* Try using egd */
	if (!krb5_init_context(&context)) {
	    p = krb5_config_get_string(context, NULL, "libdefaults",
				       "egd_socket", NULL);
	    if (p != NULL)
		RAND_egd_bytes(p, ENTROPY_NEEDED);
	    krb5_free_context(context);
	}
    }

    if (RAND_status() == 1)	{
	/* Update the seed file */
	if (seedfile[0])
	    RAND_write_file(seedfile);

	return 0;
    } else
	return -1;
}

void KRB5_LIB_FUNCTION
krb5_generate_random_block(void *buf, size_t len)
{
    static int rng_initialized = 0;

    HEIMDAL_MUTEX_lock(&crypto_mutex);
    if (!rng_initialized) {
	if (seed_something())
	    krb5_abortx(NULL, "Fatal: could not seed the "
			"random number generator");
	
	rng_initialized = 1;
    }
    HEIMDAL_MUTEX_unlock(&crypto_mutex);
    if (RAND_bytes(buf, len) != 1)
	krb5_abortx(NULL, "Failed to generate random block");
}

static krb5_error_code
derive_key(krb5_context context,
	   struct encryption_type *et,
	   struct key_data *key,
	   const void *constant,
	   size_t len)
{
    unsigned char *k = NULL;
    unsigned int nblocks = 0, i;
    krb5_error_code ret = 0;
    struct key_type *kt = et->keytype;

    ret = _key_schedule(context, key);
    if(ret)
	return ret;
    if(et->blocksize * 8 < kt->bits || len != et->blocksize) {
	nblocks = (kt->bits + et->blocksize * 8 - 1) / (et->blocksize * 8);
	k = malloc(nblocks * et->blocksize);
	if(k == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto out;
	}
	ret = _krb5_n_fold(constant, len, k, et->blocksize);
	if (ret) {
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto out;
	}

	for(i = 0; i < nblocks; i++) {
	    if(i > 0)
		memcpy(k + i * et->blocksize,
		       k + (i - 1) * et->blocksize,
		       et->blocksize);
	    (*et->encrypt)(context, key, k + i * et->blocksize, et->blocksize,
			   1, 0, NULL);
	}
    } else {
	/* this case is probably broken, but won't be run anyway */
	void *c = malloc(len);
	size_t res_len = (kt->bits + 7) / 8;

	if(len != 0 && c == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto out;
	}
	memcpy(c, constant, len);
	(*et->encrypt)(context, key, c, len, 1, 0, NULL);
	k = malloc(res_len);
	if(res_len != 0 && k == NULL) {
	    free(c);
	    ret = ENOMEM;
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto out;
	}
	ret = _krb5_n_fold(c, len, k, res_len);
	free(c);
	if (ret) {
	    krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	    goto out;
	}
    }

    /* XXX keytype dependent post-processing */
    switch(kt->type) {
    case KEYTYPE_DES3:
	DES3_random_to_key(context, key->key, k, nblocks * et->blocksize);
	break;
    case KEYTYPE_AES128:
    case KEYTYPE_AES256:
	memcpy(key->key->keyvalue.data, k, key->key->keyvalue.length);
	break;
    default:
	ret = KRB5_CRYPTO_INTERNAL;
	krb5_set_error_message(context, ret,
			       N_("derive_key() called with unknown keytype (%u)", ""),
			       kt->type);
	break;
    }
 out:
    if (key->schedule) {
	free_key_schedule(context, key, et);
	key->schedule = NULL;
    }
    if (k) {
	memset(k, 0, nblocks * et->blocksize);
	free(k);
    }
    return ret;
}

static struct key_data *
_new_derived_key(krb5_crypto crypto, unsigned usage)
{
    struct key_usage *d = crypto->key_usage;
    d = realloc(d, (crypto->num_key_usage + 1) * sizeof(*d));
    if(d == NULL)
	return NULL;
    crypto->key_usage = d;
    d += crypto->num_key_usage++;
    memset(d, 0, sizeof(*d));
    d->usage = usage;
    return &d->key;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_derive_key(krb5_context context,
		const krb5_keyblock *key,
		krb5_enctype etype,
		const void *constant,
		size_t constant_len,
		krb5_keyblock **derived_key)
{
    krb5_error_code ret;
    struct encryption_type *et;
    struct key_data d;

    *derived_key = NULL;

    et = _find_enctype (etype);
    if (et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }

    ret = krb5_copy_keyblock(context, key, &d.key);
    if (ret)
	return ret;

    d.schedule = NULL;
    ret = derive_key(context, et, &d, constant, constant_len);
    if (ret == 0)
	ret = krb5_copy_keyblock(context, d.key, derived_key);
    free_key_data(context, &d, et);
    return ret;
}

static krb5_error_code
_get_derived_key(krb5_context context,
		 krb5_crypto crypto,
		 unsigned usage,
		 struct key_data **key)
{
    int i;
    struct key_data *d;
    unsigned char constant[5];

    for(i = 0; i < crypto->num_key_usage; i++)
	if(crypto->key_usage[i].usage == usage) {
	    *key = &crypto->key_usage[i].key;
	    return 0;
	}
    d = _new_derived_key(crypto, usage);
    if(d == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    krb5_copy_keyblock(context, crypto->key.key, &d->key);
    _krb5_put_int(constant, usage, 5);
    derive_key(context, crypto->et, d, constant, sizeof(constant));
    *key = d;
    return 0;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_init(krb5_context context,
		 const krb5_keyblock *key,
		 krb5_enctype etype,
		 krb5_crypto *crypto)
{
    krb5_error_code ret;
    ALLOC(*crypto, 1);
    if(*crypto == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    if(etype == ETYPE_NULL)
	etype = key->keytype;
    (*crypto)->et = _find_enctype(etype);
    if((*crypto)->et == NULL || ((*crypto)->et->flags & F_DISABLED)) {
	free(*crypto);
	*crypto = NULL;
	krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				N_("encryption type %d not supported", ""),
				etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    if((*crypto)->et->keytype->size != key->keyvalue.length) {
	free(*crypto);
	*crypto = NULL;
	krb5_set_error_message (context, KRB5_BAD_KEYSIZE,
				"encryption key has bad length");
	return KRB5_BAD_KEYSIZE;
    }
    ret = krb5_copy_keyblock(context, key, &(*crypto)->key.key);
    if(ret) {
	free(*crypto);
	*crypto = NULL;
	return ret;
    }
    (*crypto)->key.schedule = NULL;
    (*crypto)->num_key_usage = 0;
    (*crypto)->key_usage = NULL;
    return 0;
}

static void
free_key_schedule(krb5_context context,
		  struct key_data *key,
		  struct encryption_type *et)
{
    if (et->keytype->cleanup)
	(*et->keytype->cleanup)(context, key);
    memset(key->schedule->data, 0, key->schedule->length);
    krb5_free_data(context, key->schedule);
}

static void
free_key_data(krb5_context context, struct key_data *key,
	      struct encryption_type *et)
{
    krb5_free_keyblock(context, key->key);
    if(key->schedule) {
	free_key_schedule(context, key, et);
	key->schedule = NULL;
    }
}

static void
free_key_usage(krb5_context context, struct key_usage *ku,
	       struct encryption_type *et)
{
    free_key_data(context, &ku->key, et);
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_destroy(krb5_context context,
		    krb5_crypto crypto)
{
    int i;

    for(i = 0; i < crypto->num_key_usage; i++)
	free_key_usage(context, &crypto->key_usage[i], crypto->et);
    free(crypto->key_usage);
    free_key_data(context, &crypto->key, crypto->et);
    free (crypto);
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_getblocksize(krb5_context context,
			 krb5_crypto crypto,
			 size_t *blocksize)
{
    *blocksize = crypto->et->blocksize;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_getenctype(krb5_context context,
		       krb5_crypto crypto,
		       krb5_enctype *enctype)
{
    *enctype = crypto->et->type;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_getpadsize(krb5_context context,
                       krb5_crypto crypto,
                       size_t *padsize)
{
    *padsize = crypto->et->padsize;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_getconfoundersize(krb5_context context,
                              krb5_crypto crypto,
                              size_t *confoundersize)
{
    *confoundersize = crypto->et->confoundersize;
    return 0;
}


/**
 * Disable encryption type
 *
 * @param context Kerberos 5 context
 * @param enctype encryption type to disable
 *
 * @return Return an error code or 0.
 *
 * @ingroup krb5_crypto
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_enctype_disable(krb5_context context,
		     krb5_enctype enctype)
{
    struct encryption_type *et = _find_enctype(enctype);
    if(et == NULL) {
	if (context)
	    krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				    N_("encryption type %d not supported", ""),
				    enctype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    et->flags |= F_DISABLED;
    return 0;
}

/**
 * Enable encryption type
 *
 * @param context Kerberos 5 context
 * @param enctype encryption type to enable
 *
 * @return Return an error code or 0.
 *
 * @ingroup krb5_crypto
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_enctype_enable(krb5_context context,
		    krb5_enctype enctype)
{
    struct encryption_type *et = _find_enctype(enctype);
    if(et == NULL) {
	if (context)
	    krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				    N_("encryption type %d not supported", ""),
				    enctype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    et->flags &= ~F_DISABLED;
    return 0;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_string_to_key_derived(krb5_context context,
			   const void *str,
			   size_t len,
			   krb5_enctype etype,
			   krb5_keyblock *key)
{
    struct encryption_type *et = _find_enctype(etype);
    krb5_error_code ret;
    struct key_data kd;
    size_t keylen;
    u_char *tmp;

    if(et == NULL) {
	krb5_set_error_message (context, KRB5_PROG_ETYPE_NOSUPP,
				N_("encryption type %d not supported", ""),
				etype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    keylen = et->keytype->bits / 8;

    ALLOC(kd.key, 1);
    if(kd.key == NULL) {
	krb5_set_error_message (context, ENOMEM,
				N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    ret = krb5_data_alloc(&kd.key->keyvalue, et->keytype->size);
    if(ret) {
	free(kd.key);
	return ret;
    }
    kd.key->keytype = etype;
    tmp = malloc (keylen);
    if(tmp == NULL) {
	krb5_free_keyblock(context, kd.key);
	krb5_set_error_message (context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    ret = _krb5_n_fold(str, len, tmp, keylen);
    if (ret) {
	free(tmp);
	krb5_set_error_message (context, ENOMEM, N_("malloc: out of memory", ""));
	return ret;
    }
    kd.schedule = NULL;
    DES3_random_to_key(context, kd.key, tmp, keylen);
    memset(tmp, 0, keylen);
    free(tmp);
    ret = derive_key(context,
		     et,
		     &kd,
		     "kerberos", /* XXX well known constant */
		     strlen("kerberos"));
    if (ret) {
	free_key_data(context, &kd, et);
	return ret;
    }
    ret = krb5_copy_keyblock_contents(context, kd.key, key);
    free_key_data(context, &kd, et);
    return ret;
}

static size_t
wrapped_length (krb5_context context,
		krb5_crypto  crypto,
		size_t       data_len)
{
    struct encryption_type *et = crypto->et;
    size_t padsize = et->padsize;
    size_t checksumsize = CHECKSUMSIZE(et->checksum);
    size_t res;

    res =  et->confoundersize + checksumsize + data_len;
    res =  (res + padsize - 1) / padsize * padsize;
    return res;
}

static size_t
wrapped_length_dervied (krb5_context context,
			krb5_crypto  crypto,
			size_t       data_len)
{
    struct encryption_type *et = crypto->et;
    size_t padsize = et->padsize;
    size_t res;

    res =  et->confoundersize + data_len;
    res =  (res + padsize - 1) / padsize * padsize;
    if (et->keyed_checksum)
	res += et->keyed_checksum->checksumsize;
    else
	res += et->checksum->checksumsize;
    return res;
}

/*
 * Return the size of an encrypted packet of length `data_len'
 */

size_t
krb5_get_wrapped_length (krb5_context context,
			 krb5_crypto  crypto,
			 size_t       data_len)
{
    if (derived_crypto (context, crypto))
	return wrapped_length_dervied (context, crypto, data_len);
    else
	return wrapped_length (context, crypto, data_len);
}

/*
 * Return the size of an encrypted packet of length `data_len'
 */

static size_t
crypto_overhead (krb5_context context,
		 krb5_crypto  crypto)
{
    struct encryption_type *et = crypto->et;
    size_t res;

    res = CHECKSUMSIZE(et->checksum);
    res += et->confoundersize;
    if (et->padsize > 1)
	res += et->padsize;
    return res;
}

static size_t
crypto_overhead_dervied (krb5_context context,
			 krb5_crypto  crypto)
{
    struct encryption_type *et = crypto->et;
    size_t res;

    if (et->keyed_checksum)
	res = CHECKSUMSIZE(et->keyed_checksum);
    else
	res = CHECKSUMSIZE(et->checksum);
    res += et->confoundersize;
    if (et->padsize > 1)
	res += et->padsize;
    return res;
}

size_t
krb5_crypto_overhead (krb5_context context, krb5_crypto crypto)
{
    if (derived_crypto (context, crypto))
	return crypto_overhead_dervied (context, crypto);
    else
	return crypto_overhead (context, crypto);
}

/**
 * Converts the random bytestring to a protocol key according to
 * Kerberos crypto frame work. It may be assumed that all the bits of
 * the input string are equally random, even though the entropy
 * present in the random source may be limited.
 *
 * @param context Kerberos 5 context
 * @param type the enctype resulting key will be of
 * @param data input random data to convert to a key
 * @param data size of input random data, at least krb5_enctype_keysize() long
 * @param data key, output key, free with krb5_free_keyblock_contents()
 *
 * @return Return an error code or 0.
 *
 * @ingroup krb5_crypto
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_random_to_key(krb5_context context,
		   krb5_enctype type,
		   const void *data,
		   size_t size,
		   krb5_keyblock *key)
{
    krb5_error_code ret;
    struct encryption_type *et = _find_enctype(type);
    if(et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    if ((et->keytype->bits + 7) / 8 > size) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption key %s needs %d bytes "
				  "of random to make an encryption key "
				  "out of it", ""),
			       et->name, (int)et->keytype->size);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    ret = krb5_data_alloc(&key->keyvalue, et->keytype->size);
    if(ret)
	return ret;
    key->keytype = type;
    if (et->keytype->random_to_key)
 	(*et->keytype->random_to_key)(context, key, data, size);
    else
	memcpy(key->keyvalue.data, data, et->keytype->size);

    return 0;
}

krb5_error_code
_krb5_pk_octetstring2key(krb5_context context,
			 krb5_enctype type,
			 const void *dhdata,
			 size_t dhsize,
			 const heim_octet_string *c_n,
			 const heim_octet_string *k_n,
			 krb5_keyblock *key)
{
    struct encryption_type *et = _find_enctype(type);
    krb5_error_code ret;
    size_t keylen, offset;
    void *keydata;
    unsigned char counter;
    unsigned char shaoutput[SHA_DIGEST_LENGTH];

    if(et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    keylen = (et->keytype->bits + 7) / 8;

    keydata = malloc(keylen);
    if (keydata == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    counter = 0;
    offset = 0;
    do {
	SHA_CTX m;
	
	SHA1_Init(&m);
	SHA1_Update(&m, &counter, 1);
	SHA1_Update(&m, dhdata, dhsize);
	if (c_n)
	    SHA1_Update(&m, c_n->data, c_n->length);
	if (k_n)
	    SHA1_Update(&m, k_n->data, k_n->length);
	SHA1_Final(shaoutput, &m);

	memcpy((unsigned char *)keydata + offset,
	       shaoutput,
	       min(keylen - offset, sizeof(shaoutput)));

	offset += sizeof(shaoutput);
	counter++;
    } while(offset < keylen);
    memset(shaoutput, 0, sizeof(shaoutput));

    ret = krb5_random_to_key(context, type, keydata, keylen, key);
    memset(keydata, 0, sizeof(keylen));
    free(keydata);
    return ret;
}

static krb5_error_code
encode_uvinfo(krb5_context context, krb5_const_principal p, krb5_data *data)
{
    KRB5PrincipalName pn;
    krb5_error_code ret;
    size_t size;

    pn.principalName = p->name;
    pn.realm = p->realm;

    ASN1_MALLOC_ENCODE(KRB5PrincipalName, data->data, data->length,
		       &pn, &size, ret);
    if (ret) {
	krb5_data_zero(data);
	krb5_set_error_message(context, ret,
			       N_("Failed to encode KRB5PrincipalName", ""));
	return ret;
    }
    if (data->length != size)
	krb5_abortx(context, "asn1 compiler internal error");
    return 0;
}

static krb5_error_code
encode_otherinfo(krb5_context context,
		 const AlgorithmIdentifier *ai,
		 krb5_const_principal client,
		 krb5_const_principal server,
		 krb5_enctype enctype,
		 const krb5_data *as_req,
		 const krb5_data *pk_as_rep,
		 const Ticket *ticket,
		 krb5_data *other)
{
    PkinitSP80056AOtherInfo otherinfo;
    PkinitSuppPubInfo pubinfo;
    krb5_error_code ret;
    krb5_data pub;
    size_t size;

    krb5_data_zero(other);
    memset(&otherinfo, 0, sizeof(otherinfo));
    memset(&pubinfo, 0, sizeof(pubinfo));

    pubinfo.enctype = enctype;
    pubinfo.as_REQ = *as_req;
    pubinfo.pk_as_rep = *pk_as_rep;
    pubinfo.ticket = *ticket;
    ASN1_MALLOC_ENCODE(PkinitSuppPubInfo, pub.data, pub.length,
		       &pubinfo, &size, ret);
    if (ret) {
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	return ret;
    }
    if (pub.length != size)
	krb5_abortx(context, "asn1 compiler internal error");

    ret = encode_uvinfo(context, client, &otherinfo.partyUInfo);
    if (ret) {
	free(pub.data);
	return ret;
    }
    ret = encode_uvinfo(context, server, &otherinfo.partyVInfo);
    if (ret) {
	free(otherinfo.partyUInfo.data);
	free(pub.data);
	return ret;
    }

    otherinfo.algorithmID = *ai;
    otherinfo.suppPubInfo = &pub;

    ASN1_MALLOC_ENCODE(PkinitSP80056AOtherInfo, other->data, other->length,
		       &otherinfo, &size, ret);
    free(otherinfo.partyUInfo.data);
    free(otherinfo.partyVInfo.data);
    free(pub.data);
    if (ret) {
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	return ret;
    }
    if (other->length != size)
	krb5_abortx(context, "asn1 compiler internal error");

    return 0;
}

krb5_error_code
_krb5_pk_kdf(krb5_context context,
	     const struct AlgorithmIdentifier *ai,
	     const void *dhdata,
	     size_t dhsize,
	     krb5_const_principal client,
	     krb5_const_principal server,
	     krb5_enctype enctype,
	     const krb5_data *as_req,
	     const krb5_data *pk_as_rep,
	     const Ticket *ticket,
	     krb5_keyblock *key)
{
    struct encryption_type *et;
    krb5_error_code ret;
    krb5_data other;
    size_t keylen, offset;
    uint32_t counter;
    unsigned char *keydata;
    unsigned char shaoutput[SHA_DIGEST_LENGTH];

    if (der_heim_oid_cmp(&asn1_oid_id_pkinit_kdf_ah_sha1, &ai->algorithm) != 0) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("KDF not supported", ""));
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    if (ai->parameters != NULL &&
	(ai->parameters->length != 2 ||
	 memcmp(ai->parameters->data, "\x05\x00", 2) != 0))
	{
	    krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
				   N_("kdf params not NULL or the NULL-type",
				      ""));
	    return KRB5_PROG_ETYPE_NOSUPP;
	}

    et = _find_enctype(enctype);
    if(et == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       enctype);
	return KRB5_PROG_ETYPE_NOSUPP;
    }
    keylen = (et->keytype->bits + 7) / 8;

    keydata = malloc(keylen);
    if (keydata == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    ret = encode_otherinfo(context, ai, client, server,
			   enctype, as_req, pk_as_rep, ticket, &other);
    if (ret) {
	free(keydata);
	return ret;
    }

    offset = 0;
    counter = 1;
    do {
	unsigned char cdata[4];
	SHA_CTX m;
	
	SHA1_Init(&m);
	_krb5_put_int(cdata, counter, 4);
	SHA1_Update(&m, cdata, 4);
	SHA1_Update(&m, dhdata, dhsize);
	SHA1_Update(&m, other.data, other.length);
	SHA1_Final(shaoutput, &m);

	memcpy((unsigned char *)keydata + offset,
	       shaoutput,
	       min(keylen - offset, sizeof(shaoutput)));

	offset += sizeof(shaoutput);
	counter++;
    } while(offset < keylen);
    memset(shaoutput, 0, sizeof(shaoutput));

    free(other.data);

    ret = krb5_random_to_key(context, enctype, keydata, keylen, key);
    memset(keydata, 0, sizeof(keylen));
    free(keydata);

    return ret;
}


krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_prf_length(krb5_context context,
		       krb5_enctype type,
		       size_t *length)
{
    struct encryption_type *et = _find_enctype(type);

    if(et == NULL || et->prf_length == 0) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       N_("encryption type %d not supported", ""),
			       type);
	return KRB5_PROG_ETYPE_NOSUPP;
    }

    *length = et->prf_length;
    return 0;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_prf(krb5_context context,
		const krb5_crypto crypto,
		const krb5_data *input,
		krb5_data *output)
{
    struct encryption_type *et = crypto->et;

    krb5_data_zero(output);

    if(et->prf == NULL) {
	krb5_set_error_message(context, KRB5_PROG_ETYPE_NOSUPP,
			       "kerberos prf for %s not supported",
			       et->name);
	return KRB5_PROG_ETYPE_NOSUPP;
    }

    return (*et->prf)(context, crypto, input, output);
}

static krb5_error_code
krb5_crypto_prfplus(krb5_context context,
		    const krb5_crypto crypto,
		    const krb5_data *input,
		    size_t length,
		    krb5_data *output)
{
    krb5_error_code ret;
    krb5_data input2;
    unsigned char i = 1;
    unsigned char *p;

    krb5_data_zero(&input2);
    krb5_data_zero(output);

    krb5_clear_error_message(context);

    ret = krb5_data_alloc(output, length);
    if (ret) goto out;
    ret = krb5_data_alloc(&input2, input->length + 1);
    if (ret) goto out;

    krb5_clear_error_message(context);

    memcpy(((unsigned char *)input2.data) + 1, input->data, input->length);

    p = output->data;

    while (length) {
	krb5_data block;

	((unsigned char *)input2.data)[0] = i++;

	ret = krb5_crypto_prf(context, crypto, &input2, &block);
	if (ret)
	    goto out;

	if (block.length < length) {
	    memcpy(p, block.data, block.length);
	    length -= block.length;
	} else {
	    memcpy(p, block.data, length);
	    length = 0;
	}
	p += block.length;
	krb5_data_free(&block);
    }

 out:
    krb5_data_free(&input2);
    if (ret)
	krb5_data_free(output);
    return 0;
}

/**
 * The FX-CF2 key derivation function, used in FAST and preauth framework.
 *
 * @param context Kerberos 5 context
 * @param crypto1 first key to combine
 * @param crypto2 second key to combine
 * @param pepper1 factor to combine with first key to garante uniqueness
 * @param pepper1 factor to combine with second key to garante uniqueness
 * @param enctype the encryption type of the resulting key
 * @param res allocated key, free with krb5_free_keyblock_contents()
 *
 * @return Return an error code or 0.
 *
 * @ingroup krb5_crypto
 */

krb5_error_code KRB5_LIB_FUNCTION
krb5_crypto_fx_cf2(krb5_context context,
		   const krb5_crypto crypto1,
		   const krb5_crypto crypto2,
		   krb5_data *pepper1,
		   krb5_data *pepper2,
		   krb5_enctype enctype,
		   krb5_keyblock *res)
{
    krb5_error_code ret;
    krb5_data os1, os2;
    size_t i, keysize;

    memset(res, 0, sizeof(*res));

    ret = krb5_enctype_keysize(context, enctype, &keysize);
    if (ret)
	return ret;

    ret = krb5_data_alloc(&res->keyvalue, keysize);
    if (ret)
	goto out;
    ret = krb5_crypto_prfplus(context, crypto1, pepper1, keysize, &os1);
    if (ret)
	goto out;
    ret = krb5_crypto_prfplus(context, crypto2, pepper2, keysize, &os2);
    if (ret)
	goto out;

    res->keytype = enctype;
    {
	unsigned char *p1 = os1.data, *p2 = os2.data, *p3 = res->keyvalue.data;
	for (i = 0; i < keysize; i++)
	    p3[i] = p1[i] ^ p2[i];
    }
 out:
    if (ret)
	krb5_data_free(&res->keyvalue);
    krb5_data_free(&os1);
    krb5_data_free(&os2);

    return ret;
}



#ifndef HEIMDAL_SMALLER

krb5_error_code KRB5_LIB_FUNCTION
krb5_keytype_to_enctypes (krb5_context context,
			  krb5_keytype keytype,
			  unsigned *len,
			  krb5_enctype **val)
    KRB5_DEPRECATED
{
    int i;
    unsigned n = 0;
    krb5_enctype *ret;

    for (i = num_etypes - 1; i >= 0; --i) {
	if (etypes[i]->keytype->type == keytype
	    && !(etypes[i]->flags & F_PSEUDO)
	    && krb5_enctype_valid(context, etypes[i]->type) == 0)
	    ++n;
    }
    if (n == 0) {
	krb5_set_error_message(context, KRB5_PROG_KEYTYPE_NOSUPP,
			       "Keytype have no mapping");
	return KRB5_PROG_KEYTYPE_NOSUPP;
    }

    ret = malloc(n * sizeof(*ret));
    if (ret == NULL && n != 0) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    n = 0;
    for (i = num_etypes - 1; i >= 0; --i) {
	if (etypes[i]->keytype->type == keytype
	    && !(etypes[i]->flags & F_PSEUDO)
	    && krb5_enctype_valid(context, etypes[i]->type) == 0)
	    ret[n++] = etypes[i]->type;
    }
    *len = n;
    *val = ret;
    return 0;
}

/* if two enctypes have compatible keys */
krb5_boolean KRB5_LIB_FUNCTION
krb5_enctypes_compatible_keys(krb5_context context,
			      krb5_enctype etype1,
			      krb5_enctype etype2)
    KRB5_DEPRECATED
{
    struct encryption_type *e1 = _find_enctype(etype1);
    struct encryption_type *e2 = _find_enctype(etype2);
    return e1 != NULL && e2 != NULL && e1->keytype == e2->keytype;
}

#endif /* HEIMDAL_SMALLER */
