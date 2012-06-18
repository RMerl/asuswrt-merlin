/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <krb5-types.h>
#include <rfc2459_asn1.h>

#include <rsa.h>

#include <roken.h>

/**
 * @page page_rsa RSA - public-key cryptography
 *
 * RSA is named by its inventors (Ron Rivest, Adi Shamir, and Leonard
 * Adleman) (published in 1977), patented expired in 21 September 2000.
 *
 * See the library functions here: @ref hcrypto_rsa
 */

/**
 * Same as RSA_new_method() using NULL as engine.
 *
 * @return a newly allocated RSA object. Free with RSA_free().
 *
 * @ingroup hcrypto_rsa
 */

RSA *
RSA_new(void)
{
    return RSA_new_method(NULL);
}

/**
 * Allocate a new RSA object using the engine, if NULL is specified as
 * the engine, use the default RSA engine as returned by
 * ENGINE_get_default_RSA().
 *
 * @param engine Specific what ENGINE RSA provider should be used.
 *
 * @return a newly allocated RSA object. Free with RSA_free().
 *
 * @ingroup hcrypto_rsa
 */

RSA *
RSA_new_method(ENGINE *engine)
{
    RSA *rsa;

    rsa = calloc(1, sizeof(*rsa));
    if (rsa == NULL)
	return NULL;

    rsa->references = 1;

    if (engine) {
	ENGINE_up_ref(engine);
	rsa->engine = engine;
    } else {
	rsa->engine = ENGINE_get_default_RSA();
    }

    if (rsa->engine) {
	rsa->meth = ENGINE_get_RSA(rsa->engine);
	if (rsa->meth == NULL) {
	    ENGINE_finish(engine);
	    free(rsa);
	    return 0;
	}
    }

    if (rsa->meth == NULL)
	rsa->meth = rk_UNCONST(RSA_get_default_method());

    (*rsa->meth->init)(rsa);

    return rsa;
}

/**
 * Free an allocation RSA object.
 *
 * @param rsa the RSA object to free.
 * @ingroup hcrypto_rsa
 */

void
RSA_free(RSA *rsa)
{
    if (rsa->references <= 0)
	abort();

    if (--rsa->references > 0)
	return;

    (*rsa->meth->finish)(rsa);

    if (rsa->engine)
	ENGINE_finish(rsa->engine);

#define free_if(f) if (f) { BN_free(f); }
    free_if(rsa->n);
    free_if(rsa->e);
    free_if(rsa->d);
    free_if(rsa->p);
    free_if(rsa->q);
    free_if(rsa->dmp1);
    free_if(rsa->dmq1);
    free_if(rsa->iqmp);
#undef free_if

    memset(rsa, 0, sizeof(*rsa));
    free(rsa);
}

/**
 * Add an extra reference to the RSA object. The object should be free
 * with RSA_free() to drop the reference.
 *
 * @param rsa the object to add reference counting too.
 *
 * @return the current reference count, can't safely be used except
 * for debug printing.
 *
 * @ingroup hcrypto_rsa
 */

int
RSA_up_ref(RSA *rsa)
{
    return ++rsa->references;
}

/**
 * Return the RSA_METHOD used for this RSA object.
 *
 * @param rsa the object to get the method from.
 *
 * @return the method used for this RSA object.
 *
 * @ingroup hcrypto_rsa
 */

const RSA_METHOD *
RSA_get_method(const RSA *rsa)
{
    return rsa->meth;
}

/**
 * Set a new method for the RSA keypair.
 *
 * @param rsa rsa parameter.
 * @param method the new method for the RSA parameter.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_rsa
 */

int
RSA_set_method(RSA *rsa, const RSA_METHOD *method)
{
    (*rsa->meth->finish)(rsa);

    if (rsa->engine) {
	ENGINE_finish(rsa->engine);
	rsa->engine = NULL;
    }

    rsa->meth = method;
    (*rsa->meth->init)(rsa);
    return 1;
}

/**
 * Set the application data for the RSA object.
 *
 * @param rsa the rsa object to set the parameter for
 * @param arg the data object to store
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_rsa
 */

int
RSA_set_app_data(RSA *rsa, void *arg)
{
    rsa->ex_data.sk = arg;
    return 1;
}

/**
 * Get the application data for the RSA object.
 *
 * @param rsa the rsa object to get the parameter for
 *
 * @return the data object
 *
 * @ingroup hcrypto_rsa
 */

