/* t-kdf.c -  KDF regression tests
 * Copyright (C) 2011 Free Software Foundation, Inc.
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "../src/gcrypt.h"

#ifndef DIM
# define DIM(v)		     (sizeof(v)/sizeof((v)[0]))
#endif

/* Program option flags.  */
static int verbose;
static int error_count;

static void
fail (const char *format, ...)
{
  va_list arg_ptr;

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  error_count++;
}

static void
die (const char *format, ...)
{
  va_list arg_ptr;

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  exit (1);
}


static void
check_openpgp (void)
{
  /* Test vectors manually created with gpg 1.4 derived code: In
     passphrase.c:hash_passpharse, add this code to the end of the
     function:

       ===8<===
       printf ("{\n"
               "  \"");
       for (i=0; i < pwlen; i++)
         {
           if (i && !(i%16))
             printf ("\"\n  \"");
           printf ("\\x%02x", ((const unsigned char *)pw)[i]);
         }
       printf ("\", %d,\n", pwlen);

       printf ("  %s, %s,\n",
               s2k->mode == 0? "GCRY_KDF_SIMPLE_S2K":
               s2k->mode == 1? "GCRY_KDF_SALTED_S2K":
               s2k->mode == 3? "GCRY_KDF_ITERSALTED_S2K":"?",
               s2k->hash_algo == DIGEST_ALGO_MD5   ? "GCRY_MD_MD5" :
               s2k->hash_algo == DIGEST_ALGO_SHA1  ? "GCRY_MD_SHA1" :
               s2k->hash_algo == DIGEST_ALGO_RMD160? "GCRY_MD_RMD160" :
               s2k->hash_algo == DIGEST_ALGO_SHA256? "GCRY_MD_SHA256" :
               s2k->hash_algo == DIGEST_ALGO_SHA384? "GCRY_MD_SHA384" :
               s2k->hash_algo == DIGEST_ALGO_SHA512? "GCRY_MD_SHA512" :
               s2k->hash_algo == DIGEST_ALGO_SHA224? "GCRY_MD_SHA224" : "?");

       if (s2k->mode == 0)
         printf ("  NULL, 0,\n");
       else
         {
           printf ("  \"");
           for (i=0; i < 8; i++)
             printf ("\\x%02x", (unsigned int)s2k->salt[i]);
           printf ("\", %d,\n", 8);
         }

       if (s2k->mode == 3)
         printf ("  %lu,\n", (unsigned long)S2K_DECODE_COUNT(s2k->count));
       else
         printf ("  0,\n");

       printf ("  %d,\n", (int)dek->keylen);

       printf ("  \"");
       for (i=0; i < dek->keylen; i++)
         {
           if (i && !(i%16))
             printf ("\"\n  \"");
           printf ("\\x%02x", ((unsigned char *)dek->key)[i]);
         }
       printf ("\"\n},\n");
       ===>8===

     Then prepare a file x.inp with utf8 encoding:

       ===8<===
       0 aes    md5 1024 a
       0 aes    md5 1024 ab
       0 aes    md5 1024 abc
       0 aes    md5 1024 abcd
       0 aes    md5 1024 abcde
       0 aes    md5 1024 abcdef
       0 aes    md5 1024 abcdefg
       0 aes    md5 1024 abcdefgh
       0 aes    md5 1024 abcdefghi
       0 aes    md5 1024 abcdefghijklmno
       0 aes    md5 1024 abcdefghijklmnop
       0 aes    md5 1024 abcdefghijklmnopq
       0 aes    md5 1024 Long_sentence_used_as_passphrase
       0 aes    md5 1024 With_utf8_umlauts:äüÖß
       0 aes    sha1 1024 a
       0 aes    sha1 1024 ab
       0 aes    sha1 1024 abc
       0 aes    sha1 1024 abcd
       0 aes    sha1 1024 abcde
       0 aes    sha1 1024 abcdef
       0 aes    sha1 1024 abcdefg
       0 aes    sha1 1024 abcdefgh
       0 aes    sha1 1024 abcdefghi
       0 aes    sha1 1024 abcdefghijklmno
       0 aes    sha1 1024 abcdefghijklmnop
       0 aes    sha1 1024 abcdefghijklmnopq
       0 aes    sha1 1024 abcdefghijklmnopqr
       0 aes    sha1 1024 abcdefghijklmnopqrs
       0 aes    sha1 1024 abcdefghijklmnopqrst
       0 aes    sha1 1024 abcdefghijklmnopqrstu
       0 aes    sha1 1024 Long_sentence_used_as_passphrase
       0 aes256 sha1 1024 Long_sentence_used_as_passphrase
       0 aes    sha1 1024 With_utf8_umlauts:äüÖß
       3 aes    sha1 1024 a
       3 aes    sha1 1024 ab
       3 aes    sha1 1024 abc
       3 aes    sha1 1024 abcd
       3 aes    sha1 1024 abcde
       3 aes    sha1 1024 abcdef
       3 aes    sha1 1024 abcdefg
       3 aes    sha1 1024 abcdefgh
       3 aes    sha1 1024 abcdefghi
       3 aes    sha1 1024 abcdefghijklmno
       3 aes    sha1 1024 abcdefghijklmnop
       3 aes    sha1 1024 abcdefghijklmnopq
       3 aes    sha1 1024 abcdefghijklmnopqr
       3 aes    sha1 1024 abcdefghijklmnopqrs
       3 aes    sha1 1024 abcdefghijklmnopqrst
       3 aes    sha1 1024 abcdefghijklmnopqrstu
       3 aes    sha1 1024 With_utf8_umlauts:äüÖß
       3 aes    sha1 1024 Long_sentence_used_as_passphrase
       3 aes    sha1 10240 Long_sentence_used_as_passphrase
       3 aes    sha1 102400 Long_sentence_used_as_passphrase
       3 aes192 sha1 1024 a
       3 aes192 sha1 1024 abcdefg
       3 aes192 sha1 1024 abcdefghi
       3 aes192 sha1 1024 abcdefghi
       3 aes192 sha1 1024 Long_sentence_used_as_passphrase
       3 aes256 sha1 1024 a
       3 aes256 sha1 1024 abcdefg
       3 aes256 sha1 1024 abcdefghi
       3 aes256 sha1 1024 abcdefghi
       3 aes256 sha1 1024 Long_sentence_used_as_passphrase
       0 aes    sha256 1024 Long_sentence_used_as_passphrase
       1 aes    sha256 1024 Long_sentence_used_as_passphrase
       3 aes    sha256 1024 Long_sentence_used_as_passphrase
       3 aes    sha256 10240 Long_sentence_used_as_passphrase
       3 aes    sha384 1024 Long_sentence_used_as_passphrase
       3 aes    sha512 1024 Long_sentence_used_as_passphrase
       3 aes256 sha512 1024 Long_sentence_used_as_passphrase
       3 3des   sha512 1024 Long_sentence_used_as_passphrase
       ===>8===

    and finally using a proper utf-8 enabled shell, run:

       cat x.inp | while read mode cipher digest count pass dummy; do \
         ./gpg </dev/null -o /dev/null -c  --passphrase "$pass" \
           --s2k-mode $mode --s2k-digest $digest --s2k-count $count \
           --cipher-algo $cipher ; done >x.out
  */
  static struct {
    const char *p;   /* Passphrase.  */
    size_t plen;     /* Length of P. */
    int algo;
    int hashalgo;
    const char *salt;
    size_t saltlen;
    unsigned long c; /* Iterations.  */
    int dklen;       /* Requested key length.  */
    const char *dk;  /* Derived key.  */
    int disabled;
  } tv[] = {
    {
      "\x61", 1,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8\x31\xc3\x99\xe2\x69\x77\x26\x61"
    },
    {
      "\x61\x62", 2,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x18\x7e\xf4\x43\x61\x22\xd1\xcc\x2f\x40\xdc\x2b\x92\xf0\xeb\xa0"
    },
    {
      "\x61\x62\x63", 3,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f\x72"
    },
    {
      "\x61\x62\x63\x64", 4,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\xe2\xfc\x71\x4c\x47\x27\xee\x93\x95\xf3\x24\xcd\x2e\x7f\x33\x1f"
    },
    {
      "\x61\x62\x63\x64\x65", 5,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\xab\x56\xb4\xd9\x2b\x40\x71\x3a\xcc\x5a\xf8\x99\x85\xd4\xb7\x86"
    },
    {
      "\x61\x62\x63\x64\x65\x66", 6,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\xe8\x0b\x50\x17\x09\x89\x50\xfc\x58\xaa\xd8\x3c\x8c\x14\x97\x8e"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67", 7,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x7a\xc6\x6c\x0f\x14\x8d\xe9\x51\x9b\x8b\xd2\x64\x31\x2c\x4d\x64"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68", 8,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\xe8\xdc\x40\x81\xb1\x34\x34\xb4\x51\x89\xa7\x20\xb7\x7b\x68\x18"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69", 9,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x8a\xa9\x9b\x1f\x43\x9f\xf7\x12\x93\xe9\x53\x57\xba\xc6\xfd\x94"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f", 15,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x8a\x73\x19\xdb\xf6\x54\x4a\x74\x22\xc9\xe2\x54\x52\x58\x0e\xa5"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70", 16,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x1d\x64\xdc\xe2\x39\xc4\x43\x7b\x77\x36\x04\x1d\xb0\x89\xe1\xb9"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71", 17,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x9a\x8d\x98\x45\xa6\xb4\xd8\x2d\xfc\xb2\xc2\xe3\x51\x62\xc8\x30"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x35\x2a\xf0\xfc\xdf\xe9\xbb\x62\x16\xfc\x99\x9d\x8d\x58\x05\xcb"
    },
    {
      "\x57\x69\x74\x68\x5f\x75\x74\x66\x38\x5f\x75\x6d\x6c\x61\x75\x74"
      "\x73\x3a\xc3\xa4\xc3\xbc\xc3\x96\xc3\x9f", 26,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_MD5,
      NULL, 0,
      0,
      16,
      "\x21\xa4\xeb\xd8\xfd\xf0\x59\x25\xd1\x32\x31\xdb\xe7\xf2\x13\x5d"
    },
    {
      "\x61", 1,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x86\xf7\xe4\x37\xfa\xa5\xa7\xfc\xe1\x5d\x1d\xdc\xb9\xea\xea\xea"
    },
    {
      "\x61\x62", 2,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\xda\x23\x61\x4e\x02\x46\x9a\x0d\x7c\x7b\xd1\xbd\xab\x5c\x9c\x47"
    },
    {
      "\x61\x62\x63", 3,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\xa9\x99\x3e\x36\x47\x06\x81\x6a\xba\x3e\x25\x71\x78\x50\xc2\x6c"
    },
    {
      "\x61\x62\x63\x64", 4,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x81\xfe\x8b\xfe\x87\x57\x6c\x3e\xcb\x22\x42\x6f\x8e\x57\x84\x73"
    },
    {
      "\x61\x62\x63\x64\x65", 5,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x03\xde\x6c\x57\x0b\xfe\x24\xbf\xc3\x28\xcc\xd7\xca\x46\xb7\x6e"
    },
    {
      "\x61\x62\x63\x64\x65\x66", 6,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x1f\x8a\xc1\x0f\x23\xc5\xb5\xbc\x11\x67\xbd\xa8\x4b\x83\x3e\x5c"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67", 7,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x2f\xb5\xe1\x34\x19\xfc\x89\x24\x68\x65\xe7\xa3\x24\xf4\x76\xec"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68", 8,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x42\x5a\xf1\x2a\x07\x43\x50\x2b\x32\x2e\x93\xa0\x15\xbc\xf8\x68"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69", 9,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\xc6\x3b\x19\xf1\xe4\xc8\xb5\xf7\x6b\x25\xc4\x9b\x8b\x87\xf5\x7d"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f", 15,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x29\x38\xdc\xc2\xe3\xaa\x77\x98\x7c\x7e\x5d\x4a\x0f\x26\x96\x67"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70", 16,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x14\xf3\x99\x52\x88\xac\xd1\x89\xe6\xe5\x0a\x7a\xf4\x7e\xe7\x09"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71", 17,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\xd8\x3d\x62\x1f\xcd\x2d\x4d\x29\x85\x54\x70\x43\xa7\xa5\xfd\x4d"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72", 18,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\xe3\x81\xfe\x42\xc5\x7e\x48\xa0\x82\x17\x86\x41\xef\xfd\x1c\xb9"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72\x73", 19,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x89\x3e\x69\xff\x01\x09\xf3\x45\x9c\x42\x43\x01\x3b\x3d\xe8\xb1"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72\x73\x74", 20,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x14\xa2\x3a\xd7\x0f\x2a\x5d\xd7\x25\x57\x5d\xe6\xc4\x3e\x1c\xdd"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72\x73\x74\x75", 21,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\xec\xa9\x86\xb9\x5d\x58\x7f\x34\xd7\x1c\xa7\x75\x2a\x4e\x00\x10"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\x3e\x1b\x9a\x50\x7d\x6e\x9a\xd8\x93\x64\x96\x7a\x3f\xcb\x27\x3f"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      32,
      "\x3e\x1b\x9a\x50\x7d\x6e\x9a\xd8\x93\x64\x96\x7a\x3f\xcb\x27\x3f"
      "\xc3\x7b\x3a\xb2\xef\x4d\x68\xaa\x9c\xd7\xe4\x88\xee\xd1\x5e\x70"
    },
    {
      "\x57\x69\x74\x68\x5f\x75\x74\x66\x38\x5f\x75\x6d\x6c\x61\x75\x74"
      "\x73\x3a\xc3\xa4\xc3\xbc\xc3\x96\xc3\x9f", 26,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA1,
      NULL, 0,
      0,
      16,
      "\xe0\x4e\x1e\xe3\xad\x0b\x49\x7c\x7a\x5f\x37\x3b\x4d\x90\x3c\x2e"
    },
    {
      "\x61", 1,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x6d\x47\xe3\x68\x5d\x2c\x36\x16", 8,
      1024,
      16,
      "\x41\x9f\x48\x6e\xbf\xe6\xdd\x05\x9a\x72\x23\x17\x44\xd8\xd3\xf3"
    },
    {
      "\x61\x62", 2,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x7c\x34\x78\xfb\x28\x2d\x25\xc7", 8,
      1024,
      16,
      "\x0a\x9d\x09\x06\x43\x3d\x4f\xf9\x87\xd6\xf7\x48\x90\xde\xd1\x1c"
    },
    {
      "\x61\x62\x63", 3,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xc3\x16\x37\x2e\x27\xf6\x9f\x6f", 8,
      1024,
      16,
      "\xf8\x27\xa0\x07\xc6\xcb\xdd\xf1\xfe\x5c\x88\x3a\xfc\xcd\x84\x4d"
    },
    {
      "\x61\x62\x63\x64", 4,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xf0\x0c\x73\x38\xb7\xc3\xd5\x14", 8,
      1024,
      16,
      "\x9b\x5f\x26\xba\x52\x3b\xcd\xd9\xa5\x2a\xef\x3c\x03\x4d\xd1\x52"
    },
    {
      "\x61\x62\x63\x64\x65", 5,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xe1\x7d\xa2\x36\x09\x59\xee\xc5", 8,
      1024,
      16,
      "\x94\x9d\x5b\x1a\x5a\x66\x8c\xfa\x8f\x6f\x22\xaf\x8b\x60\x9f\xaf"
    },
    {
      "\x61\x62\x63\x64\x65\x66", 6,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xaf\xa7\x0c\x68\xdf\x7e\xaa\x27", 8,
      1024,
      16,
      "\xe5\x38\xf4\x39\x62\x27\xcd\xcc\x91\x37\x7f\x1b\xdc\x58\x64\x27"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67", 7,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x40\x57\xb2\x9d\x5f\xbb\x11\x4f", 8,
      1024,
      16,
      "\xad\xa2\x33\xd9\xdd\xe0\xfb\x94\x8e\xcc\xec\xcc\xb3\xa8\x3a\x9e"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68", 8,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x38\xf5\x65\xc5\x0f\x8c\x19\x61", 8,
      1024,
      16,
      "\xa0\xb0\x3e\x29\x76\xe6\x8f\xa0\xd8\x34\x8f\xa4\x2d\xfd\x65\xee"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69", 9,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xc3\xb7\x99\xcc\xda\x2d\x05\x7b", 8,
      1024,
      16,
      "\x27\x21\xc8\x99\x5f\xcf\x20\xeb\xf2\xd9\xff\x6a\x69\xff\xad\xe8"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f", 15,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x7d\xd8\x68\x8a\x1c\xc5\x47\x22", 8,
      1024,
      16,
      "\x0f\x96\x7a\x12\x23\x54\xf6\x92\x61\x67\x07\xb4\x68\x17\xb8\xaa"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70", 16,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x8a\x95\xd4\x88\x0b\xb8\xe9\x9d", 8,
      1024,
      16,
      "\xcc\xe4\xc8\x82\x53\x32\xf1\x93\x5a\x00\xd4\x7f\xd4\x46\xfa\x07"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71", 17,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xb5\x22\x48\xa6\xc4\xad\x74\x67", 8,
      1024,
      16,
      "\x0c\xe3\xe0\xee\x3d\x8f\x35\xd2\x35\x14\x14\x29\x0c\xf1\xe3\x34"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72", 18,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xac\x9f\x04\x63\x83\x0e\x3c\x95", 8,
      1024,
      16,
      "\x49\x0a\x04\x68\xa8\x2a\x43\x6f\xb9\x73\x94\xb4\x85\x9a\xaa\x0e"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72\x73", 19,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x03\x6f\x60\x30\x3a\x19\x61\x0d", 8,
      1024,
      16,
      "\x15\xe5\x9b\xbf\x1c\xf0\xbe\x74\x95\x1a\xb2\xc4\xda\x09\xcd\x99"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72\x73\x74", 20,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x51\x40\xa5\x57\xf5\x28\xfd\x03", 8,
      1024,
      16,
      "\xa6\xf2\x7e\x6b\x30\x4d\x8d\x67\xd4\xa2\x7f\xa2\x57\x27\xab\x96"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70"
      "\x71\x72\x73\x74\x75", 21,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x4c\xf1\x10\x11\x04\x70\xd3\x6e", 8,
      1024,
      16,
      "\x2c\x50\x79\x8d\x83\x23\xac\xd6\x22\x29\x37\xaf\x15\x0d\xdd\x8f"
    },
    {
      "\x57\x69\x74\x68\x5f\x75\x74\x66\x38\x5f\x75\x6d\x6c\x61\x75\x74"
      "\x73\x3a\xc3\xa4\xc3\xbc\xc3\x96\xc3\x9f", 26,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xfe\x3a\x25\xcb\x78\xef\xe1\x21", 8,
      1024,
      16,
      "\x2a\xb0\x53\x08\xf3\x2f\xd4\x6e\xeb\x01\x49\x5d\x87\xf6\x27\xf6"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x04\x97\xd0\x02\x6a\x44\x2d\xde", 8,
      1024,
      16,
      "\x57\xf5\x70\x41\xa0\x9b\x8c\x09\xca\x74\xa9\x22\xa5\x82\x2d\x17"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xdd\xf3\x31\x7c\xce\xf4\x81\x26", 8,
      10240,
      16,
      "\xc3\xdd\x01\x6d\xaf\xf6\x58\xc8\xd7\x79\xb4\x40\x00\xb5\xe8\x0b"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x95\xd6\x72\x4e\xfb\xe1\xc3\x1a", 8,
      102400,
      16,
      "\xf2\x3f\x36\x7f\xb4\x6a\xd0\x3a\x31\x9e\x65\x11\x8e\x2b\x99\x9b"
    },
    {
      "\x61", 1,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x6d\x69\x15\x18\xe4\x13\x42\x82", 8,
      1024,
      24,
      "\x28\x0c\x7e\xf2\x31\xf6\x1c\x6b\x5c\xef\x6a\xd5\x22\x64\x97\x91"
      "\xe3\x5e\x37\xfd\x50\xe2\xfc\x6c"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67", 7,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x9b\x76\x5e\x81\xde\x13\xdf\x15", 8,
      1024,
      24,
      "\x91\x1b\xa1\xc1\x7b\x4f\xc3\xb1\x80\x61\x26\x08\xbe\x53\xe6\x50"
      "\x40\x6f\x28\xed\xc6\xe6\x67\x55"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69", 9,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x7a\xac\xcc\x6e\x15\x56\xbd\xa1", 8,
      1024,
      24,
      "\xfa\x7e\x20\x07\xb6\x47\xb0\x09\x46\xb8\x38\xfb\xa1\xaf\xf7\x75"
      "\x2a\xfa\x77\x14\x06\x54\xcb\x34"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69", 9,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x1c\x68\xf8\xfb\x98\xf7\x8c\x39", 8,
      1024,
      24,
      "\xcb\x1e\x86\xf5\xe0\xe4\xfb\xbf\x71\x34\x99\x24\xf4\x39\x8c\xc2"
      "\x8e\x25\x1c\x4c\x96\x47\x22\xe8"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x10\xa9\x4e\xc1\xa5\xec\x17\x52", 8,
      1024,
      24,
      "\x0f\x83\xa2\x77\x92\xbb\xe4\x58\x68\xc5\xf2\x14\x6e\x6e\x2e\x6b"
      "\x98\x17\x70\x92\x07\x44\xe0\x51"
    },
    {
      "\x61", 1,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xef\x8f\x37\x61\x8f\xab\xae\x4f", 8,
      1024,
      32,
      "\x6d\x65\xae\x86\x23\x91\x39\x98\xec\x1c\x23\x44\xb6\x0d\xad\x32"
      "\x54\x46\xc7\x23\x26\xbb\xdf\x4b\x54\x6e\xd4\xc2\xfa\xc6\x17\x17"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67", 7,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xaa\xfb\xd9\x06\x7d\x7c\x40\xaf", 8,
      1024,
      32,
      "\x7d\x10\x54\x13\x3c\x43\x7a\xb3\x54\x1f\x38\xd4\x8f\x70\x0a\x09"
      "\xe2\xfa\xab\x97\x9a\x70\x16\xef\x66\x68\xca\x34\x2e\xce\xfa\x1f"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69", 9,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x58\x03\x4f\x56\x8b\x97\xd4\x98", 8,
      1024,
      32,
      "\xf7\x40\xb1\x25\x86\x0d\x35\x8f\x9f\x91\x2d\xce\x04\xee\x5a\x04"
      "\x9d\xbd\x44\x23\x4c\xa6\xbb\xab\xb0\xd0\x56\x82\xa9\xda\x47\x16"
    },
    {
      "\x61\x62\x63\x64\x65\x66\x67\x68\x69", 9,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\x5d\x41\x3d\xa3\xa7\xfc\x5d\x0c", 8,
      1024,
      32,
      "\x4c\x7a\x86\xed\x81\x8a\x94\x99\x7d\x4a\xc4\xf7\x1c\xf8\x08\xdb"
      "\x09\x35\xd9\xa3\x2d\x22\xde\x32\x2d\x74\x38\xe5\xc8\xf2\x50\x6e"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA1,
      "\xca\xa7\xdc\x59\xce\x31\xe7\x49", 8,
      1024,
      32,
      "\x67\xe9\xd6\x29\x49\x1c\xb6\xa0\x85\xe8\xf9\x8b\x85\x47\x3a\x7e"
      "\xa7\xee\x89\x52\x6f\x19\x00\x53\x93\x07\x0a\x8b\xb9\xa8\x86\x94"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_SIMPLE_S2K, GCRY_MD_SHA256,
      NULL, 0,
      0,
      16,
      "\x88\x36\x78\x6b\xd9\x5a\x62\xff\x47\xd3\xfb\x79\xc9\x08\x70\x56"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_SALTED_S2K, GCRY_MD_SHA256,
      "\x05\x8b\xfe\x31\xaa\xf3\x29\x11", 8,
      0,
      16,
      "\xb2\x42\xfe\x5e\x09\x02\xd9\x62\xb9\x35\xf3\xa8\x43\x80\x9f\xb1"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA256,
      "\xd3\x4a\xea\xc9\x97\x1b\xcc\x83", 8,
      1024,
      16,
      "\x35\x37\x99\x62\x07\x26\x68\x23\x05\x47\xb2\xa0\x0b\x2b\x2b\x8d"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA256,
      "\x5e\x71\xbd\x00\x5f\x96\xc4\x23", 8,
      10240,
      16,
      "\xa1\x6a\xee\xba\xde\x73\x25\x25\xd1\xab\xa0\xc5\x7e\xc6\x39\xa7"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA384,
      "\xc3\x08\xeb\x17\x62\x08\x89\xef", 8,
      1024,
      16,
      "\x9b\x7f\x0c\x81\x6f\x71\x59\x9b\xd5\xf6\xbf\x3a\x86\x20\x16\x33"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA512,
      "\xe6\x7d\x13\x6b\x39\xe3\x44\x05", 8,
      1024,
      16,
      "\xc8\xcd\x4b\xa4\xf3\xf1\xd5\xb0\x59\x06\xf0\xbb\x89\x34\x6a\xad"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA512,
      "\xed\x7d\x30\x47\xe4\xc3\xf8\xb6", 8,
      1024,
      32,
      "\x89\x7a\xef\x70\x97\xe7\x10\xdb\x75\xcc\x20\x22\xab\x7b\xf3\x05"
      "\x4b\xb6\x2e\x17\x11\x9f\xd6\xeb\xbf\xdf\x4d\x70\x59\xf0\xf9\xe5"
    },
    {
      "\x4c\x6f\x6e\x67\x5f\x73\x65\x6e\x74\x65\x6e\x63\x65\x5f\x75\x73"
      "\x65\x64\x5f\x61\x73\x5f\x70\x61\x73\x73\x70\x68\x72\x61\x73\x65", 32,
      GCRY_KDF_ITERSALTED_S2K, GCRY_MD_SHA512,
      "\xbb\x1a\x45\x30\x68\x62\x6d\x63", 8,
      1024,
      24,
      "\xde\x5c\xb8\xd5\x75\xf6\xad\x69\x5b\xc9\xf6\x2f\xba\xeb\xfb\x36"
      "\x34\xf2\xb8\xee\x3b\x37\x21\xb7"
    }
  };
  int tvidx;
  gpg_error_t err;
  unsigned char outbuf[32];
  int i;

  for (tvidx=0; tvidx < DIM(tv); tvidx++)
    {
      if (tv[tvidx].disabled)
        continue;
      if (verbose)
        fprintf (stderr, "checking S2K test vector %d\n", tvidx);
      assert (tv[tvidx].dklen <= sizeof outbuf);
      err = gcry_kdf_derive (tv[tvidx].p, tv[tvidx].plen,
                             tv[tvidx].algo, tv[tvidx].hashalgo,
                             tv[tvidx].salt, tv[tvidx].saltlen,
                             tv[tvidx].c, tv[tvidx].dklen, outbuf);
      if (err)
        fail ("s2k test %d failed: %s\n", tvidx, gpg_strerror (err));
      else if (memcmp (outbuf, tv[tvidx].dk, tv[tvidx].dklen))
        {
          fail ("s2k test %d failed: mismatch\n", tvidx);
          fputs ("got:", stderr);
          for (i=0; i < tv[tvidx].dklen; i++)
            fprintf (stderr, " %02x", outbuf[i]);
          putc ('\n', stderr);
        }
    }
}


