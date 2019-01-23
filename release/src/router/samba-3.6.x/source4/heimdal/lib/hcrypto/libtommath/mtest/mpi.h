/*
    mpi.h

    by Michael J. Fromberger <sting@linguist.dartmouth.edu>
    Copyright (C) 1998 Michael J. Fromberger, All Rights Reserved

    Arbitrary precision integer arithmetic library

    $Id: mpi.h,v 1.2 2005/05/05 14:38:47 tom Exp $
 */

#ifndef _H_MPI_
#define _H_MPI_

#include "mpi-config.h"

#define  MP_LT       -1
#define  MP_EQ        0
#define  MP_GT        1

#if MP_DEBUG
#undef MP_IOFUNC
#define MP_IOFUNC 1
#endif

#if MP_IOFUNC
#include <stdio.h>
#include <ctype.h>
#endif

#include <limits.h>

#define  MP_NEG  1
#define  MP_ZPOS 0

/* Included for compatibility... */
#define  NEG     MP_NEG
#define  ZPOS    MP_ZPOS

#define  MP_OKAY          0 /* no error, all is well */
#define  MP_YES           0 /* yes (boolean result)  */
#define  MP_NO           -1 /* no (boolean result)   */
#define  MP_MEM          -2 /* out of memory         */
#define  MP_RANGE        -3 /* argument out of range */
#define  MP_BADARG       -4 /* invalid parameter     */
#define  MP_UNDEF        -5 /* answer is undefined   */
#define  MP_LAST_CODE    MP_UNDEF

#include "mpi-types.h"

/* Included for compatibility... */
#define DIGIT_BIT         MP_DIGIT_BIT
#define DIGIT_MAX         MP_DIGIT_MAX

/* Macros for accessing the mp_int internals           */
#define  SIGN(MP)     ((MP)->sign)
#define  USED(MP)     ((MP)->used)
#define  ALLOC(MP)    ((MP)->alloc)
#define  DIGITS(MP)   ((MP)->dp)
#define  DIGIT(MP,N)  (MP)->dp[(N)]

#if MP_ARGCHK == 1
#define  ARGCHK(X,Y)  {if(!(X)){return (Y);}}
#elif MP_ARGCHK == 2
#include <assert.h>
#define  ARGCHK(X,Y)  assert(X)
#else
#define  ARGCHK(X,Y)  /*  */
#endif

/* This defines the maximum I/O base (minimum is 2)   */
#define MAX_RADIX         64

typedef struct {
  mp_sign       sign;    /* sign of this quantity      */
  mp_size       alloc;   /* how many digits allocated  */
  mp_size       used;    /* how many digits used       */
  mp_digit     *dp;      /* the digits themselves      */
} mp_int;

/*------------------------------------------------------------------------*/
/* Default precision                                                      */

unsigned int mp_get_prec(void);
void         mp_set_prec(unsigned int prec);

/*------------------------------------------------------------------------*/
/* Memory management                                                      */

mp_err mp_init(mp_int *mp);
mp_err mp_init_array(mp_int mp[], int count);
mp_err mp_init_size(mp_int *mp, mp_size prec);
mp_err mp_init_copy(mp_int *mp, mp_int *from);
mp_err mp_copy(mp_int *from, mp_int *to);
void   mp_exch(mp_int *mp1, mp_int *mp2);
void   mp_clear(mp_int *mp);
void   mp_clear_array(mp_int mp[], int count);
void   mp_zero(mp_int *mp);
void   mp_set(mp_int *mp, mp_digit d);
mp_err mp_set_int(mp_int *mp, long z);
mp_err mp_shrink(mp_int *a);


/*------------------------------------------------------------------------*/
/* Single digit arithmetic                                                */

mp_err mp_add_d(mp_int *a, mp_digit d, mp_int *b);
mp_err mp_sub_d(mp_int *a, mp_digit d, mp_int *b);
mp_err mp_mul_d(mp_int *a, mp_digit d, mp_int *b);
mp_err mp_mul_2(mp_int *a, mp_int *c);
mp_err mp_div_d(mp_int *a, mp_digit d, mp_int *q, mp_digit *r);
mp_err mp_div_2(mp_int *a, mp_int *c);
mp_err mp_expt_d(mp_int *a, mp_digit d, mp_int *c);

/*------------------------------------------------------------------------*/
/* Sign manipulations                                                     */

mp_err mp_abs(mp_int *a, mp_int *b);
mp_err mp_neg(mp_int *a, mp_int *b);

/*------------------------------------------------------------------------*/
/* Full arithmetic                                                        */

