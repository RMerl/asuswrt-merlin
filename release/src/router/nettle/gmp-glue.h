/* gmp-glue.h

   Copyright (C) 2013 Niels Möller
   Copyright (C) 2013 Red Hat

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

#ifndef NETTLE_GMP_GLUE_H_INCLUDED
#define NETTLE_GMP_GLUE_H_INCLUDED

#include "bignum.h"

#ifdef mpz_limbs_read
#define GMP_HAVE_mpz_limbs_read 1
#else
#define GMP_HAVE_mpz_limbs_read 0
#endif

#ifdef mpn_copyd
#define GMP_HAVE_mpn_copyd 1
#else
#define GMP_HAVE_mpn_copyd 0
#endif

/* Name mangling. */
#if !GMP_HAVE_mpz_limbs_read
#define mpz_limbs_read _nettle_mpz_limbs_read
#define mpz_limbs_write _nettle_mpz_limbs_write
#define mpz_limbs_modify _nettle_mpz_limbs_modify
#define mpz_limbs_finish _nettle_mpz_limbs_finish
#define mpz_roinit_n _nettle_mpz_roinit_n
#endif

#if !GMP_HAVE_mpn_copyd
#define mpn_copyd _nettle_mpn_copyd
#define mpn_copyi _nettle_mpn_copyi
#define mpn_zero  _nettle_mpn_zero
#endif

#ifndef mpn_sqr
#define mpn_sqr(rp, ap, n) mpn_mul_n((rp), (ap), (ap), (n))
#endif

#define cnd_swap _nettle_cnd_swap
#define mpz_limbs_cmp _nettle_mpz_limbs_cmp
#define mpz_limbs_read_n _nettle_mpz_limbs_read_n
#define mpz_limbs_copy _nettle_mpz_limbs_copy
#define mpz_set_n _nettle_mpz_set_n
#define mpn_set_base256 _nettle_mpn_set_base256
#define mpn_set_base256_le _nettle_mpn_set_base256_le
#define mpn_get_base256_le _nettle_mpn_get_base256_le
#define gmp_alloc_limbs _nettle_gmp_alloc_limbs
#define gmp_free_limbs _nettle_gmp_free_limbs
#define gmp_free _nettle_gmp_free
#define gmp_alloc _nettle_gmp_alloc

#define TMP_GMP_DECL(name, type) type *name;	\
  size_t tmp_##name##_size
#define TMP_GMP_ALLOC(name, size) do {					\
    tmp_##name##_size = (size);						\
    (name) = gmp_alloc(sizeof (*name) * (size));	\
  } while (0)
#define TMP_GMP_FREE(name) (gmp_free(name, tmp_##name##_size))


/* Use only in-place operations, so we can fall back to addmul_1/submul_1 */
#ifdef mpn_cnd_add_n
# define cnd_add_n(cnd, rp, ap, n) mpn_cnd_add_n ((cnd), (rp), (rp), (ap), (n))
# define cnd_sub_n(cnd, rp, ap, n) mpn_cnd_sub_n ((cnd), (rp), (rp), (ap), (n))
#else
# define cnd_add_n(cnd, rp, ap, n) mpn_addmul_1 ((rp), (ap), (n), (cnd) != 0)
# define cnd_sub_n(cnd, rp, ap, n) mpn_submul_1 ((rp), (ap), (n), (cnd) != 0)
#endif

/* Some functions for interfacing between mpz and mpn code. Signs of
   the mpz numbers are generally ignored. */

#if !GMP_HAVE_mpz_limbs_read
/* Read access to mpz numbers. */

/* Return limb pointer, for read-only operations. Use mpz_size to get
   the number of limbs. */
const mp_limb_t *
mpz_limbs_read (const mpz_srcptr x);

/* Write access to mpz numbers. */

/* Get a limb pointer for writing, previous contents may be
   destroyed. */
mp_limb_t *
mpz_limbs_write (mpz_ptr x, mp_size_t n);

/* Get a limb pointer for writing, previous contents is intact. */
mp_limb_t *
mpz_limbs_modify (mpz_ptr x, mp_size_t n);

/* Update size. */
void
mpz_limbs_finish (mpz_ptr x, mp_size_t n);

/* Using an mpn number as an mpz. Can be used for read-only access
   only. x must not be cleared or reallocated. */
mpz_srcptr
mpz_roinit_n (mpz_ptr x, const mp_limb_t *xp, mp_size_t xs);

#endif /* !GMP_HAVE_mpz_limbs_read */

#if !GMP_HAVE_mpn_copyd
/* Copy elements, backwards */
void
mpn_copyd (mp_ptr dst, mp_srcptr src, mp_size_t n);

/* Copy elements, forwards */
void
mpn_copyi (mp_ptr dst, mp_srcptr src, mp_size_t n);

/* Zero elements */
void
mpn_zero (mp_ptr ptr, mp_size_t n);
#endif /* !GMP_HAVE_mpn_copyd */

void
cnd_swap (mp_limb_t cnd, mp_limb_t *ap, mp_limb_t *bp, mp_size_t n);

/* Convenience functions */
int
mpz_limbs_cmp (mpz_srcptr a, const mp_limb_t *bp, mp_size_t bn);

/* Get a pointer to an n limb area, for read-only operation. n must be
   greater or equal to the current size, and the mpz is zero-padded if
   needed. */
const mp_limb_t *
mpz_limbs_read_n (mpz_ptr x, mp_size_t n);

/* Copy limbs, with zero-padding. */
/* FIXME: Reorder arguments, on the theory that the first argument of
   an _mpz_* function should be an mpz_t? Or rename to _mpz_get_limbs,
   with argument order consistent with mpz_get_*. */
void
mpz_limbs_copy (mp_limb_t *xp, mpz_srcptr x, mp_size_t n);

void
mpz_set_n (mpz_t r, const mp_limb_t *xp, mp_size_t xn);

/* Like mpn_set_str, but always writes rn limbs. If input is larger,
   higher bits are ignored. */
void
mpn_set_base256 (mp_limb_t *rp, mp_size_t rn,
		 const uint8_t *xp, size_t xn);

void
mpn_set_base256_le (mp_limb_t *rp, mp_size_t rn,
		    const uint8_t *xp, size_t xn);

void
mpn_get_base256_le (uint8_t *rp, size_t rn,
		    const mp_limb_t *xp, mp_size_t xn);


mp_limb_t *
gmp_alloc_limbs (mp_size_t n);

void
gmp_free_limbs (mp_limb_t *p, mp_size_t n);

void *gmp_alloc(size_t n);
void gmp_free(void *p, size_t n);

#endif /* NETTLE_GMP_GLUE_H_INCLUDED */
