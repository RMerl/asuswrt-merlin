/* hogweed-benchmark.c

   Copyright (C) 2013, 2014 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <time.h>

#include "timing.h"

#include "dsa.h"
#include "rsa.h"
#include "curve25519.h"

#include "nettle-meta.h"
#include "sexp.h"
#include "knuth-lfib.h"

#include "../ecdsa.h"
#include "../ecc-internal.h"
#include "../gmp-glue.h"

#if WITH_OPENSSL
#include <openssl/rsa.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/objects.h>
#endif

#define BENCH_INTERVAL 0.1

static void NORETURN PRINTF_STYLE(1,2)
die(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  exit(EXIT_FAILURE);
}

static void *
xalloc (size_t size)
{
  void *p = malloc (size);
  if (!p)
    {
      fprintf (stderr, "Virtual memory exhausted\n");
      abort ();
    }
  return p;
}

static uint8_t *
hash_string (const struct nettle_hash *hash,
	     unsigned length, const char *s)
{
  void *ctx = xalloc (hash->context_size);
  uint8_t *digest = xalloc (hash->digest_size);
  hash->init (ctx);
  hash->update (ctx, length, s);
  hash->digest (ctx, hash->digest_size, digest);
  free (ctx);

  return digest;
}

struct alg {
  const char *name;
  unsigned size;
  void *(*init) (unsigned size);
  void (*sign)  (void *);
  void (*verify)(void *);
  void (*clear) (void *);
};

/* Returns second per function call */
static double
time_function(void (*f)(void *arg), void *arg)
{
  unsigned ncalls;
  double elapsed;

  /* Warm up */
  f(arg);
  for (ncalls = 10 ;;)
    {
      unsigned i;

      time_start();
      for (i = 0; i < ncalls; i++)
	f(arg);
      elapsed = time_end();
      if (elapsed > BENCH_INTERVAL)
	break;
      else if (elapsed < BENCH_INTERVAL / 10)
	ncalls *= 10;
      else
	ncalls *= 2;
    }
  return elapsed / ncalls;
}

static void 
bench_alg (const struct alg *alg)
{
  double sign;
  double verify;
  void *ctx;

  ctx = alg->init(alg->size);
  if (ctx == NULL)
    {
      printf("%15s %4d N/A\n", alg->name, alg->size);
      return;
    }

  sign = time_function (alg->sign, ctx);
  verify = time_function (alg->verify, ctx);

  alg->clear (ctx);

  printf("%15s %4d %9.4f %9.4f\n",
	 alg->name, alg->size, 1e-3/sign, 1e-3/verify);
}

struct rsa_ctx
{
  struct rsa_public_key pub;
  struct rsa_private_key key;
  uint8_t *digest;
  mpz_t s;
};

