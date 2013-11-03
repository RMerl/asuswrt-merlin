/* kdf.c  - Key Derivation Functions
 * Copyright (C) 1998, 2011 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
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
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "cipher.h"
#include "ath.h"


/* Transform a passphrase into a suitable key of length KEYSIZE and
   store this key in the caller provided buffer KEYBUFFER.  The caller
   must provide an HASHALGO, a valid ALGO and depending on that algo a
   SALT of 8 bytes and the number of ITERATIONS.  Code taken from
   gnupg/agent/protect.c:hash_passphrase.  */
gpg_err_code_t
openpgp_s2k (const void *passphrase, size_t passphraselen,
             int algo, int hashalgo,
             const void *salt, size_t saltlen,
             unsigned long iterations,
             size_t keysize, void *keybuffer)
{
  gpg_err_code_t ec;
  gcry_md_hd_t md;
  char *key = keybuffer;
  int pass, i;
  int used = 0;
  int secmode;

  if ((algo == GCRY_KDF_SALTED_S2K || algo == GCRY_KDF_ITERSALTED_S2K)
      && (!salt || saltlen != 8))
    return GPG_ERR_INV_VALUE;

  secmode = gcry_is_secure (passphrase) || gcry_is_secure (keybuffer);

  ec = gpg_err_code (gcry_md_open (&md, hashalgo,
                                   secmode? GCRY_MD_FLAG_SECURE : 0));
  if (ec)
    return ec;

  for (pass=0; used < keysize; pass++)
    {
      if (pass)
        {
          gcry_md_reset (md);
          for (i=0; i < pass; i++) /* Preset the hash context.  */
            gcry_md_putc (md, 0);
	}

      if (algo == GCRY_KDF_SALTED_S2K || algo == GCRY_KDF_ITERSALTED_S2K)
        {
          int len2 = passphraselen + 8;
          unsigned long count = len2;

          if (algo == GCRY_KDF_ITERSALTED_S2K)
            {
              count = iterations;
              if (count < len2)
                count = len2;
            }

          while (count > len2)
            {
              gcry_md_write (md, salt, saltlen);
              gcry_md_write (md, passphrase, passphraselen);
              count -= len2;
            }
          if (count < saltlen)
            gcry_md_write (md, salt, count);
          else
            {
              gcry_md_write (md, salt, saltlen);
              count -= saltlen;
              gcry_md_write (md, passphrase, count);
            }
        }
      else
        gcry_md_write (md, passphrase, passphraselen);

      gcry_md_final (md);
      i = gcry_md_get_algo_dlen (hashalgo);
      if (i > keysize - used)
        i = keysize - used;
      memcpy (key+used, gcry_md_read (md, hashalgo), i);
      used += i;
    }
  gcry_md_close (md);
  return 0;
}


/* Transform a passphrase into a suitable key of length KEYSIZE and
   store this key in the caller provided buffer KEYBUFFER.  The caller
   must provide PRFALGO which indicates the pseudorandom function to
   use: This shall be the algorithms id of a hash algorithm; it is
   used in HMAC mode.  SALT is a salt of length SALTLEN and ITERATIONS
   gives the number of iterations.  */
