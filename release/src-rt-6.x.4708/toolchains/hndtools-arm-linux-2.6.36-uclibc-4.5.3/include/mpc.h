/* mpc.h -- Include file for mpc.

Copyright (C) INRIA, 2002, 2003, 2004, 2005, 2007, 2008, 2009, 2010, 2011

This file is part of the MPC Library.

The MPC Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

The MPC Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the MPC Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

#ifndef __MPC_H
#define __MPC_H

#include "gmp.h"
#include "mpfr.h"

/* Backwards compatibility with mpfr<3.0.0 */
#ifndef mpfr_exp_t
#define mpfr_exp_t mp_exp_t
#endif

/* Define MPC version number */
#define MPC_VERSION_MAJOR 0
#define MPC_VERSION_MINOR 9
#define MPC_VERSION_PATCHLEVEL 0
#define MPC_VERSION_STRING "0.9"

/* Macros dealing with MPC VERSION */
#define MPC_VERSION_NUM(a,b,c) (((a) << 16L) | ((b) << 8) | (c))
#define MPC_VERSION                                                     \
  MPC_VERSION_NUM(MPC_VERSION_MAJOR,MPC_VERSION_MINOR,MPC_VERSION_PATCHLEVEL)

/* Check if stdio.h is included */
#if defined (_GMP_H_HAVE_FILE)
# define _MPC_H_HAVE_FILE 1
#endif

/* Check if stdint.h/inttypes.h is included */
#if defined (INTMAX_C) && defined (UINTMAX_C)
# define _MPC_H_HAVE_INTMAX_T 1
#endif

/* Check if complex.h is included */
#if defined (_COMPLEX_H)
# define _MPC_H_HAVE_COMPLEX 1
#endif

/* Return values */

/* Transform negative to 2, positive to 1, leave 0 unchanged */
#define MPC_INEX_POS(inex) (((inex) < 0) ? 2 : ((inex) == 0) ? 0 : 1)
/* Transform 2 to negative, 1 to positive, leave 0 unchanged */
#define MPC_INEX_NEG(inex) (((inex) == 2) ? -1 : ((inex) == 0) ? 0 : 1)

/* The global inexact flag is made of (real flag) + 4 * (imaginary flag), where
   each of the real and imaginary inexact flag are:
   0 when the result is exact (no rounding error)
   1 when the result is larger than the exact value
   2 when the result is smaller than the exact value */
#define MPC_INEX(inex_re, inex_im) \
        (MPC_INEX_POS(inex_re) | (MPC_INEX_POS(inex_im) << 2))
#define MPC_INEX_RE(inex) MPC_INEX_NEG((inex) & 3)
#define MPC_INEX_IM(inex) MPC_INEX_NEG((inex) >> 2)

/* For functions computing two results, the return value is
   inexact1+16*inexact2, which is 0 iif both results are exact. */
#define MPC_INEX12(inex1, inex2) (inex1 | (inex2 << 4))
#define MPC_INEX1(inex) (inex & 15)
#define MPC_INEX2(inex) (inex >> 4)

/* Definition of rounding modes */

/* a complex rounding mode is just a pair of two real rounding modes
   we reserve four bits for a real rounding mode.  */
typedef int mpc_rnd_t;

#define RNDC(r1,r2) (((int)(r1)) + ((int)(r2) << 4))
#define MPC_RND_RE(x) ((mpfr_rnd_t)((x) & 0x0F))
#define MPC_RND_IM(x) ((mpfr_rnd_t)((x) >> 4))

#define MPC_RNDNN RNDC(GMP_RNDN,GMP_RNDN)
#define MPC_RNDNZ RNDC(GMP_RNDN,GMP_RNDZ)
#define MPC_RNDNU RNDC(GMP_RNDN,GMP_RNDU)
#define MPC_RNDND RNDC(GMP_RNDN,GMP_RNDD)

#define MPC_RNDZN RNDC(GMP_RNDZ,GMP_RNDN)
#define MPC_RNDZZ RNDC(GMP_RNDZ,GMP_RNDZ)
#define MPC_RNDZU RNDC(GMP_RNDZ,GMP_RNDU)
#define MPC_RNDZD RNDC(GMP_RNDZ,GMP_RNDD)

#define MPC_RNDUN RNDC(GMP_RNDU,GMP_RNDN)
#define MPC_RNDUZ RNDC(GMP_RNDU,GMP_RNDZ)
#define MPC_RNDUU RNDC(GMP_RNDU,GMP_RNDU)
#define MPC_RNDUD RNDC(GMP_RNDU,GMP_RNDD)

#define MPC_RNDDN RNDC(GMP_RNDD,GMP_RNDN)
#define MPC_RNDDZ RNDC(GMP_RNDD,GMP_RNDZ)
#define MPC_RNDDU RNDC(GMP_RNDD,GMP_RNDU)
#define MPC_RNDDD RNDC(GMP_RNDD,GMP_RNDD)


/* Definitions of types and their semantics */

typedef struct {
  mpfr_t re;
  mpfr_t im;
}
__mpc_struct;