static void *
bench_rsa_init (unsigned size)
{
  char rsa1024[] = 
    "{KDExOnByaXZhdGUta2V5KDE0OnJzYS1wa2NzMS1zaGExKDE6bjEyOToA90+K5EmjbFJBeJD"
    " xP2KD2Df+0Twc9425uB+vhqTrVijtd2PnwEQDfR2VoducgkKcXJzYYyCNILQJbFAi2Km/sD"
    " jImERBqDtaI217Ze+tOKEmImexYTAgFuqEptp2F3M4DqgRQ7s/3nJQ/bPE5Hfi1OZhJSShu"
    " I80ATTU4fUgrPspKDE6ZTM6AQABKSgxOmQxMjk6APAhKckzvxxkWfHJOpXDACWnaSKcbbvo"
    " vtWK3pGr/F2ya7CrLtE+uOx5F1sLs9G+/7flCy5k4uNILIYg4VTirZ1zQ8fNKPrjK1VMRls"
    " JiRRU/0VAs9d7HdncJfs6rbvRQbCRSRYURo4hWir3Lq8V3UUQVBprc4dO+uWmplvwQ5qxKS"
    " gxOnA2NToA+8aIVkdbk8Jg8dJOuzc7m/hZnwkKSs6eVDw4N/2T0CJKGJYT+B3Ct+fPkxhUR"
    " ggd2DQ9OpkTra7SXHTNkzdPVSkoMTpxNjU6APt11P8vXNnGYF0OC/cPsR8zhSYuFmtRuX6G"
    " ES+DdG0VCU07UeNQkok1UoW5sXqY0IGr1jkJq8AMSgKoNbLQ6w8pKDE6YTY0Ohzkxsan/8F"
    " wQDHgQbrIduXKVXaj0fONzKu8EXOTfUAYf0pdBsOlnq/+QVsPIrS6v7oNHK253YFEG84SdX"
    " kcktUpKDE6YjY1OgCR+cRtY3RWY+f6/TWK9gwPndv03xpasLWrMm71ky1aSbT9pasS9+opR"
    " tAiGzthfSbFsBiLQgb3VOr+AeIybT+XKSgxOmM2NDojigqARWN5u1CVDVuD2L2ManpoGiM6"
    " kQ6FaJjqRjxeRRKFrQxGJa9tM1hqStxokC1oJidgaOLGnn60iwzToug9KSkp}";
    
  char rsa2048[] =
    "{KDExOnByaXZhdGUta2V5KDE0OnJzYS1wa2NzMS1zaGExKDE6bjI1NzoAtxWXiglIdunDK48"
    " 8I0vW0wTqnh/riW9pLk8n1F8MUPBFdhvkkl0bDQqSJPUvSHy+w4fLVwcEzeI4qFyo3b2Avz"
    " JK20MFbt/WfHD1TbxuK8rNqXyqmqjJ9vgjtV9nPzAz7CM9ogs3/RJHpcfZPQF15ifizleUZ"
    " aQT0GAXHZL7cePj10yGI2u3hgTkokVzdNC/1T34guKYpErg0pt0B/KejWpsFTb84z3tkR+B"
    " YVx07p/OoByZwoABgncS/uALl31fRS8jyJ2JqUiZOqe7XoO9hkDHYNCWUGUfNGQ7ZgVp9+e"
    " NQpracSjrp6Jnrj7r/oxJUx5ZDVNi18AzQadE/oKOrSkoMTplMzoBAAEpKDE6ZDI1NjogBT"
    " C5vaHk2kF+LtDvw2XRBj0aZq7FHK0ioklvBSicR0l+vKYfSxVeFIk22YLphJfAjtFraRjYA"
    " Uaze3E1Rt1rkxoweupKV++lWAQvElOaaR/LErirz/Vysjdck1D1ZjLOi+NNofSq2DWbsvY1"
    " iznZhQRP3lVf6XBls0iXrYs4gb0pBZqXLQW+j9Ihx6eantf1/6ZMKPgCkzcAZ0ABsCfaFSg"
    " ouNCzilblsgFEspEbb8QqrNQoStS3F/gMwWgDsr3+vQzBqt+7ykWoCJ9wctbYy9kEPq+hX3"
    " GP0hG6HdS81r9E8pgdf3wloNNMzYUHwn7poXGpOi8tG0pmR56TqD/BKSgxOnAxMjk6AN4AJ"
    " TiGPm9We2ga3Y0jpTfA3mWpUbhYgaXYLWA1/riniwq16fqxRIkWQT/O2KKpBVe6WSvNYq9u"
    " lM8N6bdPtDytJs6AOXy0X5vtJ953ZYVMhHbhmUxhIL9I+s0O1+LxMF8b9U4CrFyaTxd8Un/"
    " FXP1BvYJRrkoup6HYvOlGx36lKSgxOnExMjk6ANMfrfH6z/3o7K56aW6kSiloDDbKZQ0+W5"
    " 8LzP2ZOBLf6LX6jLhN3olU1Z0KGTM0S/1AxvwGjuRqhu+LcOJ7oUCUH3uusR5c5nSnArYPq"
    " +0wbco4BQngot/HmGN7U0EDsIWqPt/qoa/b8bCk+TOwJlknNq/PnZU26SPj48XS05lpKSgx"
    " OmExMjk6AJM2n3gLNW3ZeH5BindkktQU9qWNkV5geqDCaNyrEZ3bpI1WsrEGSj9p3Zz1ipz"
    " a3msdbLJqQS26c72WKUzg8tFltR0s1HJInjolGtIgdNbfNdwrn9+RbQjL2VyPokOg0wXO4W"
    " 14wlmqDhax33dRJmfe50964MvaglkGA8fhorrtKSgxOmIxMjk6AKMe+vrX2xRHf3dfxU5jS"
    " ZmsdqNuxZzx7UB5kazvUU/kCJ1yNH/CSoq5LULkpovVgFDwV84qEwWQ+SjkCBg1hWWsDJc3"
    " ZkobZUQENigM+72LiYiQt/PlyHI2eRuEEdNN0nm0DFhdpQeHXLoq/RBerYJ8tdgpBYxgnMn"
    " KLhaOykbhKSgxOmMxMjg6MVlKj2bjb7qFQVkLO1OPg28jSrtRpnQCR+qegN4ZmNam/qbest"
    " 8yn0JQ6gxX7PvP382+jx7uHHWHYYqPq/Flf8gqtOOcjqS5TJgVHz3F3xHWquo1ZofGtCMro"
    " Dd2c0xjRjIVGvLV6Ngs+HRdljRav40vRpTyEoEdlzHBQiILesopKSk=}";

  struct rsa_ctx *ctx;
  struct sexp_iterator i;

  int res;

  ctx = xalloc(sizeof(*ctx));

  rsa_public_key_init (&ctx->pub);
  rsa_private_key_init (&ctx->key);
  mpz_init (ctx->s);

  /* NOTE: Base64-decodes the strings in-place */
  if (size == 1024)
    res = sexp_transport_iterator_first (&i, sizeof(rsa1024) - 1, rsa1024);
  else if (size == 2048)
    res = sexp_transport_iterator_first (&i, sizeof(rsa2048) - 1, rsa2048);
  else
    die ("Internal error.\n");

  if (! (res
	 && sexp_iterator_check_type (&i, "private-key")
	 && sexp_iterator_check_type (&i, "rsa-pkcs1-sha1")
	 && rsa_keypair_from_sexp_alist (&ctx->pub, &ctx->key, 0, &i)))
    die ("Internal error.\n");

  ctx->digest = hash_string (&nettle_sha256, 3, "foo");

  rsa_sha256_sign_digest (&ctx->key, ctx->digest, ctx->s);
  
  return ctx;
}