mp_err mp_add(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_sub(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_mul(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_mul_2d(mp_int *a, mp_digit d, mp_int *c);
#if MP_SQUARE
mp_err mp_sqr(mp_int *a, mp_int *b);
#else
#define mp_sqr(a, b) mp_mul(a, a, b)
#endif
mp_err mp_div(mp_int *a, mp_int *b, mp_int *q, mp_int *r);
mp_err mp_div_2d(mp_int *a, mp_digit d, mp_int *q, mp_int *r);
mp_err mp_expt(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_2expt(mp_int *a, mp_digit k);
mp_err mp_sqrt(mp_int *a, mp_int *b);

/*------------------------------------------------------------------------*/
/* Modular arithmetic                                                     */

#if MP_MODARITH
mp_err mp_mod(mp_int *a, mp_int *m, mp_int *c);
mp_err mp_mod_d(mp_int *a, mp_digit d, mp_digit *c);
mp_err mp_addmod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
mp_err mp_submod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
mp_err mp_mulmod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
#if MP_SQUARE
mp_err mp_sqrmod(mp_int *a, mp_int *m, mp_int *c);
#else
#define mp_sqrmod(a, m, c) mp_mulmod(a, a, m, c)
#endif
mp_err mp_exptmod(mp_int *a, mp_int *b, mp_int *m, mp_int *c);
mp_err mp_exptmod_d(mp_int *a, mp_digit d, mp_int *m, mp_int *c);
#endif /* MP_MODARITH */

/*------------------------------------------------------------------------*/
/* Comparisons                                                            */

int    mp_cmp_z(mp_int *a);
int    mp_cmp_d(mp_int *a, mp_digit d);
int    mp_cmp(mp_int *a, mp_int *b);
int    mp_cmp_mag(mp_int *a, mp_int *b);
int    mp_cmp_int(mp_int *a, long z);
int    mp_isodd(mp_int *a);
int    mp_iseven(mp_int *a);

/*------------------------------------------------------------------------*/
/* Number theoretic                                                       */

#if MP_NUMTH
mp_err mp_gcd(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_lcm(mp_int *a, mp_int *b, mp_int *c);
mp_err mp_xgcd(mp_int *a, mp_int *b, mp_int *g, mp_int *x, mp_int *y);
mp_err mp_invmod(mp_int *a, mp_int *m, mp_int *c);
#endif /* end MP_NUMTH */

/*------------------------------------------------------------------------*/
/* Input and output                                                       */

#if MP_IOFUNC
void   mp_print(mp_int *mp, FILE *ofp);
#endif /* end MP_IOFUNC */

/*------------------------------------------------------------------------*/
/* Base conversion                                                        */

#define BITS     1
#define BYTES    CHAR_BIT

mp_err mp_read_signed_bin(mp_int *mp, unsigned char *str, int len);
int    mp_signed_bin_size(mp_int *mp);
mp_err mp_to_signed_bin(mp_int *mp, unsigned char *str);

mp_err mp_read_unsigned_bin(mp_int *mp, unsigned char *str, int len);
int    mp_unsigned_bin_size(mp_int *mp);
mp_err mp_to_unsigned_bin(mp_int *mp, unsigned char *str);

int    mp_count_bits(mp_int *mp);

#if MP_COMPAT_MACROS
#define mp_read_raw(mp, str, len) mp_read_signed_bin((mp), (str), (len))
#define mp_raw_size(mp)           mp_signed_bin_size(mp)
#define mp_toraw(mp, str)         mp_to_signed_bin((mp), (str))
#define mp_read_mag(mp, str, len) mp_read_unsigned_bin((mp), (str), (len))
#define mp_mag_size(mp)           mp_unsigned_bin_size(mp)
#define mp_tomag(mp, str)         mp_to_unsigned_bin((mp), (str))
#endif

mp_err mp_read_radix(mp_int *mp, unsigned char *str, int radix);
int    mp_radix_size(mp_int *mp, int radix);
int    mp_value_radix_size(int num, int qty, int radix);
mp_err mp_toradix(mp_int *mp, unsigned char *str, int radix);

int    mp_char2value(char ch, int r);

#define mp_tobinary(M, S)  mp_toradix((M), (S), 2)
#define mp_tooctal(M, S)   mp_toradix((M), (S), 8)
#define mp_todecimal(M, S) mp_toradix((M), (S), 10)
#define mp_tohex(M, S)     mp_toradix((M), (S), 16)

/*------------------------------------------------------------------------*/
/* Error strings                                                          */

const  char  *mp_strerror(mp_err ec);

#endif /* end _H_MPI_ */

/* $Source: /cvs/libtom/libtommath/mtest/mpi.h,v $ */
/* $Revision: 1.2 $ */
/* $Date: 2005/05/05 14:38:47 $ */
