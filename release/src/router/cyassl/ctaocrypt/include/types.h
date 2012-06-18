/* types.h
 *
 * Copyright (C) 2006-2009 Sawtooth Consulting Ltd.
 *
 * This file is part of CyaSSL.
 *
 * CyaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CyaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#ifndef CTAO_CRYPT_TYPES_H
#define CTAO_CRYPT_TYPES_H

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#ifdef XMALLOC_USER
    #include <stdlib.h>   /* for size_t */
#endif

#ifdef __cplusplus
    extern "C" {
#endif


#if defined(WORDS_BIGENDIAN) || (defined(__MWERKS__) && !defined(__INTEL__))
    #define BIG_ENDIAN_ORDER
#endif

#ifndef BIG_ENDIAN_ORDER
    #define LITTLE_ENDIAN_ORDER
#endif

#ifdef IPHONE
    #define SIZEOF_LONG_LONG 8
#endif

#ifdef THREADX
    #define SIZEOF_LONG_LONG 8
#endif


typedef unsigned char  byte;
typedef unsigned short word16;
typedef unsigned int   word32;

#if defined(_MSC_VER) || defined(__BCPLUSPLUS__)
    #define WORD64_AVAILABLE
    #define W64LIT(x) x##ui64
    typedef unsigned __int64 word64;
#elif SIZEOF_LONG == 8
    #define WORD64_AVAILABLE
    #define W64LIT(x) x##LL
    typedef unsigned long word64;
#elif SIZEOF_LONG_LONG == 8 
    #define WORD64_AVAILABLE
    #define W64LIT(x) x##LL
    typedef unsigned long long word64;
#else
    #define MP_16BIT   /* for mp_int, mp_word needs to be twice as big as
                          mp_digit, no 64 bit type so make mp_digit 16 bit */
#endif


/* These platforms have 64-bit CPU registers.  */
#if (defined(__alpha__) || defined(__ia64__) || defined(_ARCH_PPC64) || \
     defined(__mips64)  || defined(__x86_64__)) 
    typedef word64 word;
#else
    typedef word32 word;
    #ifdef WORD64_AVAILABLE
        #define CTAOCRYPT_SLOW_WORD64
    #endif
#endif


enum {
    WORD_SIZE  = sizeof(word),
    BIT_SIZE   = 8,
    WORD_BITS  = WORD_SIZE * BIT_SIZE
};


/* use inlining if compiler allows */
#ifndef INLINE
#ifndef NO_INLINE
    #ifdef _MSC_VER
        #define INLINE __inline
    #elif __GNUC__
        #define INLINE inline
    #elif THREADX
        #define INLINE _Inline
    #else
        #define INLINE 
    #endif
#else
    #define INLINE 
#endif
#endif


/* set up rotate style */
#if defined(_MSC_VER) || defined(__BCPLUSPLUS__)
	#define INTEL_INTRINSICS
	#define FAST_ROTATE
#elif defined(__MWERKS__) && TARGET_CPU_PPC
	#define PPC_INTRINSICS
	#define FAST_ROTATE
#elif defined(__GNUC__) && defined(__i386__)
        /* GCC does peephole optimizations which should result in using rotate
           instructions  */
	#define FAST_ROTATE
#endif


/* idea to add global alloc override by Moisés Guimarães  */
/* default to libc stuff */
/* XCALLOC not used by CyaSSL or either math lib */
/* XREALLOC is used once in mormal math lib, not in fast math lib */
/* XFREE on some embeded systems doesn't like free(0) so test  */
#ifndef XMALLOC_USER
    #define XMALLOC(s, h)     malloc(s)
    #define XFREE(p, h)       if (p) free(p)
    #define XREALLOC(p, n, h) realloc(p, n)
    #define XCALLOC(n, s, h)  calloc(n, s)
#else
    /* prototypes for our heap functions */
    extern void *XMALLOC(size_t n, void* heap);
    extern void *XREALLOC(void *p, size_t n, void* heap);
    extern void *XCALLOC(size_t n, size_t s, void* heap);
    extern void XFREE(void *p, void* heap);
#endif


#ifdef __cplusplus
    }   /* extern "C" */
#endif


#endif /* CTAO_CRYPT_TYPES_H */

