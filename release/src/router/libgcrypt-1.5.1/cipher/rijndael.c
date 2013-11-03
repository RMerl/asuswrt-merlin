/* Rijndael (AES) for GnuPG
 * Copyright (C) 2000, 2001, 2002, 2003, 2007,
 *               2008, 2011 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *******************************************************************
 * The code here is based on the optimized implementation taken from
 * http://www.esat.kuleuven.ac.be/~rijmen/rijndael/ on Oct 2, 2000,
 * which carries this notice:
 *------------------------------------------
 * rijndael-alg-fst.c   v2.3   April '2000
 *
 * Optimised ANSI C code
 *
 * authors: v1.0: Antoon Bosselaers
 *          v2.0: Vincent Rijmen
 *          v2.3: Paulo Barreto
 *
 * This code is placed in the public domain.
 *------------------------------------------
 *
 * The SP800-38a document is available at:
 *   http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
 *
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcmp() */

#include "types.h"  /* for byte and u32 typedefs */
#include "g10lib.h"
#include "cipher.h"

#define MAXKC			(256/32)
#define MAXROUNDS		14
#define BLOCKSIZE               (128/8)


/* Helper macro to force alignment to 16 bytes.  */
#ifdef __GNUC__
# define ATTR_ALIGNED_16  __attribute__ ((aligned (16)))
#else
# define ATTR_ALIGNED_16
#endif


/* USE_PADLOCK indicates whether to compile the padlock specific
   code.  */
#undef USE_PADLOCK
#ifdef ENABLE_PADLOCK_SUPPORT
# if defined (__i386__) && SIZEOF_UNSIGNED_LONG == 4 && defined (__GNUC__)
#  define USE_PADLOCK 1
# endif
#endif /*ENABLE_PADLOCK_SUPPORT*/

/* USE_AESNI inidicates whether to compile with Intel AES-NI code.  We
   need the vector-size attribute which seems to be available since
   gcc 3.  However, to be on the safe side we require at least gcc 4.  */
#undef USE_AESNI
#ifdef ENABLE_AESNI_SUPPORT
# if defined (__i386__) && SIZEOF_UNSIGNED_LONG == 4 && __GNUC__ >= 4
#  define USE_AESNI 1
# endif
#endif /* ENABLE_AESNI_SUPPORT */

#ifdef USE_AESNI
  typedef int m128i_t __attribute__ ((__vector_size__ (16)));
#endif /*USE_AESNI*/

/* Define an u32 variant for the sake of gcc 4.4's strict aliasing.  */
#if __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ >= 4 )
typedef u32           __attribute__ ((__may_alias__)) u32_a_t;
#else
typedef u32           u32_a_t;
#endif



/* Our context object.  */
typedef struct
{
  /* The first fields are the keyschedule arrays.  This is so that
     they are aligned on a 16 byte boundary if using gcc.  This
     alignment is required for the AES-NI code and a good idea in any
     case.  The alignment is guaranteed due to the way cipher.c
     allocates the space for the context.  The PROPERLY_ALIGNED_TYPE
     hack is used to force a minimal alignment if not using gcc of if
     the alignment requirement is higher that 16 bytes.  */
  union
  {
    PROPERLY_ALIGNED_TYPE dummy;
    byte keyschedule[MAXROUNDS+1][4][4];
#ifdef USE_PADLOCK
    /* The key as passed to the padlock engine.  It is only used if
       the padlock engine is used (USE_PADLOCK, below).  */
    unsigned char padlock_key[16] __attribute__ ((aligned (16)));
#endif /*USE_PADLOCK*/
  } u1;
  union
  {
    PROPERLY_ALIGNED_TYPE dummy;
    byte keyschedule[MAXROUNDS+1][4][4];
  } u2;
  int rounds;               /* Key-length-dependent number of rounds.  */
  int decryption_prepared;  /* The decryption key schedule is available.  */
#ifdef USE_PADLOCK
  int use_padlock;          /* Padlock shall be used.  */
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI
  int use_aesni;            /* AES-NI shall be used.  */
#endif /*USE_AESNI*/
} RIJNDAEL_context ATTR_ALIGNED_16;

/* Macros defining alias for the keyschedules.  */
#define keyschenc  u1.keyschedule
#define keyschdec  u2.keyschedule
#define padlockkey u1.padlock_key

/* Two macros to be called prior and after the use of AESNI
   instructions.  There should be no external function calls between
   the use of these macros.  There purpose is to make sure that the
   SSE regsiters are cleared and won't reveal any information about
   the key or the data.  */
#ifdef USE_AESNI
# define aesni_prepare() do { } while (0)
# define aesni_cleanup()                                                \
  do { asm volatile ("pxor %%xmm0, %%xmm0\n\t"                          \
                     "pxor %%xmm1, %%xmm1\n" :: );                      \
  } while (0)
# define aesni_cleanup_2_4()                                            \
  do { asm volatile ("pxor %%xmm2, %%xmm2\n\t"                          \
                     "pxor %%xmm3, %%xmm3\n"                            \
                     "pxor %%xmm4, %%xmm4\n":: );                       \
  } while (0)
#else
# define aesni_prepare() do { } while (0)
# define aesni_cleanup() do { } while (0)
#endif


/* All the numbers.  */
#include "rijndael-tables.h"



/* Function prototypes.  */
#ifdef USE_AESNI
/* We don't want to inline these functions to help gcc allocate enough
   registers.  */
static void do_aesni_ctr (const RIJNDAEL_context *ctx, unsigned char *ctr,
                          unsigned char *b, const unsigned char *a)
  __attribute__ ((__noinline__));
static void do_aesni_ctr_4 (const RIJNDAEL_context *ctx, unsigned char *ctr,
                            unsigned char *b, const unsigned char *a)
  __attribute__ ((__noinline__));
#endif /*USE_AESNI*/

static const char *selftest(void);



