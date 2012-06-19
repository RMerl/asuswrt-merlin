#include <tommath.h>
#ifdef BN_MP_INIT_COPY_C
/* LibTomMath, multiple-precision integer library -- Tom St Denis
 *
 * LibTomMath is a library that provides multiple-precision
 * integer arithmetic as well as number theoretic functionality.
 *
 * The library was designed directly after the MPI library by
 * Michael Fromberger but has been written from scratch with
 * additional optimizations in place.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://math.libtomcrypt.com
 */

/* creates "a" then copies b into it */
int mp_init_copy (mp_int * a, mp_int * b)
{
  int     res;

  if ((res = mp_init_size (a, b->used)) != MP_OKAY) {
    return res;
  }
  return mp_copy (b, a);
}
#endif

/* $Source: /cvs/libtom/libtommath/bn_mp_init_copy.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2006/03/31 14:18:44 $ */
