/* Prototype declarations for math functions; helper file for <math.h>.
   Copyright (C) 1996-2002, 2003, 2006 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* NOTE: Because of the special way this file is used by <math.h>, this
   file must NOT be protected from multiple inclusion as header files
   usually are.

   This file provides prototype declarations for the math functions.
   Most functions are declared using the macro:

   __MATHCALL (NAME,[_r], (ARGS...))

   This means there is a function `NAME' returning `double' and a function
   `NAMEf' returning `float'.  Each place `_Mdouble_' appears in the
   prototype, that is actually `double' in the prototype for `NAME' and
   `float' in the prototype for `NAMEf'.  Reentrant variant functions are
   called `NAME_r' and `NAMEf_r'.

   Functions returning other types like `int' are declared using the macro:

   __MATHDECL (TYPE, NAME,[_r], (ARGS...))

   This is just like __MATHCALL but for a function returning `TYPE'
   instead of `_Mdouble_'.  In all of these cases, there is still
   both a `NAME' and a `NAMEf' that takes `float' arguments.

   Note that there must be no whitespace before the argument passed for
   NAME, to make token pasting work with -traditional.  */

#ifndef _MATH_H
# error "Never include <bits/mathcalls.h> directly; include <math.h> instead."
#endif


/* __MATHCALLX(type,function,[suffix],args,attrib) and
 * __MATHCALLI(type,function,[suffix],args) include libm_hidden_def
 * (for "double" versions only, xxxf and xxxl do not get this treatment).
 *
 * __MATHDECL(type,function,[suffix],args) does not.
 * __MATHCALL(function,[suffix],args) also does not
 * (it is just a shortcut to __MATHDECL(_Mdouble_,function,[suffix],args)).
 *
 * __MATHDECL_PRIV(type,function,[suffix],args,attrib)
 * includes libm_hidden_def (always) and declares __foo, not foo.
 */


/* Trigonometric functions.  */

_Mdouble_BEGIN_NAMESPACE
/* Arc cosine of X.  */
__MATHCALLI (acos,, (_Mdouble_ __x))
/* Arc sine of X.  */
__MATHCALLI (asin,, (_Mdouble_ __x))
/* Arc tangent of X.  */
__MATHCALLI (atan,, (_Mdouble_ __x))
/* Arc tangent of Y/X.  */
__MATHCALLI (atan2,, (_Mdouble_ __y, _Mdouble_ __x))

/* Cosine of X.  */
__MATHCALLI (cos,, (_Mdouble_ __x))
/* Sine of X.  */
__MATHCALLI (sin,, (_Mdouble_ __x))
/* Tangent of X.  */
__MATHCALLI (tan,, (_Mdouble_ __x))

/* Hyperbolic functions.  */

/* Hyperbolic cosine of X.  */
__MATHCALLI (cosh,, (_Mdouble_ __x))
/* Hyperbolic sine of X.  */
__MATHCALLI (sinh,, (_Mdouble_ __x))
/* Hyperbolic tangent of X.  */
__MATHCALLI (tanh,, (_Mdouble_ __x))
_Mdouble_END_NAMESPACE

#if 0 /*def __USE_GNU*/
/* Cosine and sine of X.  */
__MATHDECL (void,sincos,,
	    (_Mdouble_ __x, _Mdouble_ *__sinx, _Mdouble_ *__cosx))
#endif

#if defined __USE_MISC || defined __USE_XOPEN_EXTENDED || defined __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Hyperbolic arc cosine of X.  */
__MATHCALLI (acosh,, (_Mdouble_ __x))
/* Hyperbolic arc sine of X.  */
__MATHCALLI (asinh,, (_Mdouble_ __x))
/* Hyperbolic arc tangent of X.  */
__MATHCALLI (atanh,, (_Mdouble_ __x))
__END_NAMESPACE_C99
#endif

/* Exponential and logarithmic functions.  */

_Mdouble_BEGIN_NAMESPACE
/* Exponential function of X.  */
__MATHCALLI (exp,, (_Mdouble_ __x))

/* Break VALUE into a normalized fraction and an integral power of 2.  */
__MATHCALLI (frexp,, (_Mdouble_ __x, int *__exponent))

/* X times (two to the EXP power).  */
__MATHCALLI (ldexp,, (_Mdouble_ __x, int __exponent))

/* Natural logarithm of X.  */
__MATHCALLI (log,, (_Mdouble_ __x))

/* Base-ten logarithm of X.  */
__MATHCALLI (log10,, (_Mdouble_ __x))

/* Break VALUE into integral and fractional parts.  */
__MATHCALLI (modf,, (_Mdouble_ __x, _Mdouble_ *__iptr))
_Mdouble_END_NAMESPACE