/* Perform the key setup.  */
static gcry_err_code_t
do_setkey (RIJNDAEL_context *ctx, const byte *key, const unsigned keylen)
{
  static int initialized = 0;
  static const char *selftest_failed=0;
  int rounds;
  int i,j, r, t, rconpointer = 0;
  int KC;
  union
  {
    PROPERLY_ALIGNED_TYPE dummy;
    byte k[MAXKC][4];
  } k;
#define k k.k
  union
  {
    PROPERLY_ALIGNED_TYPE dummy;
    byte tk[MAXKC][4];
  } tk;
#define tk tk.tk

  /* The on-the-fly self tests are only run in non-fips mode. In fips
     mode explicit self-tests are required.  Actually the on-the-fly
     self-tests are not fully thread-safe and it might happen that a
     failed self-test won't get noticed in another thread.

     FIXME: We might want to have a central registry of succeeded
     self-tests. */
  if (!fips_mode () && !initialized)
    {
      initialized = 1;
      selftest_failed = selftest ();
      if (selftest_failed)
        log_error ("%s\n", selftest_failed );
    }
  if (selftest_failed)
    return GPG_ERR_SELFTEST_FAILED;

  ctx->decryption_prepared = 0;
#ifdef USE_PADLOCK
  ctx->use_padlock = 0;
#endif
#ifdef USE_AESNI
  ctx->use_aesni = 0;
#endif

  if( keylen == 128/8 )
    {
      rounds = 10;
      KC = 4;

      if (0)
        ;
#ifdef USE_PADLOCK
      else if ((_gcry_get_hw_features () & HWF_PADLOCK_AES))
        {
          ctx->use_padlock = 1;
          memcpy (ctx->padlockkey, key, keylen);
        }
#endif
#ifdef USE_AESNI
      else if ((_gcry_get_hw_features () & HWF_INTEL_AESNI))
        {
          ctx->use_aesni = 1;
        }
#endif
    }
  else if ( keylen == 192/8 )
    {
      rounds = 12;
      KC = 6;

      if (0)
        {
          ;
        }
#ifdef USE_AESNI
      else if ((_gcry_get_hw_features () & HWF_INTEL_AESNI))
        {
          ctx->use_aesni = 1;
        }
#endif
    }
  else if ( keylen == 256/8 )
    {
      rounds = 14;
      KC = 8;

      if (0)
        {
          ;
        }
#ifdef USE_AESNI
      else if ((_gcry_get_hw_features () & HWF_INTEL_AESNI))
        {
          ctx->use_aesni = 1;
        }
#endif
    }
  else
    return GPG_ERR_INV_KEYLEN;

  ctx->rounds = rounds;

  if (0)
    ;
#ifdef USE_PADLOCK
  else if (ctx->use_padlock)
    {
      /* Nothing to do as we support only hardware key generation for
         now.  */
    }
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI_is_disabled_here
  else if (ctx->use_aesni && ctx->rounds == 10)
    {
      /* Note: This code works for AES-128 but it is not much better
         than using the standard key schedule.  We disable it for
         now and don't put any effort into implementing this for
         AES-192 and AES-256.  */
      asm volatile ("movl   %[key], %%esi\n\t"
                    "movdqu (%%esi), %%xmm1\n\t"     /* xmm1 := key   */
                    "movl   %[ksch], %%esi\n\t"
                    "movdqa %%xmm1, (%%esi)\n\t"     /* ksch[0] := xmm1  */
                    "aeskeygenassist $0x01, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x10(%%esi)\n\t" /* ksch[1] := xmm1  */
                    "aeskeygenassist $0x02, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x20(%%esi)\n\t" /* ksch[2] := xmm1  */
                    "aeskeygenassist $0x04, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x30(%%esi)\n\t" /* ksch[3] := xmm1  */
                    "aeskeygenassist $0x08, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x40(%%esi)\n\t" /* ksch[4] := xmm1  */
                    "aeskeygenassist $0x10, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x50(%%esi)\n\t" /* ksch[5] := xmm1  */
                    "aeskeygenassist $0x20, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x60(%%esi)\n\t" /* ksch[6] := xmm1  */
                    "aeskeygenassist $0x40, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x70(%%esi)\n\t" /* ksch[7] := xmm1  */
                    "aeskeygenassist $0x80, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x80(%%esi)\n\t" /* ksch[8] := xmm1  */
                    "aeskeygenassist $0x1b, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0x90(%%esi)\n\t" /* ksch[9] := xmm1  */
                    "aeskeygenassist $0x36, %%xmm1, %%xmm2\n\t"
                    "call .Lexpand128_%=\n\t"
                    "movdqa %%xmm1, 0xa0(%%esi)\n\t" /* ksch[10] := xmm1  */
                    "jmp .Lleave%=\n"

                    ".Lexpand128_%=:\n\t"
                    "pshufd $0xff, %%xmm2, %%xmm2\n\t"
                    "movdqa %%xmm1, %%xmm3\n\t"
                    "pslldq $4, %%xmm3\n\t"
                    "pxor   %%xmm3, %%xmm1\n\t"
                    "pslldq $4, %%xmm3\n\t"
                    "pxor   %%xmm3, %%xmm1\n\t"
                    "pslldq $4, %%xmm3\n\t"
                    "pxor   %%xmm3, %%xmm2\n\t"
                    "pxor   %%xmm2, %%xmm1\n\t"
                    "ret\n"

                    ".Lleave%=:\n\t"
                    "pxor %%xmm1, %%xmm1\n\t"
                    "pxor %%xmm2, %%xmm2\n\t"
                    "pxor %%xmm3, %%xmm3\n"
                    :
                    : [key] "g" (key), [ksch] "g" (ctx->keyschenc)
                    : "%esi", "cc", "memory" );
    }
#endif /*USE_AESNI*/
  else
    {
#define W (ctx->keyschenc)
      for (i = 0; i < keylen; i++)
        {
          k[i >> 2][i & 3] = key[i];
        }

      for (j = KC-1; j >= 0; j--)
        {
          *((u32*)tk[j]) = *((u32*)k[j]);
        }
      r = 0;
      t = 0;
      /* Copy values into round key array.  */
      for (j = 0; (j < KC) && (r < rounds + 1); )
        {
          for (; (j < KC) && (t < 4); j++, t++)
            {
              *((u32*)W[r][t]) = *((u32*)tk[j]);
            }
          if (t == 4)
            {
              r++;
              t = 0;
            }
        }

      while (r < rounds + 1)
        {
          /* While not enough round key material calculated calculate
             new values.  */
          tk[0][0] ^= S[tk[KC-1][1]];
          tk[0][1] ^= S[tk[KC-1][2]];
          tk[0][2] ^= S[tk[KC-1][3]];
          tk[0][3] ^= S[tk[KC-1][0]];
          tk[0][0] ^= rcon[rconpointer++];

          if (KC != 8)
            {
              for (j = 1; j < KC; j++)
                {
                  *((u32*)tk[j]) ^= *((u32*)tk[j-1]);
                }
            }
          else
            {
              for (j = 1; j < KC/2; j++)
                {
                  *((u32*)tk[j]) ^= *((u32*)tk[j-1]);
                }
              tk[KC/2][0] ^= S[tk[KC/2 - 1][0]];
              tk[KC/2][1] ^= S[tk[KC/2 - 1][1]];
              tk[KC/2][2] ^= S[tk[KC/2 - 1][2]];
              tk[KC/2][3] ^= S[tk[KC/2 - 1][3]];
              for (j = KC/2 + 1; j < KC; j++)
                {
                  *((u32*)tk[j]) ^= *((u32*)tk[j-1]);
                }
            }

          /* Copy values into round key array.  */
          for (j = 0; (j < KC) && (r < rounds + 1); )
            {
              for (; (j < KC) && (t < 4); j++, t++)
                {
                  *((u32*)W[r][t]) = *((u32*)tk[j]);
                }
              if (t == 4)
                {
                  r++;
                  t = 0;
                }
            }
        }
#undef W
    }

  return 0;
#undef tk
#undef k
}


static gcry_err_code_t
rijndael_setkey (void *context, const byte *key, const unsigned keylen)
{
  RIJNDAEL_context *ctx = context;

  int rc = do_setkey (ctx, key, keylen);
  _gcry_burn_stack ( 100 + 16*sizeof(int));
  return rc;
}


/* Make a decryption key from an encryption key. */
static void
prepare_decryption( RIJNDAEL_context *ctx )
{
  int r;

#ifdef USE_AESNI
  if (ctx->use_aesni)
    {
      /* The AES-NI decrypt instructions use the Equivalent Inverse
         Cipher, thus we can't use the the standard decrypt key
         preparation.  */
        m128i_t *ekey = (m128i_t*)ctx->keyschenc;
        m128i_t *dkey = (m128i_t*)ctx->keyschdec;
        int rr;

        dkey[0] = ekey[ctx->rounds];
        for (r=1, rr=ctx->rounds-1; r < ctx->rounds; r++, rr--)
          {
            asm volatile
              ("movdqu %[ekey], %%xmm1\n\t"
               /*"aesimc %%xmm1, %%xmm1\n\t"*/
               ".byte 0x66, 0x0f, 0x38, 0xdb, 0xc9\n\t"
               "movdqu %%xmm1, %[dkey]"
               : [dkey] "=m" (dkey[r])
               : [ekey] "m" (ekey[rr]) );
          }
        dkey[r] = ekey[0];
    }
  else
#endif /*USE_AESNI*/
    {
      union
      {
        PROPERLY_ALIGNED_TYPE dummy;
        byte *w;
      } w;
#define w w.w

      for (r=0; r < MAXROUNDS+1; r++ )
        {
          *((u32*)ctx->keyschdec[r][0]) = *((u32*)ctx->keyschenc[r][0]);
          *((u32*)ctx->keyschdec[r][1]) = *((u32*)ctx->keyschenc[r][1]);
          *((u32*)ctx->keyschdec[r][2]) = *((u32*)ctx->keyschenc[r][2]);
          *((u32*)ctx->keyschdec[r][3]) = *((u32*)ctx->keyschenc[r][3]);
        }
#define W (ctx->keyschdec)
      for (r = 1; r < ctx->rounds; r++)
        {
          w = W[r][0];
          *((u32*)w) = *((u32*)U1[w[0]]) ^ *((u32*)U2[w[1]])
            ^ *((u32*)U3[w[2]]) ^ *((u32*)U4[w[3]]);

          w = W[r][1];
          *((u32*)w) = *((u32*)U1[w[0]]) ^ *((u32*)U2[w[1]])
            ^ *((u32*)U3[w[2]]) ^ *((u32*)U4[w[3]]);

          w = W[r][2];
          *((u32*)w) = *((u32*)U1[w[0]]) ^ *((u32*)U2[w[1]])
        ^ *((u32*)U3[w[2]]) ^ *((u32*)U4[w[3]]);

          w = W[r][3];
          *((u32*)w) = *((u32*)U1[w[0]]) ^ *((u32*)U2[w[1]])
            ^ *((u32*)U3[w[2]]) ^ *((u32*)U4[w[3]]);
        }
#undef W
#undef w
    }
}


/* Encrypt one block.  A and B need to be aligned on a 4 byte
   boundary.  A and B may be the same. */