static void
bench_rsa_sign (void *p)
{
  struct rsa_ctx *ctx = p;

  mpz_t s;
  mpz_init (s);
  rsa_sha256_sign_digest (&ctx->key, ctx->digest, s);
  mpz_clear (s);
}

static void
bench_rsa_verify (void *p)
{
  struct rsa_ctx *ctx = p;
  if (! rsa_sha256_verify_digest (&ctx->pub, ctx->digest, ctx->s))
    die ("Internal error, rsa_sha256_verify_digest failed.\n");
}

static void
bench_rsa_clear (void *p)
{
  struct rsa_ctx *ctx = p;

  rsa_public_key_clear (&ctx->pub);
  rsa_private_key_clear (&ctx->key);
  mpz_clear (ctx->s);
  
  free (ctx->digest);
  free (ctx);
}

struct dsa_ctx
{
  struct dsa_params params;
  mpz_t pub;
  mpz_t key;
  struct knuth_lfib_ctx lfib;
  struct dsa_signature s;
  uint8_t *digest;
};

static void *
bench_dsa_init (unsigned size)
{
  struct dsa_ctx *ctx;
  struct sexp_iterator i;  

  char dsa1024[] =
    "{KDExOnByaXZhdGUta2V5KDM6ZHNhKDE6cDEyOToA2q4hOXEClLMXXMOl9xaPcGC/GeGmCMv"
    " VCaaW0uWc50DvvmJDPQPdCehyfZr/1dv2UDbx06TC7ls/IFd+BsDzGBRxqIQ44J20cn+0gt"
    " NMIXAocE1QhCCFaT5gXrk8zMlqBEGaP3RdpgxNanEXkTj2Wma8r1GtrLX3HPafio62jicpK"
    " DE6cTIxOgDN9pcW3exdVAesC9WsxwCGoJK24ykoMTpnMTI5OgCJr9DmKdiE0WJZB7HACESv"
    " Tpg1qZgc8E15byQ+OsHUyOTRrJRTcrgKZJW7dFRJ9cXmyi7XYCd3bJtu/2HRHLY1vd4qMvU"
    " 7Y8x08ooCoABGV7nGqcmdQfEho1OY6TZh2NikmPKZLeur3PZFpcZ8Dl+KVWtwC55plNC7Om"
    " iAQy8MaCkoMTp5MTI5OgDakk0LOUQwzOKt9FHOBmBKrWsvTm7549eScTUqH4OMm3btjUsXz"
    " MmlqEe+imwQCOW/AE3Xw9pXZeETWK0jlLe8k5vnKcNskerFwZ1eQKtOPPQty8IqQ9PEfF6B"
    " 0oVQiJg2maHUDWFnDkIBd7ZR1z8FnZMUxH9mH4kEUo6YQgtCdykoMTp4MjA6cOl3ijiiMjI"
    " pesFD8jxESWb2mn8pKSk=}";

  ctx = xalloc(sizeof(*ctx));

  dsa_params_init (&ctx->params);
  mpz_init (ctx->pub);
  mpz_init (ctx->key);
  dsa_signature_init (&ctx->s);
  knuth_lfib_init (&ctx->lfib, 1);

  if (size != 1024)
    die ("Internal error.\n");
  
  if (! (sexp_transport_iterator_first (&i, sizeof(dsa1024) - 1, dsa1024)
	 && sexp_iterator_check_type (&i, "private-key")
	 && sexp_iterator_check_type (&i, "dsa")
	 && dsa_keypair_from_sexp_alist (&ctx->params, ctx->pub, ctx->key,
					 0, DSA_SHA1_Q_BITS, &i)) )
    die ("Internal error.\n");

  ctx->digest = hash_string (&nettle_sha1, 3, "foo");

  dsa_sign (&ctx->params, ctx->key,
	    &ctx->lfib, (nettle_random_func *)knuth_lfib_random,
	    SHA1_DIGEST_SIZE, ctx->digest, &ctx->s);

  return ctx;
}