void *
RSA_get_app_data(RSA *rsa)
{
    return rsa->ex_data.sk;
}

int
RSA_check_key(const RSA *key)
{
    static const unsigned char inbuf[] = "hello, world!";
    RSA *rsa = rk_UNCONST(key);
    void *buffer;
    int ret;

    /*
     * XXX I have no clue how to implement this w/o a bignum library.
     * Well, when we have a RSA key pair, we can try to encrypt/sign
     * and then decrypt/verify.
     */

    if ((rsa->d == NULL || rsa->n == NULL) &&
	(rsa->p == NULL || rsa->q || rsa->dmp1 == NULL || rsa->dmq1 == NULL || rsa->iqmp == NULL))
	return 0;

    buffer = malloc(RSA_size(rsa));
    if (buffer == NULL)
	return 0;

    ret = RSA_private_encrypt(sizeof(inbuf), inbuf, buffer,
			     rsa, RSA_PKCS1_PADDING);
    if (ret == -1) {
	free(buffer);
	return 0;
    }

    ret = RSA_public_decrypt(ret, buffer, buffer,
			      rsa, RSA_PKCS1_PADDING);
    if (ret == -1) {
	free(buffer);
	return 0;
    }

    if (ret == sizeof(inbuf) && memcmp(buffer, inbuf, sizeof(inbuf)) == 0) {
	free(buffer);
	return 1;
    }
    free(buffer);
    return 0;
}

int
RSA_size(const RSA *rsa)
{
    return BN_num_bytes(rsa->n);
}

#define RSAFUNC(name, body) \
int \
name(int flen,const unsigned char* f, unsigned char* t, RSA* r, int p){\
    return body; \
}

RSAFUNC(RSA_public_encrypt, (r)->meth->rsa_pub_enc(flen, f, t, r, p))
RSAFUNC(RSA_public_decrypt, (r)->meth->rsa_pub_dec(flen, f, t, r, p))
RSAFUNC(RSA_private_encrypt, (r)->meth->rsa_priv_enc(flen, f, t, r, p))
RSAFUNC(RSA_private_decrypt, (r)->meth->rsa_priv_dec(flen, f, t, r, p))

/* XXX */
int
RSA_sign(int type, const unsigned char *from, unsigned int flen,
	 unsigned char *to, unsigned int *tlen, RSA *rsa)
{
    return -1;
}

int
RSA_verify(int type, const unsigned char *from, unsigned int flen,
	   unsigned char *to, unsigned int tlen, RSA *rsa)
{
    return -1;
}

/*
 * A NULL RSA_METHOD that returns failure for all operations. This is
 * used as the default RSA method if we don't have any native
 * support.
 */

static RSAFUNC(null_rsa_public_encrypt, -1)
static RSAFUNC(null_rsa_public_decrypt, -1)
static RSAFUNC(null_rsa_private_encrypt, -1)
static RSAFUNC(null_rsa_private_decrypt, -1)

/*
 *
 */

int
RSA_generate_key_ex(RSA *r, int bits, BIGNUM *e, BN_GENCB *cb)
{
    if (r->meth->rsa_keygen)
	return (*r->meth->rsa_keygen)(r, bits, e, cb);
    return 0;
}


/*
 *
 */

static int
null_rsa_init(RSA *rsa)
{
    return 1;
}

static int
null_rsa_finish(RSA *rsa)
{
    return 1;
}

static const RSA_METHOD rsa_null_method = {
    "hcrypto null RSA",
    null_rsa_public_encrypt,
    null_rsa_public_decrypt,
    null_rsa_private_encrypt,
    null_rsa_private_decrypt,
    NULL,
    NULL,
    null_rsa_init,
    null_rsa_finish,
    0,
    NULL,
    NULL,
    NULL
};

const RSA_METHOD *
RSA_null_method(void)
{
    return &rsa_null_method;
}

extern const RSA_METHOD hc_rsa_imath_method;
#ifdef HAVE_GMP
static const RSA_METHOD *default_rsa_method = &hc_rsa_gmp_method;
#else
static const RSA_METHOD *default_rsa_method = &hc_rsa_imath_method;
#endif

const RSA_METHOD *
RSA_get_default_method(void)
{
    return default_rsa_method;
}

void
RSA_set_default_method(const RSA_METHOD *meth)
{
    default_rsa_method = meth;
}

/*
 *
 */