static void
do_encrypt_aligned (const RIJNDAEL_context *ctx,
                    unsigned char *b, const unsigned char *a)
{
#define rk (ctx->keyschenc)
  int rounds = ctx->rounds;
  int r;
  union
  {
    u32  tempu32[4];  /* Force correct alignment. */
    byte temp[4][4];
  } u;

  *((u32_a_t*)u.temp[0]) = *((u32_a_t*)(a   )) ^ *((u32_a_t*)rk[0][0]);
  *((u32_a_t*)u.temp[1]) = *((u32_a_t*)(a+ 4)) ^ *((u32_a_t*)rk[0][1]);
  *((u32_a_t*)u.temp[2]) = *((u32_a_t*)(a+ 8)) ^ *((u32_a_t*)rk[0][2]);
  *((u32_a_t*)u.temp[3]) = *((u32_a_t*)(a+12)) ^ *((u32_a_t*)rk[0][3]);
  *((u32_a_t*)(b    ))   = (*((u32_a_t*)T1[u.temp[0][0]])
                        ^ *((u32_a_t*)T2[u.temp[1][1]])
                        ^ *((u32_a_t*)T3[u.temp[2][2]])
                        ^ *((u32_a_t*)T4[u.temp[3][3]]));
  *((u32_a_t*)(b + 4))   = (*((u32_a_t*)T1[u.temp[1][0]])
                        ^ *((u32_a_t*)T2[u.temp[2][1]])
                        ^ *((u32_a_t*)T3[u.temp[3][2]])
                        ^ *((u32_a_t*)T4[u.temp[0][3]]));
  *((u32_a_t*)(b + 8))   = (*((u32_a_t*)T1[u.temp[2][0]])
                        ^ *((u32_a_t*)T2[u.temp[3][1]])
                        ^ *((u32_a_t*)T3[u.temp[0][2]])
                        ^ *((u32_a_t*)T4[u.temp[1][3]]));
  *((u32_a_t*)(b +12))   = (*((u32_a_t*)T1[u.temp[3][0]])
                        ^ *((u32_a_t*)T2[u.temp[0][1]])
                        ^ *((u32_a_t*)T3[u.temp[1][2]])
                        ^ *((u32_a_t*)T4[u.temp[2][3]]));

  for (r = 1; r < rounds-1; r++)
    {
      *((u32_a_t*)u.temp[0]) = *((u32_a_t*)(b   )) ^ *((u32_a_t*)rk[r][0]);
      *((u32_a_t*)u.temp[1]) = *((u32_a_t*)(b+ 4)) ^ *((u32_a_t*)rk[r][1]);
      *((u32_a_t*)u.temp[2]) = *((u32_a_t*)(b+ 8)) ^ *((u32_a_t*)rk[r][2]);
      *((u32_a_t*)u.temp[3]) = *((u32_a_t*)(b+12)) ^ *((u32_a_t*)rk[r][3]);

      *((u32_a_t*)(b    ))   = (*((u32_a_t*)T1[u.temp[0][0]])
                            ^ *((u32_a_t*)T2[u.temp[1][1]])
                            ^ *((u32_a_t*)T3[u.temp[2][2]])
                            ^ *((u32_a_t*)T4[u.temp[3][3]]));
      *((u32_a_t*)(b + 4))   = (*((u32_a_t*)T1[u.temp[1][0]])
                            ^ *((u32_a_t*)T2[u.temp[2][1]])
                            ^ *((u32_a_t*)T3[u.temp[3][2]])
                            ^ *((u32_a_t*)T4[u.temp[0][3]]));
      *((u32_a_t*)(b + 8))   = (*((u32_a_t*)T1[u.temp[2][0]])
                            ^ *((u32_a_t*)T2[u.temp[3][1]])
                            ^ *((u32_a_t*)T3[u.temp[0][2]])
                            ^ *((u32_a_t*)T4[u.temp[1][3]]));
      *((u32_a_t*)(b +12))   = (*((u32_a_t*)T1[u.temp[3][0]])
                            ^ *((u32_a_t*)T2[u.temp[0][1]])
                            ^ *((u32_a_t*)T3[u.temp[1][2]])
                            ^ *((u32_a_t*)T4[u.temp[2][3]]));
    }

  /* Last round is special. */
  *((u32_a_t*)u.temp[0]) = *((u32_a_t*)(b   )) ^ *((u32_a_t*)rk[rounds-1][0]);
  *((u32_a_t*)u.temp[1]) = *((u32_a_t*)(b+ 4)) ^ *((u32_a_t*)rk[rounds-1][1]);
  *((u32_a_t*)u.temp[2]) = *((u32_a_t*)(b+ 8)) ^ *((u32_a_t*)rk[rounds-1][2]);
  *((u32_a_t*)u.temp[3]) = *((u32_a_t*)(b+12)) ^ *((u32_a_t*)rk[rounds-1][3]);
  b[ 0] = T1[u.temp[0][0]][1];
  b[ 1] = T1[u.temp[1][1]][1];
  b[ 2] = T1[u.temp[2][2]][1];
  b[ 3] = T1[u.temp[3][3]][1];
  b[ 4] = T1[u.temp[1][0]][1];
  b[ 5] = T1[u.temp[2][1]][1];
  b[ 6] = T1[u.temp[3][2]][1];
  b[ 7] = T1[u.temp[0][3]][1];
  b[ 8] = T1[u.temp[2][0]][1];
  b[ 9] = T1[u.temp[3][1]][1];
  b[10] = T1[u.temp[0][2]][1];
  b[11] = T1[u.temp[1][3]][1];
  b[12] = T1[u.temp[3][0]][1];
  b[13] = T1[u.temp[0][1]][1];
  b[14] = T1[u.temp[1][2]][1];
  b[15] = T1[u.temp[2][3]][1];
  *((u32_a_t*)(b   )) ^= *((u32_a_t*)rk[rounds][0]);
  *((u32_a_t*)(b+ 4)) ^= *((u32_a_t*)rk[rounds][1]);
  *((u32_a_t*)(b+ 8)) ^= *((u32_a_t*)rk[rounds][2]);
  *((u32_a_t*)(b+12)) ^= *((u32_a_t*)rk[rounds][3]);
#undef rk
}


static void
do_encrypt (const RIJNDAEL_context *ctx,
            unsigned char *bx, const unsigned char *ax)
{
  /* BX and AX are not necessary correctly aligned.  Thus we might
     need to copy them here.  We try to align to a 16 bytes.  */
  if (((size_t)ax & 0x0f) || ((size_t)bx & 0x0f))
    {
      union
      {
        u32  dummy[4];
        byte a[16] ATTR_ALIGNED_16;
      } a;
      union
      {
        u32  dummy[4];
        byte b[16] ATTR_ALIGNED_16;
      } b;

      memcpy (a.a, ax, 16);
      do_encrypt_aligned (ctx, b.b, a.a);
      memcpy (bx, b.b, 16);
    }
  else
    {
      do_encrypt_aligned (ctx, bx, ax);
    }
}


/* Encrypt or decrypt one block using the padlock engine.  A and B may
   be the same. */
#ifdef USE_PADLOCK
static void
do_padlock (const RIJNDAEL_context *ctx, int decrypt_flag,
            unsigned char *bx, const unsigned char *ax)
{
  /* BX and AX are not necessary correctly aligned.  Thus we need to
     copy them here. */
  unsigned char a[16] __attribute__ ((aligned (16)));
  unsigned char b[16] __attribute__ ((aligned (16)));
  unsigned int cword[4] __attribute__ ((aligned (16)));

  /* The control word fields are:
      127:12   11:10 9     8     7     6     5     4     3:0
      RESERVED KSIZE CRYPT INTER KEYGN CIPHR ALIGN DGEST ROUND  */
  cword[0] = (ctx->rounds & 15);  /* (The mask is just a safeguard.)  */
  cword[1] = 0;
  cword[2] = 0;
  cword[3] = 0;
  if (decrypt_flag)
    cword[0] |= 0x00000200;

  memcpy (a, ax, 16);

  asm volatile
    ("pushfl\n\t"          /* Force key reload.  */
     "popfl\n\t"
     "xchg %3, %%ebx\n\t"  /* Load key.  */
     "movl $1, %%ecx\n\t"  /* Init counter for just one block.  */
     ".byte 0xf3, 0x0f, 0xa7, 0xc8\n\t" /* REP XSTORE ECB. */
     "xchg %3, %%ebx\n"    /* Restore GOT register.  */
     : /* No output */
     : "S" (a), "D" (b), "d" (cword), "r" (ctx->padlockkey)
     : "%ecx", "cc", "memory"
     );

  memcpy (bx, b, 16);

}
#endif /*USE_PADLOCK*/


#ifdef USE_AESNI
/* Encrypt one block using the Intel AES-NI instructions.  A and B may
   be the same; they need to be properly aligned to 16 bytes.

   Our problem here is that gcc does not allow the "x" constraint for
   SSE registers in asm unless you compile with -msse.  The common
   wisdom is to use a separate file for SSE instructions and build it
   separately.  This would require a lot of extra build system stuff,
   similar to what we do in mpi/ for the asm stuff.  What we do
   instead is to use standard registers and a bit more of plain asm
   which copies the data and key stuff to the SSE registers and later
   back.  If we decide to implement some block modes with parallelized
   AES instructions, it might indeed be better to use plain asm ala
   mpi/.  */
static void
do_aesni_enc_aligned (const RIJNDAEL_context *ctx,
                      unsigned char *b, const unsigned char *a)
{
#define aesenc_xmm1_xmm0      ".byte 0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
#define aesenclast_xmm1_xmm0  ".byte 0x66, 0x0f, 0x38, 0xdd, 0xc1\n\t"
  /* Note: For now we relax the alignment requirement for A and B: It
     does not make much difference because in many case we would need
     to memcpy them to an extra buffer; using the movdqu is much faster
     that memcpy and movdqa.  For CFB we know that the IV is properly
     aligned but that is a special case.  We should better implement
     CFB direct in asm.  */
  asm volatile ("movdqu %[src], %%xmm0\n\t"     /* xmm0 := *a     */
                "movl   %[key], %%esi\n\t"      /* esi  := keyschenc */
                "movdqa (%%esi), %%xmm1\n\t"    /* xmm1 := key[0] */
                "pxor   %%xmm1, %%xmm0\n\t"     /* xmm0 ^= key[0] */
                "movdqa 0x10(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x20(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x30(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x40(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x50(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x60(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x70(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x80(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x90(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xa0(%%esi), %%xmm1\n\t"
                "cmp $10, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xb0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xc0(%%esi), %%xmm1\n\t"
                "cmp $12, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xd0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xe0(%%esi), %%xmm1\n"

                ".Lenclast%=:\n\t"
                aesenclast_xmm1_xmm0
                "movdqu %%xmm0, %[dst]\n"
                : [dst] "=m" (*b)
                : [src] "m" (*a),
                  [key] "r" (ctx->keyschenc),
                  [rounds] "r" (ctx->rounds)
                : "%esi", "cc", "memory");
#undef aesenc_xmm1_xmm0
#undef aesenclast_xmm1_xmm0
}