static void
bench_dsa_sign (void *p)
{
  struct dsa_ctx *ctx = p;
  struct dsa_signature s;

  dsa_signature_init (&s);
  dsa_sign (&ctx->params, ctx->key,
	    &ctx->lfib, (nettle_random_func *)knuth_lfib_random,
	    SHA1_DIGEST_SIZE, ctx->digest, &s);
  dsa_signature_clear (&s);
}

static void
bench_dsa_verify (void *p)
{
  struct dsa_ctx *ctx = p;
  if (! dsa_verify (&ctx->params, ctx->pub, SHA1_DIGEST_SIZE, ctx->digest, &ctx->s))
    die ("Internal error, dsa_sha1_verify_digest failed.\n");
}

static void
bench_dsa_clear (void *p)
{
  struct dsa_ctx *ctx = p;
  dsa_params_clear (&ctx->params);
  mpz_clear (ctx->pub);
  mpz_clear (ctx->key);
  dsa_signature_clear (&ctx->s);
  free (ctx->digest);
  free (ctx);
}

struct ecdsa_ctx
{
  struct ecc_point pub;
  struct ecc_scalar key;
  struct knuth_lfib_ctx rctx;
  unsigned digest_size;
  uint8_t *digest;
  struct dsa_signature s;
};
  