#if 0 /*def __USE_GNU*/
/* A function missing in all standards: compute exponent to base ten.  */
__MATHCALL (exp10,, (_Mdouble_ __x))
/* Another name occasionally used.  */
__MATHCALL (pow10,, (_Mdouble_ __x))
#endif

#if defined __USE_MISC || defined __USE_XOPEN_EXTENDED || defined __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Return exp(X) - 1.  */
__MATHCALLI (expm1,, (_Mdouble_ __x))

/* Return log(1 + X).  */
__MATHCALLI (log1p,, (_Mdouble_ __x))

/* Return the base 2 signed integral exponent of X.  */
__MATHCALLI (logb,, (_Mdouble_ __x))
__END_NAMESPACE_C99
#endif

#ifdef __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Compute base-2 exponential of X.  */
__MATHCALLI (exp2,, (_Mdouble_ __x))

/* Compute base-2 logarithm of X.  */
__MATHCALL (log2,, (_Mdouble_ __x))
__END_NAMESPACE_C99
#endif


/* Power functions.  */

_Mdouble_BEGIN_NAMESPACE
/* Return X to the Y power.  */
__MATHCALLI (pow,, (_Mdouble_ __x, _Mdouble_ __y))

/* Return the square root of X.  */
__MATHCALLI (sqrt,, (_Mdouble_ __x))
_Mdouble_END_NAMESPACE

#if defined __USE_MISC || defined __USE_XOPEN || defined __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Return `sqrt(X*X + Y*Y)'.  */
__MATHCALLI (hypot,, (_Mdouble_ __x, _Mdouble_ __y))
__END_NAMESPACE_C99
#endif

#if defined __USE_MISC || defined __USE_XOPEN_EXTENDED || defined __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Return the cube root of X.  */
__MATHCALLI (cbrt,, (_Mdouble_ __x))
__END_NAMESPACE_C99
#endif


/* Nearest integer, absolute value, and remainder functions.  */

_Mdouble_BEGIN_NAMESPACE
/* Smallest integral value not less than X.  */
__MATHCALLX (ceil,, (_Mdouble_ __x), (__const__))

/* Absolute value of X.  */
__MATHCALLX (fabs,, (_Mdouble_ __x), (__const__))

/* Largest integer not greater than X.  */
__MATHCALLX (floor,, (_Mdouble_ __x), (__const__))

/* Floating-point modulo remainder of X/Y.  */
__MATHCALLI (fmod,, (_Mdouble_ __x, _Mdouble_ __y))


/* Return 0 if VALUE is finite or NaN, +1 if it
   is +Infinity, -1 if it is -Infinity.  */
__MATHDECL_PRIV (int,isinf,, (_Mdouble_ __value), (__const__))

/* Return nonzero if VALUE is finite and not NaN.  */
__MATHDECL_PRIV (int,finite,, (_Mdouble_ __value), (__const__))
_Mdouble_END_NAMESPACE

#ifdef __USE_MISC
#if 0
/* Return 0 if VALUE is finite or NaN, +1 if it
   is +Infinity, -1 if it is -Infinity.  */
__MATHDECL_PRIV (int,isinf,, (_Mdouble_ __value), (__const__))

/* Return nonzero if VALUE is finite and not NaN.  */
__MATHDECL_PRIV (int,finite,, (_Mdouble_ __value), (__const__))
#endif
/* Return the remainder of X/Y.  */
__MATHCALL (drem,, (_Mdouble_ __x, _Mdouble_ __y))


/* Return the fractional part of X after dividing out `ilogb (X)'.  */
__MATHCALL (significand,, (_Mdouble_ __x))
#endif /* Use misc.  */

#if defined __USE_MISC || defined __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Return X with its signed changed to Y's.  */
__MATHCALLX (copysign,, (_Mdouble_ __x, _Mdouble_ __y), (__const__))
__END_NAMESPACE_C99
#endif

#ifdef __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Return representation of NaN for double type.  */
__MATHCALLX (nan,, (__const char *__tagb), (__const__))
__END_NAMESPACE_C99
#endif

#if defined __USE_MISC || defined __USE_XOPEN || defined __USE_ISOC99
/* Return nonzero if VALUE is not a number.  */
__BEGIN_NAMESPACE_C99
__MATHDECL_PRIV (int,isnan,, (_Mdouble_ __value), (__const__))
__END_NAMESPACE_C99
#endif

#if defined __USE_MISC || defined __USE_XOPEN
# ifdef __DO_XSI_MATH__
/* Bessel functions.  */
__MATHCALL (j0,, (_Mdouble_))
__MATHCALL (j1,, (_Mdouble_))
__MATHCALL (jn,, (int, _Mdouble_))
__MATHCALL (y0,, (_Mdouble_))
__MATHCALL (y1,, (_Mdouble_))
__MATHCALL (yn,, (int, _Mdouble_))
# endif
#endif