static void
do_aesni_dec_aligned (const RIJNDAEL_context *ctx,
                      unsigned char *b, const unsigned char *a)
{
#define aesdec_xmm1_xmm0      ".byte 0x66, 0x0f, 0x38, 0xde, 0xc1\n\t"
#define aesdeclast_xmm1_xmm0  ".byte 0x66, 0x0f, 0x38, 0xdf, 0xc1\n\t"
  asm volatile ("movdqu %[src], %%xmm0\n\t"     /* xmm0 := *a     */
                "movl   %[key], %%esi\n\t"
                "movdqa (%%esi), %%xmm1\n\t"
                "pxor   %%xmm1, %%xmm0\n\t"     /* xmm0 ^= key[0] */
                "movdqa 0x10(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x20(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x30(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x40(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x50(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x60(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x70(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x80(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0x90(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0xa0(%%esi), %%xmm1\n\t"
                "cmp $10, %[rounds]\n\t"
                "jz .Ldeclast%=\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0xb0(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0xc0(%%esi), %%xmm1\n\t"
                "cmp $12, %[rounds]\n\t"
                "jz .Ldeclast%=\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0xd0(%%esi), %%xmm1\n\t"
                aesdec_xmm1_xmm0
                "movdqa 0xe0(%%esi), %%xmm1\n"

                ".Ldeclast%=:\n\t"
                aesdeclast_xmm1_xmm0
                "movdqu %%xmm0, %[dst]\n"
                : [dst] "=m" (*b)
                : [src] "m" (*a),
                  [key] "r" (ctx->keyschdec),
                  [rounds] "r" (ctx->rounds)
                : "%esi", "cc", "memory");
#undef aesdec_xmm1_xmm0
#undef aesdeclast_xmm1_xmm0
}


/* Perform a CFB encryption or decryption round using the
   initialization vector IV and the input block A.  Write the result
   to the output block B and update IV.  IV needs to be 16 byte
   aligned.  */
static void
do_aesni_cfb (const RIJNDAEL_context *ctx, int decrypt_flag,
              unsigned char *iv, unsigned char *b, const unsigned char *a)
{
#define aesenc_xmm1_xmm0      ".byte 0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
#define aesenclast_xmm1_xmm0  ".byte 0x66, 0x0f, 0x38, 0xdd, 0xc1\n\t"
  asm volatile ("movdqa %[iv], %%xmm0\n\t"      /* xmm0 := IV     */
                "movl   %[key], %%esi\n\t"      /* esi  := keyschenc */
                "movdqa (%%esi), %%xmm1\n\t"    /* xmm1 := key[0] */
                "pxor   %%xmm1, %%xmm0\n\t"     /* xmm0 ^= key[0] */
                "movdqa 0x10(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x20(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x30(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x40(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x50(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x60(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x70(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x80(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x90(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xa0(%%esi), %%xmm1\n\t"
                "cmp $10, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xb0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xc0(%%esi), %%xmm1\n\t"
                "cmp $12, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xd0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xe0(%%esi), %%xmm1\n"

                ".Lenclast%=:\n\t"
                aesenclast_xmm1_xmm0
                "movdqu %[src], %%xmm1\n\t"      /* Save input.  */
                "pxor %%xmm1, %%xmm0\n\t"        /* xmm0 = input ^ IV  */

                "cmp $1, %[decrypt]\n\t"
                "jz .Ldecrypt_%=\n\t"
                "movdqa %%xmm0, %[iv]\n\t"       /* [encrypt] Store IV.  */
                "jmp .Lleave_%=\n"
                ".Ldecrypt_%=:\n\t"
                "movdqa %%xmm1, %[iv]\n"         /* [decrypt] Store IV.  */
                ".Lleave_%=:\n\t"
                "movdqu %%xmm0, %[dst]\n"        /* Store output.   */
                : [iv] "+m" (*iv), [dst] "=m" (*b)
                : [src] "m" (*a),
                  [key] "g" (ctx->keyschenc),
                  [rounds] "g" (ctx->rounds),
                  [decrypt] "m" (decrypt_flag)
                : "%esi", "cc", "memory");
#undef aesenc_xmm1_xmm0
#undef aesenclast_xmm1_xmm0
}

/* Perform a CTR encryption round using the counter CTR and the input
   block A.  Write the result to the output block B and update CTR.
   CTR needs to be a 16 byte aligned little-endian value.  */