static void *
bench_ecdsa_init (unsigned size)
{
  struct ecdsa_ctx *ctx;
  const struct ecc_curve *ecc;

  const char *xs;
  const char *ys;
  const char *zs;
  mpz_t x, y, z;
  
  ctx = xalloc (sizeof(*ctx));

  dsa_signature_init (&ctx->s);  
  knuth_lfib_init (&ctx->rctx, 17);

  switch (size)
    {
    case 192:
      ecc = &nettle_secp_192r1;
      xs = "8e8e07360350fb6b7ad8370cfd32fa8c6bba785e6e200599";
      ys = "7f82ddb58a43d59ff8dc66053002b918b99bd01bd68d6736";
      zs = "f2e620e086d658b4b507996988480917640e4dc107808bdd";
      ctx->digest = hash_string (&nettle_sha1, 3, "abc");
      ctx->digest_size = 20;
      break;
    case 224:
      ecc = &nettle_secp_224r1;
      xs = "993bf363f4f2bc0f255f22563980449164e9c894d9efd088d7b77334";
      ys = "b75fff9849997d02d135140e4d0030944589586e22df1fc4b629082a";
      zs = "cdfd01838247f5de3cc70b688418046f10a2bfaca6de9ec836d48c27";
      ctx->digest = hash_string (&nettle_sha224, 3, "abc");
      ctx->digest_size = 28;
      break;

      /* From RFC 4754 */
    case 256:
      ecc = &nettle_secp_256r1;
      xs = "2442A5CC 0ECD015F A3CA31DC 8E2BBC70 BF42D60C BCA20085 E0822CB0 4235E970";
      ys = "6FC98BD7 E50211A4 A27102FA 3549DF79 EBCB4BF2 46B80945 CDDFE7D5 09BBFD7D";
      zs = "DC51D386 6A15BACD E33D96F9 92FCA99D A7E6EF09 34E70975 59C27F16 14C88A7F";
      ctx->digest = hash_string (&nettle_sha256, 3, "abc");
      ctx->digest_size = 32;
      break;
    case 384:
      ecc = &nettle_secp_384r1;
      xs = "96281BF8 DD5E0525 CA049C04 8D345D30 82968D10 FEDF5C5A CA0C64E6 465A97EA"
	"5CE10C9D FEC21797 41571072 1F437922";
      ys = "447688BA 94708EB6 E2E4D59F 6AB6D7ED FF9301D2 49FE49C3 3096655F 5D502FAD"
	"3D383B91 C5E7EDAA 2B714CC9 9D5743CA";
      zs = "0BEB6466 34BA8773 5D77AE48 09A0EBEA 865535DE 4C1E1DCB 692E8470 8E81A5AF"
	"62E528C3 8B2A81B3 5309668D 73524D9F";
      ctx->digest = hash_string (&nettle_sha384, 3, "abc");
      ctx->digest_size = 48;
      break;
    case 521:
      ecc = &nettle_secp_521r1;
      xs = "0151518F 1AF0F563 517EDD54 85190DF9 5A4BF57B 5CBA4CF2 A9A3F647 4725A35F"
	"7AFE0A6D DEB8BEDB CD6A197E 592D4018 8901CECD 650699C9 B5E456AE A5ADD190"
	"52A8";
      ys = "006F3B14 2EA1BFFF 7E2837AD 44C9E4FF 6D2D34C7 3184BBAD 90026DD5 E6E85317"
	"D9DF45CA D7803C6C 20035B2F 3FF63AFF 4E1BA64D 1C077577 DA3F4286 C58F0AEA"
	"E643";
      zs = "0065FDA3 409451DC AB0A0EAD 45495112 A3D813C1 7BFD34BD F8C1209D 7DF58491"
	"20597779 060A7FF9 D704ADF7 8B570FFA D6F062E9 5C7E0C5D 5481C5B1 53B48B37"
	"5FA1";

      ctx->digest = hash_string (&nettle_sha512, 3, "abc");
      ctx->digest_size = 64;
      break;
    default:
      die ("Internal error.\n");
    }
  ecc_point_init (&ctx->pub, ecc);
  ecc_scalar_init (&ctx->key, ecc);

  mpz_init_set_str (x, xs, 16);
  mpz_init_set_str (y, ys, 16);
  mpz_init_set_str (z, zs, 16);

  ecc_point_set (&ctx->pub, x, y);
  ecc_scalar_set (&ctx->key, z);

  mpz_clear (x);
  mpz_clear (y);
  mpz_clear (z);

  ecdsa_sign (&ctx->key,
	      &ctx->rctx, (nettle_random_func *) knuth_lfib_random,
	      ctx->digest_size, ctx->digest,
	      &ctx->s);

  return ctx;
}

