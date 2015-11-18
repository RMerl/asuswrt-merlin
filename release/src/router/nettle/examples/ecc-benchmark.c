/* ecc-benchmark.c

   Copyright (C) 2013 Niels Möller

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

/* Development of Nettle's ECC support was funded by the .SE Internet Fund. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <time.h>

#include "timing.h"

#include "../ecc.h"
#include "../ecc-internal.h"
#include "../gmp-glue.h"

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

static mp_limb_t *
xalloc_limbs (mp_size_t size)
{
  return xalloc (size * sizeof(mp_limb_t));
}

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

#if !NETTLE_USE_MINI_GMP
static int
modinv_gcd (const struct ecc_curve *ecc,
	    mp_limb_t *rp, mp_limb_t *ap, mp_limb_t *tp)
{
  mp_size_t size = ecc->p.size;
  mp_limb_t *up = tp;
  mp_limb_t *vp = tp + size+1;
  mp_limb_t *gp = tp + 2*(size+1);
  mp_limb_t *sp = tp + 3*(size+1);
  mp_size_t gn, sn;

  mpn_copyi (up, ap, size);
  mpn_copyi (vp, ecc->p.m, size);
  gn = mpn_gcdext (gp, sp, &sn, up, size, vp, size);
  if (gn != 1 || gp[0] != 1)
    return 0;
  
  if (sn < 0)
    mpn_sub (sp, ecc->p.m, size, sp, -sn);
  else if (sn < size)
    /* Zero-pad. */
    mpn_zero (sp + sn, size - sn);

  mpn_copyi (rp, sp, size);
  return 1;
}
#endif

struct ecc_ctx {
  const struct ecc_curve *ecc;
  mp_limb_t *rp;
  mp_limb_t *ap;
  mp_limb_t *bp;
  mp_limb_t *tp;
};

static void
bench_modp (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp, ctx->ap, 2*ctx->ecc->p.size);
  ctx->ecc->p.mod (&ctx->ecc->p, ctx->rp);
}

static void
bench_reduce (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp, ctx->ap, 2*ctx->ecc->p.size);
  ctx->ecc->p.reduce (&ctx->ecc->p, ctx->rp);
}

static void
bench_modq (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp, ctx->ap, 2*ctx->ecc->p.size);
  ctx->ecc->q.mod(&ctx->ecc->q, ctx->rp);
}

static void
bench_modinv (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ctx->ecc->p.invert (&ctx->ecc->p, ctx->rp, ctx->ap, ctx->tp);
}

#if !NETTLE_USE_MINI_GMP
static void
bench_modinv_gcd (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp + ctx->ecc->p.size, ctx->ap, ctx->ecc->p.size);
  modinv_gcd (ctx->ecc, ctx->rp, ctx->rp + ctx->ecc->p.size, ctx->tp);  
}
#endif

#ifdef mpn_sec_powm
static void
bench_modinv_powm (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  const struct ecc_curve *ecc = ctx->ecc;
  mp_size_t size = ecc->p.size;
  
  mpn_sub_1 (ctx->rp + size, ecc->p.m, size, 2);
  mpn_sec_powm (ctx->rp, ctx->ap, size,
		ctx->rp + size, ecc->p.bit_size,
		ecc->p.m, size, ctx->tp);
}
#endif

static void
bench_dup_jj (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_dup_jj (ctx->ecc, ctx->rp, ctx->ap, ctx->tp);
}

static void
bench_add_jja (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_add_jja (ctx->ecc, ctx->rp, ctx->ap, ctx->bp, ctx->tp);
}

static void
bench_add_hhh (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ctx->ecc->add_hhh (ctx->ecc, ctx->rp, ctx->ap, ctx->bp, ctx->tp);
}

static void
bench_mul_g (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ctx->ecc->mul_g (ctx->ecc, ctx->rp, ctx->ap, ctx->tp);
}

static void
bench_mul_a (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ctx->ecc->mul (ctx->ecc, ctx->rp, ctx->ap, ctx->bp, ctx->tp);
}

static void
bench_dup_eh (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_dup_eh (ctx->ecc, ctx->rp, ctx->ap, ctx->tp);
}

static void
bench_add_eh (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_add_eh (ctx->ecc, ctx->rp, ctx->ap, ctx->bp, ctx->tp);
}