gpg_err_code_t
pkdf2 (const void *passphrase, size_t passphraselen,
       int hashalgo,
       const void *salt, size_t saltlen,
       unsigned long iterations,
       size_t keysize, void *keybuffer)
{
  gpg_err_code_t ec;
  gcry_md_hd_t md;
  int secmode;
  unsigned int dklen = keysize;
  char *dk = keybuffer;
  unsigned int hlen;   /* Output length of the digest function.  */
  unsigned int l;      /* Rounded up number of blocks.  */
  unsigned int r;      /* Number of octets in the last block.  */
  char *sbuf;          /* Malloced buffer to concatenate salt and iter
                          as well as space to hold TBUF and UBUF.  */
  char *tbuf;          /* Buffer for T; ptr into SBUF, size is HLEN. */
  char *ubuf;          /* Buffer for U; ptr into SBUF, size is HLEN. */
  unsigned int lidx;   /* Current block number.  */
  unsigned long iter;  /* Current iteration number.  */
  unsigned int i;

  if (!salt || !saltlen || !iterations || !dklen)
    return GPG_ERR_INV_VALUE;

  hlen = gcry_md_get_algo_dlen (hashalgo);
  if (!hlen)
    return GPG_ERR_DIGEST_ALGO;

  secmode = gcry_is_secure (passphrase) || gcry_is_secure (keybuffer);

  /* We ignore step 1 from pksc5v2.1 which demands a check that dklen
     is not larger that 0xffffffff * hlen.  */

  /* Step 2 */
  l = ((dklen - 1)/ hlen) + 1;
  r = dklen - (l - 1) * hlen;

  /* Setup buffers and prepare a hash context.  */
  sbuf = (secmode
          ? gcry_malloc_secure (saltlen + 4 + hlen + hlen)
          : gcry_malloc (saltlen + 4 + hlen + hlen));
  if (!sbuf)
    return gpg_err_code_from_syserror ();
  tbuf = sbuf + saltlen + 4;
  ubuf = tbuf + hlen;

  ec = gpg_err_code (gcry_md_open (&md, hashalgo,
                                   (GCRY_MD_FLAG_HMAC
                                    | (secmode?GCRY_MD_FLAG_SECURE:0))));
  if (ec)
    {
      gcry_free (sbuf);
      return ec;
    }

  /* Step 3 and 4. */
  memcpy (sbuf, salt, saltlen);
  for (lidx = 1; lidx <= l; lidx++)
    {
      for (iter = 0; iter < iterations; iter++)
        {
          ec = gpg_err_code (gcry_md_setkey (md, passphrase, passphraselen));
          if (ec)
            {
              gcry_md_close (md);
              gcry_free (sbuf);
              return ec;
            }
          if (!iter) /* Compute U_1:  */
            {
              sbuf[saltlen]     = (lidx >> 24);
              sbuf[saltlen + 1] = (lidx >> 16);
              sbuf[saltlen + 2] = (lidx >> 8);
              sbuf[saltlen + 3] = lidx;
              gcry_md_write (md, sbuf, saltlen + 4);
              memcpy (ubuf, gcry_md_read (md, 0), hlen);
              memcpy (tbuf, ubuf, hlen);
            }
          else /* Compute U_(2..c):  */
            {
              gcry_md_write (md, ubuf, hlen);
              memcpy (ubuf, gcry_md_read (md, 0), hlen);
              for (i=0; i < hlen; i++)
                tbuf[i] ^= ubuf[i];
            }
        }
      if (lidx == l)  /* Last block.  */
        memcpy (dk, tbuf, r);
      else
        {
          memcpy (dk, tbuf, hlen);
          dk += hlen;
        }
    }

  gcry_md_close (md);
  gcry_free (sbuf);
  return 0;
}


/* Derive a key from a passphrase.  KEYSIZE gives the requested size
   of the keys in octets.  KEYBUFFER is a caller provided buffer
   filled on success with the derived key.  The input passphrase is
   taken from (PASSPHRASE,PASSPHRASELEN) which is an arbitrary memory
   buffer.  ALGO specifies the KDF algorithm to use; these are the
   constants GCRY_KDF_*.  SUBALGO specifies an algorithm used
   internally by the KDF algorithms; this is usually a hash algorithm
   but certain KDF algorithm may use it differently.  {SALT,SALTLEN}
   is a salt as needed by most KDF algorithms.  ITERATIONS is a
   positive integer parameter to most KDFs.  0 is returned on success,
   or an error code on failure.  */
gpg_error_t
gcry_kdf_derive (const void *passphrase, size_t passphraselen,
                 int algo, int subalgo,
                 const void *salt, size_t saltlen,
                 unsigned long iterations,
                 size_t keysize, void *keybuffer)
{
  gpg_err_code_t ec;

  if (!passphrase || (!passphraselen && algo != GCRY_KDF_PBKDF2))
    {
      ec = GPG_ERR_INV_DATA;
      goto leave;
    }
  if (!keybuffer || !keysize)
    {
      ec = GPG_ERR_INV_VALUE;
      goto leave;
    }


  switch (algo)
    {
    case GCRY_KDF_SIMPLE_S2K:
    case GCRY_KDF_SALTED_S2K:
    case GCRY_KDF_ITERSALTED_S2K:
      ec = openpgp_s2k (passphrase, passphraselen, algo, subalgo,
                        salt, saltlen, iterations, keysize, keybuffer);
      break;

    case GCRY_KDF_PBKDF1:
      ec = GPG_ERR_UNSUPPORTED_ALGORITHM;
      break;

    case GCRY_KDF_PBKDF2:
      ec = pkdf2 (passphrase, passphraselen, subalgo,
                  salt, saltlen, iterations, keysize, keybuffer);
      break;

    default:
      ec = GPG_ERR_UNKNOWN_ALGORITHM;
      break;
    }

 leave:
  return gpg_error (ec);
}