static void
bench_ecdsa_sign (void *p)
{
  struct ecdsa_ctx *ctx = p;
  struct dsa_signature s;

  dsa_signature_init (&s);
  ecdsa_sign (&ctx->key,
	      &ctx->rctx, (nettle_random_func *) knuth_lfib_random,
	      ctx->digest_size, ctx->digest,
	      &s);
  dsa_signature_clear (&s);
}

static void
bench_ecdsa_verify (void *p)
{
  struct ecdsa_ctx *ctx = p;
  if (! ecdsa_verify (&ctx->pub, 
		      ctx->digest_size, ctx->digest,
		      &ctx->s))
    die ("Internal error, _ecdsa_verify failed.\n");    
}

static void
bench_ecdsa_clear (void *p)
{
  struct ecdsa_ctx *ctx = p;

  ecc_point_clear (&ctx->pub);
  ecc_scalar_clear (&ctx->key);
  dsa_signature_clear (&ctx->s);
  free (ctx->digest);

  free (ctx);
}

#if WITH_OPENSSL
struct openssl_rsa_ctx
{
  RSA *key;
  unsigned char *ref;
  unsigned char *signature;
  unsigned int siglen;
  uint8_t *digest;
};

static void *
bench_openssl_rsa_init (unsigned size)
{
  struct openssl_rsa_ctx *ctx = xalloc (sizeof (*ctx));

  ctx->key = RSA_generate_key (size, 65537, NULL, NULL);
  ctx->ref = xalloc (RSA_size (ctx->key));
  ctx->signature = xalloc (RSA_size (ctx->key));
  ctx->digest = hash_string (&nettle_sha1, 3, "foo");
  RSA_blinding_off(ctx->key);

  if (! RSA_sign (NID_sha1, ctx->digest, SHA1_DIGEST_SIZE,
		  ctx->ref, &ctx->siglen, ctx->key))
    die ("OpenSSL RSA_sign failed.\n");

  return ctx;
}

static void
bench_openssl_rsa_sign (void *p)
{
  const struct openssl_rsa_ctx *ctx = p;
  unsigned siglen;

  if (! RSA_sign (NID_sha1, ctx->digest, SHA1_DIGEST_SIZE,
		  ctx->signature, &siglen, ctx->key))
    die ("OpenSSL RSA_sign failed.\n");
}

static void
bench_openssl_rsa_verify (void *p)
{
  const struct openssl_rsa_ctx *ctx = p;
  if (! RSA_verify (NID_sha1, ctx->digest, SHA1_DIGEST_SIZE,
		    ctx->ref, ctx->siglen, ctx->key))
    die ("OpenSSL RSA_verify failed.\n");    
}