#if defined __USE_MISC || defined __USE_XOPEN || defined __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Error and gamma functions.  */
__MATHCALLI (erf,, (_Mdouble_))
__MATHCALLI (erfc,, (_Mdouble_))
__MATHCALLI (lgamma,, (_Mdouble_))
__END_NAMESPACE_C99
#endif

#ifdef __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* True gamma function.  */
__MATHCALLI (tgamma,, (_Mdouble_))
__END_NAMESPACE_C99
#endif

#if defined __USE_MISC || defined __USE_XOPEN
/* Obsolete alias for `lgamma'.  */
__MATHCALL (gamma,, (_Mdouble_))
#endif

#ifdef __USE_MISC
/* Reentrant version of lgamma.  This function uses the global variable
   `signgam'.  The reentrant version instead takes a pointer and stores
   the value through it.  */
__MATHCALL (lgamma,_r, (_Mdouble_, int *__signgamp))
#endif


#if defined __USE_MISC || defined __USE_XOPEN_EXTENDED || defined __USE_ISOC99
__BEGIN_NAMESPACE_C99
/* Return the integer nearest X in the direction of the
   prevailing rounding mode.  */
__MATHCALLI (rint,, (_Mdouble_ __x))

/* Return X + epsilon if X < Y, X - epsilon if X > Y.  */
__MATHCALLX (nextafter,, (_Mdouble_ __x, _Mdouble_ __y), (__const__))
# if defined __USE_ISOC99 && !defined __LDBL_COMPAT
__MATHCALLX (nexttoward,, (_Mdouble_ __x, long double __y), (__const__))
# endif

/* Return the remainder of integer divison X / Y with infinite precision.  */
__MATHCALLI (remainder,, (_Mdouble_ __x, _Mdouble_ __y))

# if defined __USE_MISC || defined __USE_ISOC99
/* Return X times (2 to the Nth power).  */
__MATHCALLI (scalbn,, (_Mdouble_ __x, int __n))
# endif

/* Return the binary exponent of X, which must be nonzero.  */
__MATHDECLI (int,ilogb,, (_Mdouble_ __x))
#endif

#ifdef __USE_ISOC99
/* Return X times (2 to the Nth power).  */
__MATHCALLI (scalbln,, (_Mdouble_ __x, long int __n))

/* Round X to integral value in floating-point format using current
   rounding direction, but do not raise inexact exception.  */
__MATHCALLI (nearbyint,, (_Mdouble_ __x))

/* Round X to nearest integral value, rounding halfway cases away from
   zero.  */
__MATHCALLX (round,, (_Mdouble_ __x), (__const__))

/* Round X to the integral value in floating-point format nearest but
   not larger in magnitude.  */
__MATHCALLX (trunc,, (_Mdouble_ __x), (__const__))

/* Compute remainder of X and Y and put in *QUO a value with sign of x/y
   and magnitude congruent `mod 2^n' to the magnitude of the integral
   quotient x/y, with n >= 3.  */
__MATHCALLI (remquo,, (_Mdouble_ __x, _Mdouble_ __y, int *__quo))


/* Conversion functions.  */

/* Round X to nearest integral value according to current rounding
   direction.  */
__MATHDECLI (long int,lrint,, (_Mdouble_ __x))
__MATHDECLI (long long int,llrint,, (_Mdouble_ __x))

/* Round X to nearest integral value, rounding halfway cases away from
   zero.  */
__MATHDECLI (long int,lround,, (_Mdouble_ __x))
__MATHDECLI (long long int,llround,, (_Mdouble_ __x))


/* Return positive difference between X and Y.  */
__MATHCALLI (fdim,, (_Mdouble_ __x, _Mdouble_ __y))

/* Return maximum numeric value from X and Y.  */
__MATHCALLI (fmax,, (_Mdouble_ __x, _Mdouble_ __y))

/* Return minimum numeric value from X and Y.  */
__MATHCALLI (fmin,, (_Mdouble_ __x, _Mdouble_ __y))


/* Classify given number.  */
__MATHDECL_PRIV (int, fpclassify,, (_Mdouble_ __value), (__const__))

/* Test for negative number.  */
__MATHDECL_PRIV (int, signbit,, (_Mdouble_ __value), (__const__))


/* Multiply-add function computed as a ternary operation.  */
__MATHCALLI (fma,, (_Mdouble_ __x, _Mdouble_ __y, _Mdouble_ __z))
#endif /* Use ISO C99.  */

#if defined __USE_MISC || defined __USE_XOPEN_EXTENDED || defined __USE_ISOC99
__END_NAMESPACE_C99
#endif

#if (defined __USE_MISC || defined __USE_XOPEN_EXTENDED) \
	&& defined __UCLIBC_SUSV3_LEGACY__
/* Return X times (2 to the Nth power).  */
__MATHCALL (scalb,, (_Mdouble_ __x, _Mdouble_ __n))
#endif