#if NETTLE_USE_MINI_GMP
static void
mpn_random (mp_limb_t *xp, mp_size_t n)
{
  mp_size_t i;
  for (i = 0; i < n; i++)
    xp[i] = rand();
}
#endif

static void
bench_curve (const struct ecc_curve *ecc)
{
  struct ecc_ctx ctx;  
  double modp, reduce, modq, modinv, modinv_gcd, modinv_powm,
    dup_jj, add_jja, add_hhh,
    mul_g, mul_a;

  mp_limb_t mask;
  mp_size_t itch;

  ctx.ecc = ecc;
  ctx.rp = xalloc_limbs (3*ecc->p.size);
  ctx.ap = xalloc_limbs (3*ecc->p.size);
  ctx.bp = xalloc_limbs (3*ecc->p.size);
  itch = ecc->mul_itch;
#ifdef mpn_sec_powm
  {
    mp_size_t powm_itch
      = mpn_sec_powm_itch (ecc->p.size, ecc->p.bit_size, ecc->p.size);
    if (powm_itch > itch)
      itch = powm_itch;
  }
#endif
  ctx.tp = xalloc_limbs (itch);

  mpn_random (ctx.ap, 3*ecc->p.size);
  mpn_random (ctx.bp, 3*ecc->p.size);

  mask = (~(mp_limb_t) 0) >> (ecc->p.size * GMP_NUMB_BITS - ecc->p.bit_size);
  ctx.ap[ecc->p.size - 1] &= mask;
  ctx.ap[2*ecc->p.size - 1] &= mask;
  ctx.ap[3*ecc->p.size - 1] &= mask;
  ctx.bp[ecc->p.size - 1] &= mask;
  ctx.bp[2*ecc->p.size - 1] &= mask;
  ctx.bp[3*ecc->p.size - 1] &= mask;

  modp = time_function (bench_modp, &ctx);
  reduce = time_function (bench_reduce, &ctx);

  modq = time_function (bench_modq, &ctx);

  modinv = time_function (bench_modinv, &ctx);
#if !NETTLE_USE_MINI_GMP
  modinv_gcd = time_function (bench_modinv_gcd, &ctx);
#else
  modinv_gcd = 0;
#endif
#ifdef mpn_sec_powm
  modinv_powm = time_function (bench_modinv_powm, &ctx);
#else
  modinv_powm = 0;
#endif
  if (ecc->p.bit_size == 255)
    {
      /* For now, curve25519 is a special case */
      dup_jj = time_function (bench_dup_eh, &ctx);
      add_jja = time_function (bench_add_eh, &ctx);
    }
  else
    {
      dup_jj = time_function (bench_dup_jj, &ctx);
      add_jja = time_function (bench_add_jja, &ctx);
    }
  add_hhh = time_function (bench_add_hhh, &ctx);
  mul_g = time_function (bench_mul_g, &ctx);
  mul_a = time_function (bench_mul_a, &ctx);

  free (ctx.rp);
  free (ctx.ap);
  free (ctx.bp);
  free (ctx.tp);

  printf ("%4d %6.4f %6.4f %6.4f %6.2f %6.3f %6.2f %6.3f %6.3f %6.3f %6.1f %6.1f\n",
	  ecc->p.bit_size, 1e6 * modp, 1e6 * reduce, 1e6 * modq,
	  1e6 * modinv, 1e6 * modinv_gcd, 1e6 * modinv_powm,
	  1e6 * dup_jj, 1e6 * add_jja, 1e6 * add_hhh,
	  1e6 * mul_g, 1e6 * mul_a);
}

const struct ecc_curve * const curves[] = {
  &nettle_secp_192r1,
  &nettle_secp_224r1,
  &_nettle_curve25519,
  &nettle_secp_256r1,
  &nettle_secp_384r1,
  &nettle_secp_521r1,
};

#define numberof(x)  (sizeof (x) / sizeof ((x)[0]))

int
main (int argc UNUSED, char **argv UNUSED)
{
  unsigned i;

  time_init();
  printf ("%4s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s (us)\n",
	  "size", "modp", "reduce", "modq", "modinv", "mi_gcd", "mi_pow",
	  "dup_jj", "ad_jja", "ad_hhh",
	  "mul_g", "mul_a");
  for (i = 0; i < numberof (curves); i++)
    bench_curve (curves[i]);

  return EXIT_SUCCESS;
}