static void
bench_openssl_rsa_clear (void *p)
{
  struct openssl_rsa_ctx *ctx = p;
  RSA_free (ctx->key);
  free (ctx->ref);
  free (ctx->signature);
  free (ctx->digest);
  free (ctx);
}

struct openssl_ecdsa_ctx
{
  EC_KEY *key;
  ECDSA_SIG *signature;
  unsigned digest_length;
  uint8_t *digest;
};

static void *
bench_openssl_ecdsa_init (unsigned size)
{
  struct openssl_ecdsa_ctx *ctx = xalloc (sizeof (*ctx));

  switch (size)
    {
    case 192:
      ctx->key = EC_KEY_new_by_curve_name (NID_X9_62_prime192v1);
      ctx->digest_length = 24; /* truncated */
      ctx->digest = hash_string (&nettle_sha224, 3, "abc");
      break;
    case 224:
      ctx->key = EC_KEY_new_by_curve_name (NID_secp224r1);
      ctx->digest_length = SHA224_DIGEST_SIZE;
      ctx->digest = hash_string (&nettle_sha224, 3, "abc");
      break;
    case 256:
      ctx->key = EC_KEY_new_by_curve_name (NID_X9_62_prime256v1);
      ctx->digest_length = SHA256_DIGEST_SIZE;
      ctx->digest = hash_string (&nettle_sha256, 3, "abc");
      break;
    case 384:
      ctx->key = EC_KEY_new_by_curve_name (NID_secp384r1);
      ctx->digest_length = SHA384_DIGEST_SIZE;
      ctx->digest = hash_string (&nettle_sha384, 3, "abc");
      break;
    case 521:
      ctx->key = EC_KEY_new_by_curve_name (NID_secp521r1);
      ctx->digest_length = SHA512_DIGEST_SIZE;
      ctx->digest = hash_string (&nettle_sha512, 3, "abc");
      break;
    default:
      die ("Internal error.\n");
    }

  /* This curve isn't supported in this build of openssl */
  if (ctx->key == NULL)
    return NULL;

  if (!EC_KEY_generate_key( ctx->key))
    die ("Openssl EC_KEY_generate_key failed.\n");
  
  ctx->signature = ECDSA_do_sign (ctx->digest, ctx->digest_length, ctx->key);
  
  return ctx;
}

static void
bench_openssl_ecdsa_sign (void *p)
{
  const struct openssl_ecdsa_ctx *ctx = p;
  ECDSA_SIG *sig = ECDSA_do_sign (ctx->digest, ctx->digest_length, ctx->key);
  ECDSA_SIG_free (sig);
}

static void
bench_openssl_ecdsa_verify (void *p)
{
  const struct openssl_ecdsa_ctx *ctx = p;
  if (ECDSA_do_verify (ctx->digest, ctx->digest_length,
			 ctx->signature, ctx->key) != 1)
    die ("Openssl ECDSA_do_verify failed.\n");      
}
static void
bench_openssl_ecdsa_clear (void *p)
{
  struct openssl_ecdsa_ctx *ctx = p;
  ECDSA_SIG_free (ctx->signature);
  EC_KEY_free (ctx->key);
  free (ctx->digest);
  free (ctx);
}
#endif

struct curve25519_ctx
{
  char x[CURVE25519_SIZE];
  char s[CURVE25519_SIZE];
};

static void
bench_curve25519_mul_g (void *p)
{
  struct curve25519_ctx *ctx = p;
  char q[CURVE25519_SIZE];
  curve25519_mul_g (q, ctx->s);
}

static void
bench_curve25519_mul (void *p)
{
  struct curve25519_ctx *ctx = p;
  char q[CURVE25519_SIZE];
  curve25519_mul (q, ctx->s, ctx->x);
}