static void
check_pbkdf2 (void)
{
  /* Test vectors are from RFC-6070.  */
  static struct {
    const char *p;   /* Passphrase.  */
    size_t plen;     /* Length of P. */
    const char *salt;
    size_t saltlen;
    unsigned long c; /* Iterations.  */
    int dklen;       /* Requested key length.  */
    const char *dk;  /* Derived key.  */
    int disabled;
  } tv[] = {
    {
      "password", 8,
      "salt", 4,
      1,
      20,
      "\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9"
      "\xb5\x24\xaf\x60\x12\x06\x2f\xe0\x37\xa6"
    },
    {
      "password", 8,
      "salt", 4,
      2,
      20,
      "\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e"
      "\xd9\x2a\xce\x1d\x41\xf0\xd8\xde\x89\x57"
    },
    {
      "password", 8,
      "salt", 4,
      4096,
      20,
      "\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad"
      "\x49\xd9\x26\xf7\x21\xd0\x65\xa4\x29\xc1"
    },
    {
      "password", 8,
      "salt", 4,
      16777216,
      20,
      "\xee\xfe\x3d\x61\xcd\x4d\xa4\xe4\xe9\x94"
      "\x5b\x3d\x6b\xa2\x15\x8c\x26\x34\xe9\x84",
      1 /* This test takes too long.  */
    },
    {
      "passwordPASSWORDpassword", 24,
      "saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
      4096,
      25,
      "\x3d\x2e\xec\x4f\xe4\x1c\x84\x9b\x80\xc8"
      "\xd8\x36\x62\xc0\xe4\x4a\x8b\x29\x1a\x96"
      "\x4c\xf2\xf0\x70\x38"
    },
    {
      "pass\0word", 9,
      "sa\0lt", 5,
      4096,
      16,
      "\x56\xfa\x6a\xa7\x55\x48\x09\x9d\xcc\x37"
      "\xd7\xf0\x34\x25\xe0\xc3"
    },
    { /* empty password test, not in RFC-6070 */
      "", 0,
      "salt", 4,
      2,
      20,
      "\x13\x3a\x4c\xe8\x37\xb4\xd2\x52\x1e\xe2"
      "\xbf\x03\xe1\x1c\x71\xca\x79\x4e\x07\x97"
    },
  };
  int tvidx;
  gpg_error_t err;
  unsigned char outbuf[32];
  int i;

  for (tvidx=0; tvidx < DIM(tv); tvidx++)
    {
      if (tv[tvidx].disabled)
        continue;
      if (verbose)
        fprintf (stderr, "checking PBKDF2 test vector %d\n", tvidx);
      assert (tv[tvidx].dklen <= sizeof outbuf);
      err = gcry_kdf_derive (tv[tvidx].p, tv[tvidx].plen,
                             GCRY_KDF_PBKDF2, GCRY_MD_SHA1,
                             tv[tvidx].salt, tv[tvidx].saltlen,
                             tv[tvidx].c, tv[tvidx].dklen, outbuf);
      if (err)
        fail ("pbkdf2 test %d failed: %s\n", tvidx, gpg_strerror (err));
      else if (memcmp (outbuf, tv[tvidx].dk, tv[tvidx].dklen))
        {
          fail ("pbkdf2 test %d failed: mismatch\n", tvidx);
          fputs ("got:", stderr);
          for (i=0; i < tv[tvidx].dklen; i++)
            fprintf (stderr, " %02x", outbuf[i]);
          putc ('\n', stderr);
        }
    }
}


int
main (int argc, char **argv)
{
  int debug = 0;

  if (argc > 1 && !strcmp (argv[1], "--verbose"))
    verbose = 1;
  else if (argc > 1 && !strcmp (argv[1], "--debug"))
    verbose = debug = 1;

  if (!gcry_check_version (GCRYPT_VERSION))
    die ("version mismatch\n");

  gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
  if (debug)
    gcry_control (GCRYCTL_SET_DEBUG_FLAGS, 1u, 0);

  check_openpgp ();
  check_pbkdf2 ();

  return error_count ? 1 : 0;
}
