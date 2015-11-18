#ifndef NETTLE_TESTUTILS_H_INCLUDED
#define NETTLE_TESTUTILS_H_INCLUDED

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "nettle-types.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if WITH_HOGWEED
# include "rsa.h"
# include "dsa-compat.h"
# include "ecc-curve.h"
# include "ecc.h"
# include "ecc-internal.h"
# include "ecdsa.h"
# include "gmp-glue.h"
# if NETTLE_USE_MINI_GMP
#  include "knuth-lfib.h"
# endif

/* Undo dsa-compat name mangling */
#undef dsa_generate_keypair
#define dsa_generate_keypair nettle_dsa_generate_keypair
#endif /* WITH_HOGWEED */

#include "nettle-meta.h"

/* Forward declare */
struct nettle_aead;

#ifdef __cplusplus
extern "C" {
#endif

void
die(const char *format, ...) PRINTF_STYLE (1, 2) NORETURN;

void *
xalloc(size_t size);

struct tstring {
  struct tstring *next;
  size_t length;
  uint8_t data[1];
};

struct tstring *
tstring_alloc (size_t length);

void
tstring_clear(void);

struct tstring *
tstring_data(size_t length, const char *data);

struct tstring *
tstring_hex(const char *hex);

void
tstring_print_hex(const struct tstring *s);

/* Decodes a NUL-terminated hex string. */

void
print_hex(size_t length, const uint8_t *data);

/* The main program */
void
test_main(void);

extern int verbose;

/* FIXME: When interface stabilizes, move to nettle-meta.h */
struct nettle_mac
{
  const char *name;

  /* Size of the context struct */
  unsigned context_size;

  /* Size of digests */
  unsigned digest_size;

  /* Suggested key size; other sizes are sometimes possible. */
  unsigned key_size;
  
  nettle_set_key_func *set_key;
  nettle_hash_update_func *update;
  nettle_hash_digest_func *digest;
};

#define _NETTLE_HMAC(name, NAME, keysize) {	\
  #name,					\
  sizeof(struct hmac_##name##_ctx),		\
  NAME##_DIGEST_SIZE,				\
  NAME##_DIGEST_SIZE,				\
  hmac_##name##_set_key,			\
  hmac_##name##_update,				\
  hmac_##name##_digest,				\
}

/* Test functions deallocate their inputs when finished.*/
void
test_cipher(const struct nettle_cipher *cipher,
	    const struct tstring *key,
	    const struct tstring *cleartext,
	    const struct tstring *ciphertext);

void
test_cipher_cbc(const struct nettle_cipher *cipher,
		const struct tstring *key,
		const struct tstring *cleartext,
		const struct tstring *ciphertext,
		const struct tstring *iv);

void
test_cipher_ctr(const struct nettle_cipher *cipher,
		const struct tstring *key,
		const struct tstring *cleartext,
		const struct tstring *ciphertext,
		const struct tstring *iv);

void
test_cipher_stream(const struct nettle_cipher *cipher,
		   const struct tstring *key,
		   const struct tstring *cleartext,
		   const struct tstring *ciphertext);

void
test_aead(const struct nettle_aead *aead,
	  nettle_hash_update_func *set_nonce,
	  const struct tstring *key,
	  const struct tstring *authtext,
	  const struct tstring *cleartext,
	  const struct tstring *ciphertext,
	  const struct tstring *nonce,
	  const struct tstring *digest);

void
test_hash(const struct nettle_hash *hash,
	  const struct tstring *msg,
	  const struct tstring *digest);

void
test_hash_large(const struct nettle_hash *hash,
		size_t count, size_t length,
		uint8_t c,
		const struct tstring *digest);

void
test_armor(const struct nettle_armor *armor,
           size_t data_length,
           const uint8_t *data,
           const uint8_t *ascii);

#if WITH_HOGWEED
#ifndef mpn_zero_p
int
mpn_zero_p (mp_srcptr ap, mp_size_t n);
#endif

void
mpn_out_str (FILE *f, int base, const mp_limb_t *xp, mp_size_t xn);

#if NETTLE_USE_MINI_GMP
typedef struct knuth_lfib_ctx gmp_randstate_t[1];

void gmp_randinit_default (struct knuth_lfib_ctx *ctx);
#define gmp_randclear(state)
void mpz_urandomb (mpz_t r, struct knuth_lfib_ctx *ctx, mp_bitcnt_t bits);
/* This is cheating */
#define mpz_rrandomb mpz_urandomb

#endif /* NETTLE_USE_MINI_GMP */

mp_limb_t *
xalloc_limbs (mp_size_t n);

void
write_mpn (FILE *f, int base, const mp_limb_t *xp, mp_size_t n);

void
test_rsa_set_key_1(struct rsa_public_key *pub,
		   struct rsa_private_key *key);

void
test_rsa_md5(struct rsa_public_key *pub,
	     struct rsa_private_key *key,
	     mpz_t expected);

void
test_rsa_sha1(struct rsa_public_key *pub,
	      struct rsa_private_key *key,
	      mpz_t expected);

void
test_rsa_sha256(struct rsa_public_key *pub,
		struct rsa_private_key *key,
		mpz_t expected);

void
test_rsa_sha512(struct rsa_public_key *pub,
		struct rsa_private_key *key,
		mpz_t expected);

void
test_rsa_key(struct rsa_public_key *pub,
	     struct rsa_private_key *key);

void
test_dsa160(const struct dsa_public_key *pub,
	    const struct dsa_private_key *key,
	    const struct dsa_signature *expected);

void
test_dsa256(const struct dsa_public_key *pub,
	    const struct dsa_private_key *key,
	    const struct dsa_signature *expected);

#if 0
void
test_dsa_sign(const struct dsa_public_key *pub,
	      const struct dsa_private_key *key,
	      const struct nettle_hash *hash,
	      const struct dsa_signature *expected);
#endif

void
test_dsa_verify(const struct dsa_params *params,
		const mpz_t pub,
		const struct nettle_hash *hash,
		struct tstring *msg,
		const struct dsa_signature *ref);

void
test_dsa_key(const struct dsa_params *params,
	     const mpz_t pub,
	     const mpz_t key,
	     unsigned q_size);

extern const struct ecc_curve * const ecc_curves[];

struct ecc_ref_point
{
  const char *x;
  const char *y;
};

void
test_ecc_point (const struct ecc_curve *ecc,
		const struct ecc_ref_point *ref,
		const mp_limb_t *p);

void
test_ecc_mul_a (unsigned curve, unsigned n, const mp_limb_t *p);

void
test_ecc_mul_h (unsigned curve, unsigned n, const mp_limb_t *p);

#endif /* WITH_HOGWEED */
  
/* LDATA needs to handle NUL characters. */
#define LLENGTH(x) (sizeof(x) - 1)
#define LDATA(x) LLENGTH(x), x
#define LDUP(x) strlen(x), strdup(x)

#define SHEX(x) (tstring_hex(x))
#define SDATA(x) ((const struct tstring *)tstring_data(LLENGTH(x), x))
#define H(x) (SHEX(x)->data)

#define MEMEQ(length, a, b) (!memcmp((a), (b), (length)))

#define FAIL() abort()
#define SKIP() exit(77)

#define ASSERT(x) do {							\
    if (!(x))								\
      {									\
	fprintf(stderr, "Assert failed: %s:%d: %s\n", \
		__FILE__, __LINE__, #x);					\
	FAIL();								\
      }									\
  } while(0)

#ifdef __cplusplus
}
#endif

#endif /* NETTLE_TESTUTILS_H_INCLUDED */