static void
bench_curve25519 (void)
{
  double mul_g;
  double mul;
  struct knuth_lfib_ctx lfib;
  struct curve25519_ctx ctx;
  knuth_lfib_init (&lfib, 2);

  knuth_lfib_random (&lfib, sizeof(ctx.s), ctx.s);
  curve25519_mul_g (ctx.x, ctx.s);

  mul_g = time_function (bench_curve25519_mul_g, &ctx);
  mul = time_function (bench_curve25519_mul, &ctx);

  printf("%15s %4d %9.4f %9.4f\n",
	 "curve25519", 255, 1e-3/mul_g, 1e-3/mul);
}

struct alg alg_list[] = {
  { "rsa",   1024, bench_rsa_init,   bench_rsa_sign,   bench_rsa_verify,   bench_rsa_clear },
  { "rsa",   2048, bench_rsa_init,   bench_rsa_sign,   bench_rsa_verify,   bench_rsa_clear },
#if WITH_OPENSSL
  { "rsa (openssl)",  1024, bench_openssl_rsa_init, bench_openssl_rsa_sign, bench_openssl_rsa_verify, bench_openssl_rsa_clear },
  { "rsa (openssl)",  2048, bench_openssl_rsa_init, bench_openssl_rsa_sign, bench_openssl_rsa_verify, bench_openssl_rsa_clear },
#endif
  { "dsa",   1024, bench_dsa_init,   bench_dsa_sign,   bench_dsa_verify,   bench_dsa_clear },
#if 0
  { "dsa",2048, bench_dsa_init, bench_dsa_sign,   bench_dsa_verify, bench_dsa_clear },
#endif
  { "ecdsa",  192, bench_ecdsa_init, bench_ecdsa_sign, bench_ecdsa_verify, bench_ecdsa_clear },
  { "ecdsa",  224, bench_ecdsa_init, bench_ecdsa_sign, bench_ecdsa_verify, bench_ecdsa_clear },
  { "ecdsa",  256, bench_ecdsa_init, bench_ecdsa_sign, bench_ecdsa_verify, bench_ecdsa_clear },
  { "ecdsa",  384, bench_ecdsa_init, bench_ecdsa_sign, bench_ecdsa_verify, bench_ecdsa_clear },
  { "ecdsa",  521, bench_ecdsa_init, bench_ecdsa_sign, bench_ecdsa_verify, bench_ecdsa_clear },
#if WITH_OPENSSL
  { "ecdsa (openssl)",  192, bench_openssl_ecdsa_init, bench_openssl_ecdsa_sign, bench_openssl_ecdsa_verify, bench_openssl_ecdsa_clear },
  { "ecdsa (openssl)",  224, bench_openssl_ecdsa_init, bench_openssl_ecdsa_sign, bench_openssl_ecdsa_verify, bench_openssl_ecdsa_clear },
  { "ecdsa (openssl)",  256, bench_openssl_ecdsa_init, bench_openssl_ecdsa_sign, bench_openssl_ecdsa_verify, bench_openssl_ecdsa_clear },
  { "ecdsa (openssl)",  384, bench_openssl_ecdsa_init, bench_openssl_ecdsa_sign, bench_openssl_ecdsa_verify, bench_openssl_ecdsa_clear },
  { "ecdsa (openssl)",  521, bench_openssl_ecdsa_init, bench_openssl_ecdsa_sign, bench_openssl_ecdsa_verify, bench_openssl_ecdsa_clear },
#endif
};

#define numberof(x)  (sizeof (x) / sizeof ((x)[0]))

int
main (int argc, char **argv)
{
  const char *filter = NULL;
  unsigned i;

  if (argc > 1)
    filter = argv[1];

  time_init();
  printf ("%15s %4s %9s %9s\n",
	  "name", "size", "sign/ms", "verify/ms");

  for (i = 0; i < numberof(alg_list); i++)
    if (!filter || strstr (alg_list[i].name, filter))
      bench_alg (&alg_list[i]);

  if (!filter || strstr("curve25519", filter))
    bench_curve25519();

  return EXIT_SUCCESS;
}