typedef __mpc_struct mpc_t[1];
typedef __mpc_struct *mpc_ptr;
typedef __gmp_const __mpc_struct *mpc_srcptr;

/* Prototypes: Support of K&R compiler */
#if defined (__GMP_PROTO)
# define __MPC_PROTO __GMP_PROTO
#elif defined (__STDC__) || defined (__cplusplus)
# define __MPC_PROTO(x) x
#else
# define __MPC_PROTO(x) ()
#endif

/* Support for WINDOWS Dll:
   Check if we are inside a MPC build, and if so export the functions.
   Otherwise does the same thing as GMP */
#if defined(__MPC_WITHIN_MPC) && __GMP_LIBGMP_DLL
# define __MPC_DECLSPEC __GMP_DECLSPEC_EXPORT
#else
# define __MPC_DECLSPEC __GMP_DECLSPEC
#endif

#if defined (__cplusplus)
extern "C" {
#endif

__MPC_DECLSPEC int  mpc_add    __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_add_fr __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpfr_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_add_si __MPC_PROTO ((mpc_ptr, mpc_srcptr, long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_add_ui __MPC_PROTO ((mpc_ptr, mpc_srcptr, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_sub    __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_sub_fr __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpfr_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_fr_sub __MPC_PROTO ((mpc_ptr, mpfr_srcptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_sub_ui __MPC_PROTO ((mpc_ptr, mpc_srcptr, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_ui_ui_sub __MPC_PROTO ((mpc_ptr, unsigned long int, unsigned long int, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_mul    __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_mul_fr __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpfr_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_mul_ui __MPC_PROTO ((mpc_ptr, mpc_srcptr, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_mul_si __MPC_PROTO ((mpc_ptr, mpc_srcptr, long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_mul_i  __MPC_PROTO ((mpc_ptr, mpc_srcptr, int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_sqr    __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_div    __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_pow    __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_pow_fr __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpfr_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_pow_ld __MPC_PROTO ((mpc_ptr, mpc_srcptr, long double, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_pow_d __MPC_PROTO ((mpc_ptr, mpc_srcptr, double, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_pow_si __MPC_PROTO ((mpc_ptr, mpc_srcptr, long, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_pow_ui __MPC_PROTO ((mpc_ptr, mpc_srcptr, unsigned long, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_pow_z  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpz_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_div_fr __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpfr_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_fr_div __MPC_PROTO ((mpc_ptr, mpfr_srcptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_div_ui __MPC_PROTO ((mpc_ptr, mpc_srcptr, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_ui_div __MPC_PROTO ((mpc_ptr, unsigned long int, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_div_2exp __MPC_PROTO ((mpc_ptr, mpc_srcptr, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_mul_2exp __MPC_PROTO ((mpc_ptr, mpc_srcptr, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_conj  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_neg   __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_norm  __MPC_PROTO ((mpfr_ptr, mpc_srcptr, mpfr_rnd_t));
__MPC_DECLSPEC int  mpc_abs   __MPC_PROTO ((mpfr_ptr, mpc_srcptr, mpfr_rnd_t));
__MPC_DECLSPEC int  mpc_sqrt  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set       __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_d     __MPC_PROTO ((mpc_ptr, double, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_d_d   __MPC_PROTO ((mpc_ptr, double, double, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_ld    __MPC_PROTO ((mpc_ptr, long double, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_ld_ld __MPC_PROTO ((mpc_ptr, long double, long double, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_f     __MPC_PROTO ((mpc_ptr, mpf_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_f_f   __MPC_PROTO ((mpc_ptr, mpf_srcptr, mpf_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_fr    __MPC_PROTO ((mpc_ptr, mpfr_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_fr_fr __MPC_PROTO ((mpc_ptr, mpfr_srcptr, mpfr_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_q     __MPC_PROTO ((mpc_ptr, mpq_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_q_q   __MPC_PROTO ((mpc_ptr, mpq_srcptr, mpq_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_si    __MPC_PROTO ((mpc_ptr, long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_si_si __MPC_PROTO ((mpc_ptr, long int, long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_ui    __MPC_PROTO ((mpc_ptr, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_ui_ui __MPC_PROTO ((mpc_ptr, unsigned long int, unsigned long int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_z     __MPC_PROTO ((mpc_ptr, mpz_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_z_z   __MPC_PROTO ((mpc_ptr, mpz_srcptr, mpz_srcptr, mpc_rnd_t));
__MPC_DECLSPEC void mpc_swap      __MPC_PROTO ((mpc_ptr, mpc_ptr));
__MPC_DECLSPEC int  mpc_fma       __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_srcptr, mpc_srcptr, mpc_rnd_t));

#ifdef _MPC_H_HAVE_INTMAX_T
__MPC_DECLSPEC int  mpc_set_sj __MPC_PROTO ((mpc_ptr, intmax_t, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_uj __MPC_PROTO ((mpc_ptr, uintmax_t,  mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_sj_sj __MPC_PROTO ((mpc_ptr, intmax_t, intmax_t, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_uj_uj __MPC_PROTO ((mpc_ptr, uintmax_t, uintmax_t, mpc_rnd_t));
#endif

#ifdef _MPC_H_HAVE_COMPLEX
__MPC_DECLSPEC int  mpc_set_dc __MPC_PROTO ((mpc_ptr, double _Complex, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_ldc __MPC_PROTO ((mpc_ptr, long double _Complex, mpc_rnd_t));
__MPC_DECLSPEC double _Complex  mpc_get_dc __MPC_PROTO ((mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC long double _Complex  mpc_get_ldc __MPC_PROTO ((mpc_srcptr, mpc_rnd_t));
#endif

__MPC_DECLSPEC void mpc_set_nan     __MPC_PROTO ((mpc_ptr));

__MPC_DECLSPEC int  mpc_real __MPC_PROTO ((mpfr_ptr, mpc_srcptr, mpfr_rnd_t));
__MPC_DECLSPEC int  mpc_imag __MPC_PROTO ((mpfr_ptr, mpc_srcptr, mpfr_rnd_t));
__MPC_DECLSPEC int  mpc_arg  __MPC_PROTO ((mpfr_ptr, mpc_srcptr, mpfr_rnd_t));
__MPC_DECLSPEC int  mpc_proj __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_cmp  __MPC_PROTO ((mpc_srcptr, mpc_srcptr));
__MPC_DECLSPEC int  mpc_cmp_si_si __MPC_PROTO ((mpc_srcptr, long int, long int));
__MPC_DECLSPEC int  mpc_exp  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_log  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_sin  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_cos  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_sin_cos  __MPC_PROTO ((mpc_ptr, mpc_ptr, mpc_srcptr, mpc_rnd_t, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_tan  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_sinh __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_cosh __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_tanh __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_asin  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_acos  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_atan  __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_asinh __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_acosh __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_atanh __MPC_PROTO ((mpc_ptr, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC void mpc_clear   __MPC_PROTO ((mpc_ptr));
__MPC_DECLSPEC int  mpc_urandom __MPC_PROTO ((mpc_ptr, gmp_randstate_t));
__MPC_DECLSPEC void mpc_init2 __MPC_PROTO ((mpc_ptr, mpfr_prec_t));
__MPC_DECLSPEC void mpc_init3 __MPC_PROTO ((mpc_ptr, mpfr_prec_t, mpfr_prec_t));
__MPC_DECLSPEC mpfr_prec_t mpc_get_prec __MPC_PROTO((mpc_srcptr x));
__MPC_DECLSPEC void mpc_get_prec2 __MPC_PROTO((mpfr_prec_t *pr, mpfr_prec_t *pi, mpc_srcptr x));
__MPC_DECLSPEC void mpc_set_prec  __MPC_PROTO ((mpc_ptr, mpfr_prec_t));
__MPC_DECLSPEC __gmp_const char * mpc_get_version __MPC_PROTO ((void));

__MPC_DECLSPEC int  mpc_strtoc    _MPFR_PROTO ((mpc_ptr, const char *, char **, int, mpc_rnd_t));
__MPC_DECLSPEC int  mpc_set_str   _MPFR_PROTO ((mpc_ptr, const char *, int, mpc_rnd_t));
__MPC_DECLSPEC char * mpc_get_str _MPFR_PROTO ((int, size_t, mpc_srcptr, mpc_rnd_t));
__MPC_DECLSPEC void mpc_free_str  _MPFR_PROTO ((char *));
#ifdef _MPC_H_HAVE_FILE
__MPC_DECLSPEC int mpc_inp_str __MPC_PROTO ((mpc_ptr, FILE *, size_t *, int, mpc_rnd_t));
__MPC_DECLSPEC size_t mpc_out_str __MPC_PROTO ((FILE *, int, size_t, mpc_srcptr, mpc_rnd_t));
#endif

#if defined (__cplusplus)
}
#endif

#define mpc_realref(x) ((x)->re)
#define mpc_imagref(x) ((x)->im)

#define mpc_cmp_si(x, y) \
 ( mpc_cmp_si_si ((x), (y), 0l) )
#define mpc_ui_sub(x, y, z, r) mpc_ui_ui_sub (x, y, 0ul, z, r)

/*
   Define a fake mpfr_set_fr so that, for instance, mpc_set_fr_z would
   be defined as follows:
   mpc_set_fr_z (mpc_t rop, mpfr_t x, mpz_t y, mpc_rnd_t rnd)
       MPC_SET_X_Y (fr, z, rop, x, y, rnd)
*/
#ifndef mpfr_set_fr
#define mpfr_set_fr mpfr_set
#endif
#define MPC_SET_X_Y(real_t, imag_t, z, real_value, imag_value, rnd)     \
  {                                                                     \
    int _inex_re, _inex_im;                                             \
    _inex_re = (mpfr_set_ ## real_t) (mpc_realref (z), (real_value), MPC_RND_RE (rnd)); \
    _inex_im = (mpfr_set_ ## imag_t) (mpc_imagref (z), (imag_value), MPC_RND_IM (rnd)); \
    return MPC_INEX (_inex_re, _inex_im);                               \
  }

#endif /* ifndef __MPC_H */