static BIGNUM *
heim_int2BN(const heim_integer *i)
{
    BIGNUM *bn;

    bn = BN_bin2bn(i->data, i->length, NULL);
    if (bn)
	BN_set_negative(bn, i->negative);
    return bn;
}

static int
bn2heim_int(BIGNUM *bn, heim_integer *integer)
{
    integer->length = BN_num_bytes(bn);
    integer->data = malloc(integer->length);
    if (integer->data == NULL) {
	integer->length = 0;
	return ENOMEM;
    }
    BN_bn2bin(bn, integer->data);
    integer->negative = BN_is_negative(bn);
    return 0;
}


RSA *
d2i_RSAPrivateKey(RSA *rsa, const unsigned char **pp, size_t len)
{
    RSAPrivateKey data;
    RSA *k = rsa;
    size_t size;
    int ret;

    ret = decode_RSAPrivateKey(*pp, len, &data, &size);
    if (ret)
	return NULL;

    *pp += size;

    if (k == NULL) {
	k = RSA_new();
	if (k == NULL) {
	    free_RSAPrivateKey(&data);
	    return NULL;
	}
    }

    k->n = heim_int2BN(&data.modulus);
    k->e = heim_int2BN(&data.publicExponent);
    k->d = heim_int2BN(&data.privateExponent);
    k->p = heim_int2BN(&data.prime1);
    k->q = heim_int2BN(&data.prime2);
    k->dmp1 = heim_int2BN(&data.exponent1);
    k->dmq1 = heim_int2BN(&data.exponent2);
    k->iqmp = heim_int2BN(&data.coefficient);
    free_RSAPrivateKey(&data);

    if (k->n == NULL || k->e == NULL || k->d == NULL || k->p == NULL ||
	k->q == NULL || k->dmp1 == NULL || k->dmq1 == NULL || k->iqmp == NULL)
    {
	RSA_free(k);
	return NULL;
    }
	
    return k;
}

int
i2d_RSAPrivateKey(RSA *rsa, unsigned char **pp)
{
    RSAPrivateKey data;
    size_t size;
    int ret;

    if (rsa->n == NULL || rsa->e == NULL || rsa->d == NULL || rsa->p == NULL ||
	rsa->q == NULL || rsa->dmp1 == NULL || rsa->dmq1 == NULL ||
	rsa->iqmp == NULL)
	return -1;

    memset(&data, 0, sizeof(data));

    ret  = bn2heim_int(rsa->n, &data.modulus);
    ret |= bn2heim_int(rsa->e, &data.publicExponent);
    ret |= bn2heim_int(rsa->d, &data.privateExponent);
    ret |= bn2heim_int(rsa->p, &data.prime1);
    ret |= bn2heim_int(rsa->q, &data.prime2);
    ret |= bn2heim_int(rsa->dmp1, &data.exponent1);
    ret |= bn2heim_int(rsa->dmq1, &data.exponent2);
    ret |= bn2heim_int(rsa->iqmp, &data.coefficient);
    if (ret) {
	free_RSAPrivateKey(&data);
	return -1;
    }

    if (pp == NULL) {
	size = length_RSAPrivateKey(&data);
	free_RSAPrivateKey(&data);
    } else {
	void *p;
	size_t len;

	ASN1_MALLOC_ENCODE(RSAPrivateKey, p, len, &data, &size, ret);
	free_RSAPrivateKey(&data);
	if (ret)
	    return -1;
	if (len != size)
	    abort();

	memcpy(*pp, p, size);
	free(p);

	*pp += size;

    }
    return size;
}

int
i2d_RSAPublicKey(RSA *rsa, unsigned char **pp)
{
    RSAPublicKey data;
    size_t size;
    int ret;

    memset(&data, 0, sizeof(data));

    if (bn2heim_int(rsa->n, &data.modulus) ||
	bn2heim_int(rsa->e, &data.publicExponent))
    {
	free_RSAPublicKey(&data);
	return -1;
    }

    if (pp == NULL) {
	size = length_RSAPublicKey(&data);
	free_RSAPublicKey(&data);
    } else {
	void *p;
	size_t len;

	ASN1_MALLOC_ENCODE(RSAPublicKey, p, len, &data, &size, ret);
	free_RSAPublicKey(&data);
	if (ret)
	    return -1;
	if (len != size)
	    abort();

	memcpy(*pp, p, size);
	free(p);

	*pp += size;
    }

    return size;
}