static void
do_aesni_ctr (const RIJNDAEL_context *ctx,
              unsigned char *ctr, unsigned char *b, const unsigned char *a)
{
#define aesenc_xmm1_xmm0      ".byte 0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
#define aesenclast_xmm1_xmm0  ".byte 0x66, 0x0f, 0x38, 0xdd, 0xc1\n\t"
  static unsigned char be_mask[16] __attribute__ ((aligned (16))) =
    { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

  asm volatile ("movdqa %[ctr], %%xmm0\n\t"     /* xmm0, xmm2 := CTR   */
                "movaps %%xmm0, %%xmm2\n\t"
                "mov    $1, %%esi\n\t"          /* xmm2++ (big-endian) */
                "movd   %%esi, %%xmm1\n\t"
                "pshufb %[mask], %%xmm2\n\t"
                "paddq  %%xmm1, %%xmm2\n\t"
                "pshufb %[mask], %%xmm2\n\t"
                "movdqa %%xmm2, %[ctr]\n"       /* Update CTR.         */

                "movl   %[key], %%esi\n\t"      /* esi  := keyschenc */
                "movdqa (%%esi), %%xmm1\n\t"    /* xmm1 := key[0]    */
                "pxor   %%xmm1, %%xmm0\n\t"     /* xmm0 ^= key[0]    */
                "movdqa 0x10(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x20(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x30(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x40(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x50(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x60(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x70(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x80(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0x90(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xa0(%%esi), %%xmm1\n\t"
                "cmp $10, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xb0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xc0(%%esi), %%xmm1\n\t"
                "cmp $12, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xd0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                "movdqa 0xe0(%%esi), %%xmm1\n"

                ".Lenclast%=:\n\t"
                aesenclast_xmm1_xmm0
                "movdqu %[src], %%xmm1\n\t"      /* xmm1 := input   */
                "pxor %%xmm1, %%xmm0\n\t"        /* EncCTR ^= input  */
                "movdqu %%xmm0, %[dst]"          /* Store EncCTR.    */

                : [ctr] "+m" (*ctr), [dst] "=m" (*b)
                : [src] "m" (*a),
                  [key] "g" (ctx->keyschenc),
                  [rounds] "g" (ctx->rounds),
                  [mask] "m" (*be_mask)
                : "%esi", "cc", "memory");
#undef aesenc_xmm1_xmm0
#undef aesenclast_xmm1_xmm0
}


/* Four blocks at a time variant of do_aesni_ctr.  */
static void
do_aesni_ctr_4 (const RIJNDAEL_context *ctx,
                unsigned char *ctr, unsigned char *b, const unsigned char *a)
{
#define aesenc_xmm1_xmm0      ".byte 0x66, 0x0f, 0x38, 0xdc, 0xc1\n\t"
#define aesenc_xmm1_xmm2      ".byte 0x66, 0x0f, 0x38, 0xdc, 0xd1\n\t"
#define aesenc_xmm1_xmm3      ".byte 0x66, 0x0f, 0x38, 0xdc, 0xd9\n\t"
#define aesenc_xmm1_xmm4      ".byte 0x66, 0x0f, 0x38, 0xdc, 0xe1\n\t"
#define aesenclast_xmm1_xmm0  ".byte 0x66, 0x0f, 0x38, 0xdd, 0xc1\n\t"
#define aesenclast_xmm1_xmm2  ".byte 0x66, 0x0f, 0x38, 0xdd, 0xd1\n\t"
#define aesenclast_xmm1_xmm3  ".byte 0x66, 0x0f, 0x38, 0xdd, 0xd9\n\t"
#define aesenclast_xmm1_xmm4  ".byte 0x66, 0x0f, 0x38, 0xdd, 0xe1\n\t"

  static unsigned char be_mask[16] __attribute__ ((aligned (16))) =
    { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

  /* Register usage:
      esi   keyschedule
      xmm0  CTR-0
      xmm1  temp / round key
      xmm2  CTR-1
      xmm3  CTR-2
      xmm4  CTR-3
      xmm5  temp
   */

  asm volatile ("movdqa %[ctr], %%xmm0\n\t"     /* xmm0, xmm2 := CTR   */
                "movaps %%xmm0, %%xmm2\n\t"
                "mov    $1, %%esi\n\t"          /* xmm1 := 1 */
                "movd   %%esi, %%xmm1\n\t"
                "pshufb %[mask], %%xmm2\n\t"    /* xmm2 := le(xmm2) */
                "paddq  %%xmm1, %%xmm2\n\t"     /* xmm2++           */
                "movaps %%xmm2, %%xmm3\n\t"     /* xmm3 := xmm2     */
                "paddq  %%xmm1, %%xmm3\n\t"     /* xmm3++           */
                "movaps %%xmm3, %%xmm4\n\t"     /* xmm4 := xmm3     */
                "paddq  %%xmm1, %%xmm4\n\t"     /* xmm4++           */
                "movaps %%xmm4, %%xmm5\n\t"     /* xmm5 := xmm4     */
                "paddq  %%xmm1, %%xmm5\n\t"     /* xmm5++           */
                "pshufb %[mask], %%xmm2\n\t"    /* xmm2 := be(xmm2) */
                "pshufb %[mask], %%xmm3\n\t"    /* xmm3 := be(xmm3) */
                "pshufb %[mask], %%xmm4\n\t"    /* xmm4 := be(xmm4) */
                "pshufb %[mask], %%xmm5\n\t"    /* xmm5 := be(xmm5) */
                "movdqa %%xmm5, %[ctr]\n"       /* Update CTR.      */

                "movl   %[key], %%esi\n\t"      /* esi  := keyschenc */
                "movdqa (%%esi), %%xmm1\n\t"    /* xmm1 := key[0]    */
                "pxor   %%xmm1, %%xmm0\n\t"     /* xmm0 ^= key[0]    */
                "pxor   %%xmm1, %%xmm2\n\t"     /* xmm2 ^= key[0]    */
                "pxor   %%xmm1, %%xmm3\n\t"     /* xmm3 ^= key[0]    */
                "pxor   %%xmm1, %%xmm4\n\t"     /* xmm4 ^= key[0]    */
                "movdqa 0x10(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x20(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x30(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x40(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x50(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x60(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x70(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x80(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0x90(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0xa0(%%esi), %%xmm1\n\t"
                "cmp $10, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0xb0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0xc0(%%esi), %%xmm1\n\t"
                "cmp $12, %[rounds]\n\t"
                "jz .Lenclast%=\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0xd0(%%esi), %%xmm1\n\t"
                aesenc_xmm1_xmm0
                aesenc_xmm1_xmm2
                aesenc_xmm1_xmm3
                aesenc_xmm1_xmm4
                "movdqa 0xe0(%%esi), %%xmm1\n"

                ".Lenclast%=:\n\t"
                aesenclast_xmm1_xmm0
                aesenclast_xmm1_xmm2
                aesenclast_xmm1_xmm3
                aesenclast_xmm1_xmm4

                "movdqu %[src], %%xmm1\n\t"      /* Get block 1.      */
                "pxor %%xmm1, %%xmm0\n\t"        /* EncCTR-1 ^= input */
                "movdqu %%xmm0, %[dst]\n\t"      /* Store block 1     */

                "movdqu (16)%[src], %%xmm1\n\t"  /* Get block 2.      */
                "pxor %%xmm1, %%xmm2\n\t"        /* EncCTR-2 ^= input */
                "movdqu %%xmm2, (16)%[dst]\n\t"  /* Store block 2.    */

                "movdqu (32)%[src], %%xmm1\n\t"  /* Get block 3.      */
                "pxor %%xmm1, %%xmm3\n\t"        /* EncCTR-3 ^= input */
                "movdqu %%xmm3, (32)%[dst]\n\t"  /* Store block 3.    */

                "movdqu (48)%[src], %%xmm1\n\t"  /* Get block 4.      */
                "pxor %%xmm1, %%xmm4\n\t"        /* EncCTR-4 ^= input */
                "movdqu %%xmm4, (48)%[dst]"      /* Store block 4.   */

                : [ctr] "+m" (*ctr), [dst] "=m" (*b)
                : [src] "m" (*a),
                  [key] "g" (ctx->keyschenc),
                  [rounds] "g" (ctx->rounds),
                  [mask] "m" (*be_mask)
                : "%esi", "cc", "memory");
#undef aesenc_xmm1_xmm0
#undef aesenc_xmm1_xmm2
#undef aesenc_xmm1_xmm3
#undef aesenc_xmm1_xmm4
#undef aesenclast_xmm1_xmm0
#undef aesenclast_xmm1_xmm2
#undef aesenclast_xmm1_xmm3
#undef aesenclast_xmm1_xmm4
}


static void
do_aesni (RIJNDAEL_context *ctx, int decrypt_flag,
          unsigned char *bx, const unsigned char *ax)
{

  if (decrypt_flag)
    {
      if (!ctx->decryption_prepared )
        {
          prepare_decryption ( ctx );
          ctx->decryption_prepared = 1;
        }
      do_aesni_dec_aligned (ctx, bx, ax);
    }
  else
    do_aesni_enc_aligned (ctx, bx, ax);
}
#endif /*USE_AESNI*/


static void
rijndael_encrypt (void *context, byte *b, const byte *a)
{
  RIJNDAEL_context *ctx = context;

  if (0)
    ;
#ifdef USE_PADLOCK
  else if (ctx->use_padlock)
    {
      do_padlock (ctx, 0, b, a);
      _gcry_burn_stack (48 + 15 /* possible padding for alignment */);
    }
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI
  else if (ctx->use_aesni)
    {
      aesni_prepare ();
      do_aesni (ctx, 0, b, a);
      aesni_cleanup ();
    }
#endif /*USE_AESNI*/
  else
    {
      do_encrypt (ctx, b, a);
      _gcry_burn_stack (56 + 2*sizeof(int));
    }
}


/* Bulk encryption of complete blocks in CFB mode.  Caller needs to
   make sure that IV is aligned on an unsigned long boundary.  This
   function is only intended for the bulk encryption feature of
   cipher.c. */
void
_gcry_aes_cfb_enc (void *context, unsigned char *iv,
                   void *outbuf_arg, const void *inbuf_arg,
                   unsigned int nblocks)
{
  RIJNDAEL_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  unsigned char *ivp;
  int i;

  if (0)
    ;
#ifdef USE_PADLOCK
  else if (ctx->use_padlock)
    {
      /* Fixme: Let Padlock do the CFBing.  */
      for ( ;nblocks; nblocks-- )
        {
          /* Encrypt the IV. */
          do_padlock (ctx, 0, iv, iv);
          /* XOR the input with the IV and store input into IV.  */
          for (ivp=iv,i=0; i < BLOCKSIZE; i++ )
            *outbuf++ = (*ivp++ ^= *inbuf++);
        }
    }
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI
  else if (ctx->use_aesni)
    {
      aesni_prepare ();
      for ( ;nblocks; nblocks-- )
        {
          do_aesni_cfb (ctx, 0, iv, outbuf, inbuf);
          outbuf += BLOCKSIZE;
          inbuf  += BLOCKSIZE;
        }
      aesni_cleanup ();
    }
#endif /*USE_AESNI*/
  else
    {
      for ( ;nblocks; nblocks-- )
        {
          /* Encrypt the IV. */
          do_encrypt_aligned (ctx, iv, iv);
          /* XOR the input with the IV and store input into IV.  */
          for (ivp=iv,i=0; i < BLOCKSIZE; i++ )
            *outbuf++ = (*ivp++ ^= *inbuf++);
        }
    }

  _gcry_burn_stack (48 + 2*sizeof(int));
}


/* Bulk encryption of complete blocks in CBC mode.  Caller needs to
   make sure that IV is aligned on an unsigned long boundary.  This
   function is only intended for the bulk encryption feature of
   cipher.c. */
void
_gcry_aes_cbc_enc (void *context, unsigned char *iv,
                   void *outbuf_arg, const void *inbuf_arg,
                   unsigned int nblocks, int cbc_mac)
{
  RIJNDAEL_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  unsigned char *ivp;
  int i;

#ifdef USE_AESNI
  if (ctx->use_aesni)
    aesni_prepare ();
#endif /*USE_AESNI*/

  for ( ;nblocks; nblocks-- )
    {
      for (ivp=iv, i=0; i < BLOCKSIZE; i++ )
        outbuf[i] = inbuf[i] ^ *ivp++;

      if (0)
        ;
#ifdef USE_PADLOCK
      else if (ctx->use_padlock)
        do_padlock (ctx, 0, outbuf, outbuf);
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI
      else if (ctx->use_aesni)
        do_aesni (ctx, 0, outbuf, outbuf);
#endif /*USE_AESNI*/
      else
        do_encrypt (ctx, outbuf, outbuf );

      memcpy (iv, outbuf, BLOCKSIZE);
      inbuf += BLOCKSIZE;
      if (!cbc_mac)
        outbuf += BLOCKSIZE;
    }

#ifdef USE_AESNI
  if (ctx->use_aesni)
    aesni_cleanup ();
#endif /*USE_AESNI*/

  _gcry_burn_stack (48 + 2*sizeof(int));
}


/* Bulk encryption of complete blocks in CTR mode.  Caller needs to
   make sure that CTR is aligned on a 16 byte boundary if AESNI; the
   minimum alignment is for an u32.  This function is only intended
   for the bulk encryption feature of cipher.c.  CTR is expected to be
   of size BLOCKSIZE. */
void
_gcry_aes_ctr_enc (void *context, unsigned char *ctr,
                   void *outbuf_arg, const void *inbuf_arg,
                   unsigned int nblocks)
{
  RIJNDAEL_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  unsigned char *p;
  int i;

  if (0)
    ;
#ifdef USE_AESNI
  else if (ctx->use_aesni)
    {
      aesni_prepare ();
      for ( ;nblocks > 3 ; nblocks -= 4 )
        {
          do_aesni_ctr_4 (ctx, ctr, outbuf, inbuf);
          outbuf += 4*BLOCKSIZE;
          inbuf  += 4*BLOCKSIZE;
        }
      for ( ;nblocks; nblocks-- )
        {
          do_aesni_ctr (ctx, ctr, outbuf, inbuf);
          outbuf += BLOCKSIZE;
          inbuf  += BLOCKSIZE;
        }
      aesni_cleanup ();
      aesni_cleanup_2_4 ();
    }
#endif /*USE_AESNI*/
  else
    {
      union { unsigned char x1[16]; u32 x32[4]; } tmp;

      for ( ;nblocks; nblocks-- )
        {
          /* Encrypt the counter. */
          do_encrypt_aligned (ctx, tmp.x1, ctr);
          /* XOR the input with the encrypted counter and store in output.  */
          for (p=tmp.x1, i=0; i < BLOCKSIZE; i++)
            *outbuf++ = (*p++ ^= *inbuf++);
          /* Increment the counter.  */
          for (i = BLOCKSIZE; i > 0; i--)
            {
              ctr[i-1]++;
              if (ctr[i-1])
                break;
            }
        }
    }

  _gcry_burn_stack (48 + 2*sizeof(int));
}



/* Decrypt one block.  A and B need to be aligned on a 4 byte boundary
   and the decryption must have been prepared.  A and B may be the
   same. */
static void
do_decrypt_aligned (RIJNDAEL_context *ctx,
                    unsigned char *b, const unsigned char *a)
{
#define rk  (ctx->keyschdec)
  int rounds = ctx->rounds;
  int r;
  union
  {
    u32  tempu32[4];  /* Force correct alignment. */
    byte temp[4][4];
  } u;


  *((u32_a_t*)u.temp[0]) = *((u32_a_t*)(a   )) ^ *((u32_a_t*)rk[rounds][0]);
  *((u32_a_t*)u.temp[1]) = *((u32_a_t*)(a+ 4)) ^ *((u32_a_t*)rk[rounds][1]);
  *((u32_a_t*)u.temp[2]) = *((u32_a_t*)(a+ 8)) ^ *((u32_a_t*)rk[rounds][2]);
  *((u32_a_t*)u.temp[3]) = *((u32_a_t*)(a+12)) ^ *((u32_a_t*)rk[rounds][3]);

  *((u32_a_t*)(b   ))    = (*((u32_a_t*)T5[u.temp[0][0]])
                        ^ *((u32_a_t*)T6[u.temp[3][1]])
                        ^ *((u32_a_t*)T7[u.temp[2][2]])
                        ^ *((u32_a_t*)T8[u.temp[1][3]]));
  *((u32_a_t*)(b+ 4))    = (*((u32_a_t*)T5[u.temp[1][0]])
                        ^ *((u32_a_t*)T6[u.temp[0][1]])
                        ^ *((u32_a_t*)T7[u.temp[3][2]])
                        ^ *((u32_a_t*)T8[u.temp[2][3]]));
  *((u32_a_t*)(b+ 8))    = (*((u32_a_t*)T5[u.temp[2][0]])
                        ^ *((u32_a_t*)T6[u.temp[1][1]])
                        ^ *((u32_a_t*)T7[u.temp[0][2]])
                        ^ *((u32_a_t*)T8[u.temp[3][3]]));
  *((u32_a_t*)(b+12))    = (*((u32_a_t*)T5[u.temp[3][0]])
                        ^ *((u32_a_t*)T6[u.temp[2][1]])
                        ^ *((u32_a_t*)T7[u.temp[1][2]])
                        ^ *((u32_a_t*)T8[u.temp[0][3]]));

  for (r = rounds-1; r > 1; r--)
    {
      *((u32_a_t*)u.temp[0]) = *((u32_a_t*)(b   )) ^ *((u32_a_t*)rk[r][0]);
      *((u32_a_t*)u.temp[1]) = *((u32_a_t*)(b+ 4)) ^ *((u32_a_t*)rk[r][1]);
      *((u32_a_t*)u.temp[2]) = *((u32_a_t*)(b+ 8)) ^ *((u32_a_t*)rk[r][2]);
      *((u32_a_t*)u.temp[3]) = *((u32_a_t*)(b+12)) ^ *((u32_a_t*)rk[r][3]);
      *((u32_a_t*)(b   ))    = (*((u32_a_t*)T5[u.temp[0][0]])
                            ^ *((u32_a_t*)T6[u.temp[3][1]])
                            ^ *((u32_a_t*)T7[u.temp[2][2]])
                            ^ *((u32_a_t*)T8[u.temp[1][3]]));
      *((u32_a_t*)(b+ 4))    = (*((u32_a_t*)T5[u.temp[1][0]])
                            ^ *((u32_a_t*)T6[u.temp[0][1]])
                            ^ *((u32_a_t*)T7[u.temp[3][2]])
                            ^ *((u32_a_t*)T8[u.temp[2][3]]));
      *((u32_a_t*)(b+ 8))    = (*((u32_a_t*)T5[u.temp[2][0]])
                            ^ *((u32_a_t*)T6[u.temp[1][1]])
                            ^ *((u32_a_t*)T7[u.temp[0][2]])
                            ^ *((u32_a_t*)T8[u.temp[3][3]]));
      *((u32_a_t*)(b+12))    = (*((u32_a_t*)T5[u.temp[3][0]])
                            ^ *((u32_a_t*)T6[u.temp[2][1]])
                            ^ *((u32_a_t*)T7[u.temp[1][2]])
                            ^ *((u32_a_t*)T8[u.temp[0][3]]));
    }

  /* Last round is special. */
  *((u32_a_t*)u.temp[0]) = *((u32_a_t*)(b   )) ^ *((u32_a_t*)rk[1][0]);
  *((u32_a_t*)u.temp[1]) = *((u32_a_t*)(b+ 4)) ^ *((u32_a_t*)rk[1][1]);
  *((u32_a_t*)u.temp[2]) = *((u32_a_t*)(b+ 8)) ^ *((u32_a_t*)rk[1][2]);
  *((u32_a_t*)u.temp[3]) = *((u32_a_t*)(b+12)) ^ *((u32_a_t*)rk[1][3]);
  b[ 0] = S5[u.temp[0][0]];
  b[ 1] = S5[u.temp[3][1]];
  b[ 2] = S5[u.temp[2][2]];
  b[ 3] = S5[u.temp[1][3]];
  b[ 4] = S5[u.temp[1][0]];
  b[ 5] = S5[u.temp[0][1]];
  b[ 6] = S5[u.temp[3][2]];
  b[ 7] = S5[u.temp[2][3]];
  b[ 8] = S5[u.temp[2][0]];
  b[ 9] = S5[u.temp[1][1]];
  b[10] = S5[u.temp[0][2]];
  b[11] = S5[u.temp[3][3]];
  b[12] = S5[u.temp[3][0]];
  b[13] = S5[u.temp[2][1]];
  b[14] = S5[u.temp[1][2]];
  b[15] = S5[u.temp[0][3]];
  *((u32_a_t*)(b   )) ^= *((u32_a_t*)rk[0][0]);
  *((u32_a_t*)(b+ 4)) ^= *((u32_a_t*)rk[0][1]);
  *((u32_a_t*)(b+ 8)) ^= *((u32_a_t*)rk[0][2]);
  *((u32_a_t*)(b+12)) ^= *((u32_a_t*)rk[0][3]);
#undef rk
}


/* Decrypt one block.  AX and BX may be the same. */
static void
do_decrypt (RIJNDAEL_context *ctx, byte *bx, const byte *ax)
{
  if ( !ctx->decryption_prepared )
    {
      prepare_decryption ( ctx );
      _gcry_burn_stack (64);
      ctx->decryption_prepared = 1;
    }

  /* BX and AX are not necessary correctly aligned.  Thus we might
     need to copy them here.  We try to align to a 16 bytes. */
  if (((size_t)ax & 0x0f) || ((size_t)bx & 0x0f))
    {
      union
      {
        u32  dummy[4];
        byte a[16] ATTR_ALIGNED_16;
      } a;
      union
      {
        u32  dummy[4];
        byte b[16] ATTR_ALIGNED_16;
      } b;

      memcpy (a.a, ax, 16);
      do_decrypt_aligned (ctx, b.b, a.a);
      memcpy (bx, b.b, 16);
    }
  else
    {
      do_decrypt_aligned (ctx, bx, ax);
    }
}




static void
rijndael_decrypt (void *context, byte *b, const byte *a)
{
  RIJNDAEL_context *ctx = context;

  if (0)
    ;
#ifdef USE_PADLOCK
  else if (ctx->use_padlock)
    {
      do_padlock (ctx, 1, b, a);
      _gcry_burn_stack (48 + 2*sizeof(int) /* FIXME */);
    }
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI
  else if (ctx->use_aesni)
    {
      aesni_prepare ();
      do_aesni (ctx, 1, b, a);
      aesni_cleanup ();
    }
#endif /*USE_AESNI*/
  else
    {
      do_decrypt (ctx, b, a);
      _gcry_burn_stack (56+2*sizeof(int));
    }
}


/* Bulk decryption of complete blocks in CFB mode.  Caller needs to
   make sure that IV is aligned on an unisgned lonhg boundary.  This
   function is only intended for the bulk encryption feature of
   cipher.c. */
void
_gcry_aes_cfb_dec (void *context, unsigned char *iv,
                   void *outbuf_arg, const void *inbuf_arg,
                   unsigned int nblocks)
{
  RIJNDAEL_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  unsigned char *ivp;
  unsigned char temp;
  int i;

  if (0)
    ;
#ifdef USE_PADLOCK
  else if (ctx->use_padlock)
    {
      /* Fixme:  Let Padlock do the CFBing.  */
      for ( ;nblocks; nblocks-- )
        {
          do_padlock (ctx, 0, iv, iv);
          for (ivp=iv,i=0; i < BLOCKSIZE; i++ )
            {
              temp = *inbuf++;
              *outbuf++ = *ivp ^ temp;
              *ivp++ = temp;
            }
        }
    }
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI
  else if (ctx->use_aesni)
    {
      aesni_prepare ();
      for ( ;nblocks; nblocks-- )
        {
          do_aesni_cfb (ctx, 1, iv, outbuf, inbuf);
          outbuf += BLOCKSIZE;
          inbuf  += BLOCKSIZE;
        }
      aesni_cleanup ();
    }
#endif /*USE_AESNI*/
  else
    {
      for ( ;nblocks; nblocks-- )
        {
          do_encrypt_aligned (ctx, iv, iv);
          for (ivp=iv,i=0; i < BLOCKSIZE; i++ )
            {
              temp = *inbuf++;
              *outbuf++ = *ivp ^ temp;
              *ivp++ = temp;
            }
        }
    }

  _gcry_burn_stack (48 + 2*sizeof(int));
}


/* Bulk decryption of complete blocks in CBC mode.  Caller needs to
   make sure that IV is aligned on an unsigned long boundary.  This
   function is only intended for the bulk encryption feature of
   cipher.c. */
void
_gcry_aes_cbc_dec (void *context, unsigned char *iv,
                   void *outbuf_arg, const void *inbuf_arg,
                   unsigned int nblocks)
{
  RIJNDAEL_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  unsigned char *ivp;
  int i;
  unsigned char savebuf[BLOCKSIZE];

#ifdef USE_AESNI
  if (ctx->use_aesni)
    aesni_prepare ();
#endif /*USE_AESNI*/

  for ( ;nblocks; nblocks-- )
    {
      /* We need to save INBUF away because it may be identical to
         OUTBUF.  */
      memcpy (savebuf, inbuf, BLOCKSIZE);

      if (0)
        ;
#ifdef USE_PADLOCK
      else if (ctx->use_padlock)
        do_padlock (ctx, 1, outbuf, inbuf);
#endif /*USE_PADLOCK*/
#ifdef USE_AESNI
      else if (ctx->use_aesni)
        do_aesni (ctx, 1, outbuf, inbuf);
#endif /*USE_AESNI*/
      else
        do_decrypt (ctx, outbuf, inbuf);

      for (ivp=iv, i=0; i < BLOCKSIZE; i++ )
        outbuf[i] ^= *ivp++;
      memcpy (iv, savebuf, BLOCKSIZE);
      inbuf += BLOCKSIZE;
      outbuf += BLOCKSIZE;
    }

#ifdef USE_AESNI
  if (ctx->use_aesni)
    aesni_cleanup ();
#endif /*USE_AESNI*/

  _gcry_burn_stack (48 + 2*sizeof(int) + BLOCKSIZE + 4*sizeof (char*));
}




/* Run the self-tests for AES 128.  Returns NULL on success. */
static const char*
selftest_basic_128 (void)
{
  RIJNDAEL_context ctx;
  unsigned char scratch[16];

  /* The test vectors are from the AES supplied ones; more or less
     randomly taken from ecb_tbl.txt (I=42,81,14) */
#if 1
  static const unsigned char plaintext_128[16] =
    {
      0x01,0x4B,0xAF,0x22,0x78,0xA6,0x9D,0x33,
      0x1D,0x51,0x80,0x10,0x36,0x43,0xE9,0x9A
    };
  static const unsigned char key_128[16] =
    {
      0xE8,0xE9,0xEA,0xEB,0xED,0xEE,0xEF,0xF0,
      0xF2,0xF3,0xF4,0xF5,0xF7,0xF8,0xF9,0xFA
    };
  static const unsigned char ciphertext_128[16] =
    {
      0x67,0x43,0xC3,0xD1,0x51,0x9A,0xB4,0xF2,
      0xCD,0x9A,0x78,0xAB,0x09,0xA5,0x11,0xBD
    };
#else
  /* Test vectors from fips-197, appendix C. */
# warning debug test vectors in use
  static const unsigned char plaintext_128[16] =
    {
      0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
      0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
    };
  static const unsigned char key_128[16] =
    {
      0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
      0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
      /* 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, */
      /* 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c */
    };
  static const unsigned char ciphertext_128[16] =
    {
      0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
      0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a
    };
#endif

  rijndael_setkey (&ctx, key_128, sizeof (key_128));
  rijndael_encrypt (&ctx, scratch, plaintext_128);
  if (memcmp (scratch, ciphertext_128, sizeof (ciphertext_128)))
     return "AES-128 test encryption failed.";
  rijndael_decrypt (&ctx, scratch, scratch);
  if (memcmp (scratch, plaintext_128, sizeof (plaintext_128)))
    return "AES-128 test decryption failed.";

  return NULL;
}

/* Run the self-tests for AES 192.  Returns NULL on success. */
static const char*
selftest_basic_192 (void)
{
  RIJNDAEL_context ctx;
  unsigned char scratch[16];

  static unsigned char plaintext_192[16] =
    {
      0x76,0x77,0x74,0x75,0xF1,0xF2,0xF3,0xF4,
      0xF8,0xF9,0xE6,0xE7,0x77,0x70,0x71,0x72
    };
  static unsigned char key_192[24] =
    {
      0x04,0x05,0x06,0x07,0x09,0x0A,0x0B,0x0C,
      0x0E,0x0F,0x10,0x11,0x13,0x14,0x15,0x16,
      0x18,0x19,0x1A,0x1B,0x1D,0x1E,0x1F,0x20
    };
  static const unsigned char ciphertext_192[16] =
    {
      0x5D,0x1E,0xF2,0x0D,0xCE,0xD6,0xBC,0xBC,
      0x12,0x13,0x1A,0xC7,0xC5,0x47,0x88,0xAA
    };

  rijndael_setkey (&ctx, key_192, sizeof(key_192));
  rijndael_encrypt (&ctx, scratch, plaintext_192);
  if (memcmp (scratch, ciphertext_192, sizeof (ciphertext_192)))
    return "AES-192 test encryption failed.";
  rijndael_decrypt (&ctx, scratch, scratch);
  if (memcmp (scratch, plaintext_192, sizeof (plaintext_192)))
    return "AES-192 test decryption failed.";

  return NULL;
}


/* Run the self-tests for AES 256.  Returns NULL on success. */
static const char*
selftest_basic_256 (void)
{
  RIJNDAEL_context ctx;
  unsigned char scratch[16];

  static unsigned char plaintext_256[16] =
    {
      0x06,0x9A,0x00,0x7F,0xC7,0x6A,0x45,0x9F,
      0x98,0xBA,0xF9,0x17,0xFE,0xDF,0x95,0x21
    };
  static unsigned char key_256[32] =
    {
      0x08,0x09,0x0A,0x0B,0x0D,0x0E,0x0F,0x10,
      0x12,0x13,0x14,0x15,0x17,0x18,0x19,0x1A,
      0x1C,0x1D,0x1E,0x1F,0x21,0x22,0x23,0x24,
      0x26,0x27,0x28,0x29,0x2B,0x2C,0x2D,0x2E
    };
  static const unsigned char ciphertext_256[16] =
    {
      0x08,0x0E,0x95,0x17,0xEB,0x16,0x77,0x71,
      0x9A,0xCF,0x72,0x80,0x86,0x04,0x0A,0xE3
    };

  rijndael_setkey (&ctx, key_256, sizeof(key_256));
  rijndael_encrypt (&ctx, scratch, plaintext_256);
  if (memcmp (scratch, ciphertext_256, sizeof (ciphertext_256)))
    return "AES-256 test encryption failed.";
  rijndael_decrypt (&ctx, scratch, scratch);
  if (memcmp (scratch, plaintext_256, sizeof (plaintext_256)))
    return "AES-256 test decryption failed.";

  return NULL;
}

/* Run all the self-tests and return NULL on success.  This function
   is used for the on-the-fly self-tests. */
static const char *
selftest (void)
{
  const char *r;

  if ( (r = selftest_basic_128 ())
       || (r = selftest_basic_192 ())
       || (r = selftest_basic_256 ()) )
    return r;

  return r;
}


/* SP800-38a.pdf for AES-128.  */
static const char *
selftest_fips_128_38a (int requested_mode)
{
  struct tv
  {
    int mode;
    const unsigned char key[16];
    const unsigned char iv[16];
    struct
    {
      const unsigned char input[16];
      const unsigned char output[16];
    } data[4];
  } tv[2] =
    {
      {
        GCRY_CIPHER_MODE_CFB,  /* F.3.13, CFB128-AES128 */
        { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
          0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
          0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
        {
          { { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
              0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a },
            { 0x3b, 0x3f, 0xd9, 0x2e, 0xb7, 0x2d, 0xad, 0x20,
              0x33, 0x34, 0x49, 0xf8, 0xe8, 0x3c, 0xfb, 0x4a } },

          { { 0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
              0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51 },
            { 0xc8, 0xa6, 0x45, 0x37, 0xa0, 0xb3, 0xa9, 0x3f,
              0xcd, 0xe3, 0xcd, 0xad, 0x9f, 0x1c, 0xe5, 0x8b } },

          { { 0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
              0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef },
            { 0x26, 0x75, 0x1f, 0x67, 0xa3, 0xcb, 0xb1, 0x40,
              0xb1, 0x80, 0x8c, 0xf1, 0x87, 0xa4, 0xf4, 0xdf } },

          { { 0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
              0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 },
            { 0xc0, 0x4b, 0x05, 0x35, 0x7c, 0x5d, 0x1c, 0x0e,
              0xea, 0xc4, 0xc6, 0x6f, 0x9f, 0xf7, 0xf2, 0xe6 } }
        }
      },
      {
        GCRY_CIPHER_MODE_OFB,
        { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
          0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
          0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f },
        {
          { { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
              0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a },
            { 0x3b, 0x3f, 0xd9, 0x2e, 0xb7, 0x2d, 0xad, 0x20,
              0x33, 0x34, 0x49, 0xf8, 0xe8, 0x3c, 0xfb, 0x4a } },

          { { 0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
              0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51 },
            { 0x77, 0x89, 0x50, 0x8d, 0x16, 0x91, 0x8f, 0x03,
              0xf5, 0x3c, 0x52, 0xda, 0xc5, 0x4e, 0xd8, 0x25 } },

          { { 0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
              0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef },
            { 0x97, 0x40, 0x05, 0x1e, 0x9c, 0x5f, 0xec, 0xf6,
              0x43, 0x44, 0xf7, 0xa8, 0x22, 0x60, 0xed, 0xcc } },

          { { 0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
              0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 },
            { 0x30, 0x4c, 0x65, 0x28, 0xf6, 0x59, 0xc7, 0x78,
              0x66, 0xa5, 0x10, 0xd9, 0xc1, 0xd6, 0xae, 0x5e } },
        }
      }
    };
  unsigned char scratch[16];
  gpg_error_t err;
  int tvi, idx;
  gcry_cipher_hd_t hdenc = NULL;
  gcry_cipher_hd_t hddec = NULL;

#define Fail(a) do {           \
    _gcry_cipher_close (hdenc);  \
    _gcry_cipher_close (hddec);  \
    return a;                    \
  } while (0)

  gcry_assert (sizeof tv[0].data[0].input == sizeof scratch);
  gcry_assert (sizeof tv[0].data[0].output == sizeof scratch);

  for (tvi=0; tvi < DIM (tv); tvi++)
    if (tv[tvi].mode == requested_mode)
      break;
  if (tvi == DIM (tv))
    Fail ("no test data for this mode");

  err = _gcry_cipher_open (&hdenc, GCRY_CIPHER_AES, tv[tvi].mode, 0);
  if (err)
    Fail ("open");
  err = _gcry_cipher_open (&hddec, GCRY_CIPHER_AES, tv[tvi].mode, 0);
  if (err)
    Fail ("open");
  err = _gcry_cipher_setkey (hdenc, tv[tvi].key,  sizeof tv[tvi].key);
  if (!err)
    err = _gcry_cipher_setkey (hddec, tv[tvi].key, sizeof tv[tvi].key);
  if (err)
    Fail ("set key");
  err = _gcry_cipher_setiv (hdenc, tv[tvi].iv, sizeof tv[tvi].iv);
  if (!err)
    err = _gcry_cipher_setiv (hddec, tv[tvi].iv, sizeof tv[tvi].iv);
  if (err)
    Fail ("set IV");
  for (idx=0; idx < DIM (tv[tvi].data); idx++)
    {
      err = _gcry_cipher_encrypt (hdenc, scratch, sizeof scratch,
                                  tv[tvi].data[idx].input,
                                  sizeof tv[tvi].data[idx].input);
      if (err)
        Fail ("encrypt command");
      if (memcmp (scratch, tv[tvi].data[idx].output, sizeof scratch))
        Fail ("encrypt mismatch");
      err = _gcry_cipher_decrypt (hddec, scratch, sizeof scratch,
                                  tv[tvi].data[idx].output,
                                  sizeof tv[tvi].data[idx].output);
      if (err)
        Fail ("decrypt command");
      if (memcmp (scratch, tv[tvi].data[idx].input, sizeof scratch))
        Fail ("decrypt mismatch");
    }

#undef Fail
  _gcry_cipher_close (hdenc);
  _gcry_cipher_close (hddec);
  return NULL;
}


/* Complete selftest for AES-128 with all modes and driver code.  */
static gpg_err_code_t
selftest_fips_128 (int extended, selftest_report_func_t report)
{
  const char *what;
  const char *errtxt;

  what = "low-level";
  errtxt = selftest_basic_128 ();
  if (errtxt)
    goto failed;

  if (extended)
    {
      what = "cfb";
      errtxt = selftest_fips_128_38a (GCRY_CIPHER_MODE_CFB);
      if (errtxt)
        goto failed;

      what = "ofb";
      errtxt = selftest_fips_128_38a (GCRY_CIPHER_MODE_OFB);
      if (errtxt)
        goto failed;
    }

  return 0; /* Succeeded. */

 failed:
  if (report)
    report ("cipher", GCRY_CIPHER_AES128, what, errtxt);
  return GPG_ERR_SELFTEST_FAILED;
}

/* Complete selftest for AES-192.  */
static gpg_err_code_t
selftest_fips_192 (int extended, selftest_report_func_t report)
{
  const char *what;
  const char *errtxt;

  (void)extended; /* No extended tests available.  */

  what = "low-level";
  errtxt = selftest_basic_192 ();
  if (errtxt)
    goto failed;


  return 0; /* Succeeded. */

 failed:
  if (report)
    report ("cipher", GCRY_CIPHER_AES192, what, errtxt);
  return GPG_ERR_SELFTEST_FAILED;
}


/* Complete selftest for AES-256.  */
static gpg_err_code_t
selftest_fips_256 (int extended, selftest_report_func_t report)
{
  const char *what;
  const char *errtxt;

  (void)extended; /* No extended tests available.  */

  what = "low-level";
  errtxt = selftest_basic_256 ();
  if (errtxt)
    goto failed;

  return 0; /* Succeeded. */

 failed:
  if (report)
    report ("cipher", GCRY_CIPHER_AES256, what, errtxt);
  return GPG_ERR_SELFTEST_FAILED;
}



/* Run a full self-test for ALGO and return 0 on success.  */
static gpg_err_code_t
run_selftests (int algo, int extended, selftest_report_func_t report)
{
  gpg_err_code_t ec;

  switch (algo)
    {
    case GCRY_CIPHER_AES128:
      ec = selftest_fips_128 (extended, report);
      break;
    case GCRY_CIPHER_AES192:
      ec = selftest_fips_192 (extended, report);
      break;
    case GCRY_CIPHER_AES256:
      ec = selftest_fips_256 (extended, report);
      break;
    default:
      ec = GPG_ERR_CIPHER_ALGO;
      break;

    }
  return ec;
}




static const char *rijndael_names[] =
  {
    "RIJNDAEL",
    "AES128",
    "AES-128",
    NULL
  };

static gcry_cipher_oid_spec_t rijndael_oids[] =
  {
    { "2.16.840.1.101.3.4.1.1", GCRY_CIPHER_MODE_ECB },
    { "2.16.840.1.101.3.4.1.2", GCRY_CIPHER_MODE_CBC },
    { "2.16.840.1.101.3.4.1.3", GCRY_CIPHER_MODE_OFB },
    { "2.16.840.1.101.3.4.1.4", GCRY_CIPHER_MODE_CFB },
    { NULL }
  };

gcry_cipher_spec_t _gcry_cipher_spec_aes =
  {
    "AES", rijndael_names, rijndael_oids, 16, 128, sizeof (RIJNDAEL_context),
    rijndael_setkey, rijndael_encrypt, rijndael_decrypt
  };
cipher_extra_spec_t _gcry_cipher_extraspec_aes =
  {
    run_selftests
  };

static const char *rijndael192_names[] =
  {
    "RIJNDAEL192",
    "AES-192",
    NULL
  };

static gcry_cipher_oid_spec_t rijndael192_oids[] =
  {
    { "2.16.840.1.101.3.4.1.21", GCRY_CIPHER_MODE_ECB },
    { "2.16.840.1.101.3.4.1.22", GCRY_CIPHER_MODE_CBC },
    { "2.16.840.1.101.3.4.1.23", GCRY_CIPHER_MODE_OFB },
    { "2.16.840.1.101.3.4.1.24", GCRY_CIPHER_MODE_CFB },
    { NULL }
  };

gcry_cipher_spec_t _gcry_cipher_spec_aes192 =
  {
    "AES192", rijndael192_names, rijndael192_oids, 16, 192, sizeof (RIJNDAEL_context),
    rijndael_setkey, rijndael_encrypt, rijndael_decrypt
  };
cipher_extra_spec_t _gcry_cipher_extraspec_aes192 =
  {
    run_selftests
  };

static const char *rijndael256_names[] =
  {
    "RIJNDAEL256",
    "AES-256",
    NULL
  };

static gcry_cipher_oid_spec_t rijndael256_oids[] =
  {
    { "2.16.840.1.101.3.4.1.41", GCRY_CIPHER_MODE_ECB },
    { "2.16.840.1.101.3.4.1.42", GCRY_CIPHER_MODE_CBC },
    { "2.16.840.1.101.3.4.1.43", GCRY_CIPHER_MODE_OFB },
    { "2.16.840.1.101.3.4.1.44", GCRY_CIPHER_MODE_CFB },
    { NULL }
  };

gcry_cipher_spec_t _gcry_cipher_spec_aes256 =
  {
    "AES256", rijndael256_names, rijndael256_oids, 16, 256,
    sizeof (RIJNDAEL_context),
    rijndael_setkey, rijndael_encrypt, rijndael_decrypt
  };

cipher_extra_spec_t _gcry_cipher_extraspec_aes256 =
  {
    run_selftests
  };
