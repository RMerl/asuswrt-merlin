/* pkcs1v2.c - Test OAEP and PSS padding
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _GCRYPT_IN_LIBGCRYPT
# include "../src/gcrypt.h"
#else
# include <gcrypt.h>
#endif


#define my_isascii(c) (!((c) & 0x80))
#define digitp(p)   (*(p) >= '0' && *(p) <= '9')
#define hexdigitp(a) (digitp (a)                     \
                      || (*(a) >= 'A' && *(a) <= 'F')  \
                      || (*(a) >= 'a' && *(a) <= 'f'))
#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))
#define DIM(v)		     (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)

static int verbose;
static int die_on_error;
static int error_count;


static void
info (const char *format, ...)
{
  va_list arg_ptr;

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
}

static void
fail (const char *format, ...)
{
  va_list arg_ptr;

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  error_count++;
  if (die_on_error)
    exit (1);
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
show_sexp (const char *prefix, gcry_sexp_t a)
{
  char *buf;
  size_t size;

  if (prefix)
    fputs (prefix, stderr);
  size = gcry_sexp_sprint (a, GCRYSEXP_FMT_ADVANCED, NULL, 0);
  buf = gcry_xmalloc (size);

  gcry_sexp_sprint (a, GCRYSEXP_FMT_ADVANCED, buf, size);
  fprintf (stderr, "%.*s", (int)size, buf);
  gcry_free (buf);
}


/* Convert STRING consisting of hex characters into its binary
   representation and return it as an allocated buffer. The valid
   length of the buffer is returned at R_LENGTH.  The string is
   delimited by end of string.  The function returns NULL on
   error.  */
static void *
data_from_hex (const char *string, size_t *r_length)
{
  const char *s;
  unsigned char *buffer;
  size_t length;

  buffer = gcry_xmalloc (strlen(string)/2+1);
  length = 0;
  for (s=string; *s; s +=2 )
    {
      if (!hexdigitp (s) || !hexdigitp (s+1))
        die ("error parsing hex string `%s'\n", string);
      ((unsigned char*)buffer)[length++] = xtoi_2 (s);
    }
  *r_length = length;
  return buffer;
}


static int
extract_cmp_data (gcry_sexp_t sexp, const char *name, const char *expected,
                  const char *description)
{
  gcry_sexp_t l1;
  const void *a;
  size_t alen;
  void *b;
  size_t blen;
  int rc = 0;

  l1 = gcry_sexp_find_token (sexp, name, 0);
  a = gcry_sexp_nth_data (l1, 1, &alen);
  b = data_from_hex (expected, &blen);
  if (!a)
    {
      info ("%s: parameter \"%s\" missing in key\n", description, name);
      rc = 1;
    }
  else if ( alen != blen || memcmp (a, b, alen) )
    {
      info ("%s: parameter \"%s\" does not match expected value\n",
            description, name);
      rc = 1;
    }
  gcry_free (b);
  gcry_sexp_release (l1);
  return rc;
}


/* Check against the OAEP test vectors from
   ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1d2-vec.zip .  */
static void
check_oaep (void)
{
#include "pkcs1v2-oaep.h"
  gpg_error_t err;
  int tno, mno;

  for (tno = 0; tno < DIM (tbl); tno++)
    {
      void *rsa_n, *rsa_e, *rsa_d;
      size_t rsa_n_len, rsa_e_len, rsa_d_len;
      gcry_sexp_t sec_key, pub_key;

      if (verbose > 1)
        info ("(%s)\n", tbl[tno].desc);

      rsa_n = data_from_hex (tbl[tno].n, &rsa_n_len);
      rsa_e = data_from_hex (tbl[tno].e, &rsa_e_len);
      rsa_d = data_from_hex (tbl[tno].d, &rsa_d_len);
      err = gcry_sexp_build (&sec_key, NULL,
                             "(private-key (rsa (n %b)(e %b)(d %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e,
                             (int)rsa_d_len, rsa_d);
      if (err)
        die ("constructing private key failed: %s\n", gpg_strerror (err));
      err = gcry_sexp_build (&pub_key, NULL,
                             "(public-key (rsa (n %b)(e %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e);
      if (err)
        die ("constructing public key failed: %s\n", gpg_strerror (err));
      gcry_free (rsa_n);
      gcry_free (rsa_e);
      gcry_free (rsa_d);

      for (mno = 0; mno < DIM (tbl[0].m); mno++)
        {
          void *mesg, *seed, *encr;
          size_t mesg_len, seed_len, encr_len;
          gcry_sexp_t plain, ciph;

          if (verbose)
            info ("running test: %s\n", tbl[tno].m[mno].desc);

          mesg = data_from_hex (tbl[tno].m[mno].mesg, &mesg_len);
          seed = data_from_hex (tbl[tno].m[mno].seed, &seed_len);

          err = gcry_sexp_build (&plain, NULL,
                                 "(data (flags oaep)(hash-algo sha1)"
                                 "(value %b)(random-override %b))",
                                 (int)mesg_len, mesg,
                                 (int)seed_len, seed);
          if (err)
            die ("constructing plain data failed: %s\n", gpg_strerror (err));
          gcry_free (mesg);
          gcry_free (seed);

          err = gcry_pk_encrypt (&ciph, plain, pub_key);
          if (err)
            {
              show_sexp ("plain:\n", ciph);
              fail ("gcry_pk_encrypt failed: %s\n", gpg_strerror (err));
            }
          else
            {
              if (extract_cmp_data (ciph, "a", tbl[tno].m[mno].encr,
                                    tbl[tno].m[mno].desc))
                {
                  show_sexp ("encrypt result:\n", ciph);
                  fail ("mismatch in gcry_pk_encrypt\n");
                }
              gcry_sexp_release (ciph);
              ciph = NULL;
            }
          gcry_sexp_release (plain);
          plain = NULL;

          /* Now test the decryption.  */
          seed = data_from_hex (tbl[tno].m[mno].seed, &seed_len);
          encr = data_from_hex (tbl[tno].m[mno].encr, &encr_len);

          err = gcry_sexp_build (&ciph, NULL,
                                 "(enc-val (flags oaep)(hash-algo sha1)"
                                 "(random-override %b)"
                                 "(rsa (a %b)))",
                                 (int)seed_len, seed,
                                 (int)encr_len, encr);
          if (err)
            die ("constructing cipher data failed: %s\n", gpg_strerror (err));
          gcry_free (encr);
          gcry_free (seed);

          err = gcry_pk_decrypt (&plain, ciph, sec_key);
          if (err)
            {
              show_sexp ("ciph:\n", ciph);
              fail ("gcry_pk_decrypt failed: %s\n", gpg_strerror (err));
            }
          else
            {
              if (extract_cmp_data (plain, "value", tbl[tno].m[mno].mesg,
                                    tbl[tno].m[mno].desc))
                {
                  show_sexp ("decrypt result:\n", plain);
                  fail ("mismatch in gcry_pk_decrypt\n");
                }
              gcry_sexp_release (plain);
              plain = NULL;
            }
          gcry_sexp_release (ciph);
          ciph = NULL;
        }

      gcry_sexp_release (sec_key);
      gcry_sexp_release (pub_key);
    }
}


/* Check against the PSS test vectors from
   ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1d2-vec.zip .  */
static void
check_pss (void)
{
#include "pkcs1v2-pss.h"
  gpg_error_t err;
  int tno, mno;

  for (tno = 0; tno < DIM (tbl); tno++)
    {
      void *rsa_n, *rsa_e, *rsa_d;
      size_t rsa_n_len, rsa_e_len, rsa_d_len;
      gcry_sexp_t sec_key, pub_key;

      if (verbose > 1)
        info ("(%s)\n", tbl[tno].desc);

      rsa_n = data_from_hex (tbl[tno].n, &rsa_n_len);
      rsa_e = data_from_hex (tbl[tno].e, &rsa_e_len);
      rsa_d = data_from_hex (tbl[tno].d, &rsa_d_len);
      err = gcry_sexp_build (&sec_key, NULL,
                             "(private-key (rsa (n %b)(e %b)(d %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e,
                             (int)rsa_d_len, rsa_d);
      if (err)
        die ("constructing private key failed: %s\n", gpg_strerror (err));
      err = gcry_sexp_build (&pub_key, NULL,
                             "(public-key (rsa (n %b)(e %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e);
      if (err)
        die ("constructing public key failed: %s\n", gpg_strerror (err));
      gcry_free (rsa_n);
      gcry_free (rsa_e);
      gcry_free (rsa_d);

      for (mno = 0; mno < DIM (tbl[0].m); mno++)
        {
          void *mesg, *salt, *sign;
          size_t mesg_len, salt_len, sign_len;
          gcry_sexp_t sigtmpl, sig;
          char mhash[20];

          if (verbose)
            info ("running test: %s\n", tbl[tno].m[mno].desc);

          mesg = data_from_hex (tbl[tno].m[mno].mesg, &mesg_len);
          salt = data_from_hex (tbl[tno].m[mno].salt, &salt_len);

          gcry_md_hash_buffer (GCRY_MD_SHA1, mhash, mesg, mesg_len);
          err = gcry_sexp_build (&sigtmpl, NULL,
                                 "(data (flags pss)"
                                 "(hash sha1 %b)"
                                 "(random-override %b))",
                                 20, mhash,
                                 (int)salt_len, salt);
          if (err)
            die ("constructing sig template failed: %s\n", gpg_strerror (err));
          gcry_free (mesg);
          gcry_free (salt);

          err = gcry_pk_sign (&sig, sigtmpl, sec_key);
          if (err)
            {
              show_sexp ("sigtmpl:\n", sigtmpl);
              fail ("gcry_pk_sign failed: %s\n", gpg_strerror (err));
            }
          else
            {
              if (extract_cmp_data (sig, "s", tbl[tno].m[mno].sign,
                                    tbl[tno].m[mno].desc))
                {
                  show_sexp ("sign result:\n", sig);
                  fail ("mismatch in gcry_pk_sign\n");
                }
              gcry_sexp_release (sig);
              sig = NULL;
            }
          gcry_sexp_release (sigtmpl);
          sigtmpl = NULL;

          /* Now test the verification.  */
          salt = data_from_hex (tbl[tno].m[mno].salt, &salt_len);
          sign = data_from_hex (tbl[tno].m[mno].sign, &sign_len);

          err = gcry_sexp_build (&sig, NULL,
                                 "(sig-val(rsa(s %b)))",
                                 (int)sign_len, sign);
          if (err)
            die ("constructing verify data failed: %s\n", gpg_strerror (err));
          err = gcry_sexp_build (&sigtmpl, NULL,
                                 "(data (flags pss)"
                                 "(hash sha1 %b)"
                                 "(random-override %b))",
                                 20, mhash,
                                 (int)salt_len, salt);
          if (err)
            die ("constructing verify tmpl failed: %s\n", gpg_strerror (err));
          gcry_free (sign);
          gcry_free (salt);

          err = gcry_pk_verify (sig, sigtmpl, pub_key);
          if (err)
            {
              show_sexp ("sig:\n", sig);
              show_sexp ("sigtmpl:\n", sigtmpl);
              fail ("gcry_pk_verify failed: %s\n", gpg_strerror (err));
            }
          gcry_sexp_release (sig);
          sig = NULL;
          gcry_sexp_release (sigtmpl);
          sigtmpl = NULL;
        }

      gcry_sexp_release (sec_key);
      gcry_sexp_release (pub_key);
    }
}


/* Check against PKCS#1 v1.5 encryption test  vectors as found at
   ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15crypt-vectors.txt .  */
static void
check_v15crypt (void)
{
#include "pkcs1v2-v15c.h"
  gpg_error_t err;
  int tno, mno;

  for (tno = 0; tno < DIM (tbl); tno++)
    {
      void *rsa_n, *rsa_e, *rsa_d;
      size_t rsa_n_len, rsa_e_len, rsa_d_len;
      gcry_sexp_t sec_key, pub_key;

      if (verbose > 1)
        info ("(%s)\n", tbl[tno].desc);

      rsa_n = data_from_hex (tbl[tno].n, &rsa_n_len);
      rsa_e = data_from_hex (tbl[tno].e, &rsa_e_len);
      rsa_d = data_from_hex (tbl[tno].d, &rsa_d_len);
      err = gcry_sexp_build (&sec_key, NULL,
                             "(private-key (rsa (n %b)(e %b)(d %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e,
                             (int)rsa_d_len, rsa_d);
      if (err)
        die ("constructing private key failed: %s\n", gpg_strerror (err));
      err = gcry_sexp_build (&pub_key, NULL,
                             "(public-key (rsa (n %b)(e %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e);
      if (err)
        die ("constructing public key failed: %s\n", gpg_strerror (err));
      gcry_free (rsa_n);
      gcry_free (rsa_e);
      gcry_free (rsa_d);

      for (mno = 0; mno < DIM (tbl[0].m); mno++)
        {
          void *mesg, *seed, *encr;
          size_t mesg_len, seed_len, encr_len;
          gcry_sexp_t plain, ciph;

          if (verbose)
            info ("running test: %s\n", tbl[tno].m[mno].desc);

          mesg = data_from_hex (tbl[tno].m[mno].mesg, &mesg_len);
          seed = data_from_hex (tbl[tno].m[mno].seed, &seed_len);

          err = gcry_sexp_build (&plain, NULL,
                                 "(data (flags pkcs1)(hash-algo sha1)"
                                 "(value %b)(random-override %b))",
                                 (int)mesg_len, mesg,
                                 (int)seed_len, seed);
          if (err)
            die ("constructing plain data failed: %s\n", gpg_strerror (err));
          gcry_free (mesg);
          gcry_free (seed);

          err = gcry_pk_encrypt (&ciph, plain, pub_key);
          if (err)
            {
              show_sexp ("plain:\n", ciph);
              fail ("gcry_pk_encrypt failed: %s\n", gpg_strerror (err));
            }
          else
            {
              if (extract_cmp_data (ciph, "a", tbl[tno].m[mno].encr,
                                    tbl[tno].m[mno].desc))
                {
                  show_sexp ("encrypt result:\n", ciph);
                  fail ("mismatch in gcry_pk_encrypt\n");
                }
              gcry_sexp_release (ciph);
              ciph = NULL;
            }
          gcry_sexp_release (plain);
          plain = NULL;

          /* Now test the decryption.  */
          seed = data_from_hex (tbl[tno].m[mno].seed, &seed_len);
          encr = data_from_hex (tbl[tno].m[mno].encr, &encr_len);

          err = gcry_sexp_build (&ciph, NULL,
                                 "(enc-val (flags pkcs1)(hash-algo sha1)"
                                 "(random-override %b)"
                                 "(rsa (a %b)))",
                                 (int)seed_len, seed,
                                 (int)encr_len, encr);
          if (err)
            die ("constructing cipher data failed: %s\n", gpg_strerror (err));
          gcry_free (encr);
          gcry_free (seed);

          err = gcry_pk_decrypt (&plain, ciph, sec_key);
          if (err)
            {
              show_sexp ("ciph:\n", ciph);
              fail ("gcry_pk_decrypt failed: %s\n", gpg_strerror (err));
            }
          else
            {
              if (extract_cmp_data (plain, "value", tbl[tno].m[mno].mesg,
                                    tbl[tno].m[mno].desc))
                {
                  show_sexp ("decrypt result:\n", plain);
                  fail ("mismatch in gcry_pk_decrypt\n");
                }
              gcry_sexp_release (plain);
              plain = NULL;
            }
          gcry_sexp_release (ciph);
          ciph = NULL;
        }

      gcry_sexp_release (sec_key);
      gcry_sexp_release (pub_key);
    }
}


/* Check against PKCS#1 v1.5 signature test vectors as found at
   ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt .  */
static void
check_v15sign (void)
{
#include "pkcs1v2-v15s.h"
  gpg_error_t err;
  int tno, mno;

  for (tno = 0; tno < DIM (tbl); tno++)
    {
      void *rsa_n, *rsa_e, *rsa_d;
      size_t rsa_n_len, rsa_e_len, rsa_d_len;
      gcry_sexp_t sec_key, pub_key;

      if (verbose > 1)
        info ("(%s)\n", tbl[tno].desc);

      rsa_n = data_from_hex (tbl[tno].n, &rsa_n_len);
      rsa_e = data_from_hex (tbl[tno].e, &rsa_e_len);
      rsa_d = data_from_hex (tbl[tno].d, &rsa_d_len);
      err = gcry_sexp_build (&sec_key, NULL,
                             "(private-key (rsa (n %b)(e %b)(d %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e,
                             (int)rsa_d_len, rsa_d);
      if (err)
        die ("constructing private key failed: %s\n", gpg_strerror (err));
      err = gcry_sexp_build (&pub_key, NULL,
                             "(public-key (rsa (n %b)(e %b)))",
                             (int)rsa_n_len, rsa_n,
                             (int)rsa_e_len, rsa_e);
      if (err)
        die ("constructing public key failed: %s\n", gpg_strerror (err));
      gcry_free (rsa_n);
      gcry_free (rsa_e);
      gcry_free (rsa_d);

      for (mno = 0; mno < DIM (tbl[0].m); mno++)
        {
          void *mesg, *sign;
          size_t mesg_len, sign_len;
          gcry_sexp_t sigtmpl, sig;
          char mhash[20];

          if (verbose)
            info ("running test: %s\n", tbl[tno].m[mno].desc);

          mesg = data_from_hex (tbl[tno].m[mno].mesg, &mesg_len);

          gcry_md_hash_buffer (GCRY_MD_SHA1, mhash, mesg, mesg_len);
          err = gcry_sexp_build (&sigtmpl, NULL,
                                 "(data (flags pkcs1)"
                                 "(hash sha1 %b))",
                                 20, mhash);
          if (err)
            die ("constructing sig template failed: %s\n", gpg_strerror (err));
          gcry_free (mesg);

          err = gcry_pk_sign (&sig, sigtmpl, sec_key);
          if (err)
            {
              show_sexp ("sigtmpl:\n", sigtmpl);
              fail ("gcry_pk_sign failed: %s\n", gpg_strerror (err));
            }
          else
            {
              if (extract_cmp_data (sig, "s", tbl[tno].m[mno].sign,
                                    tbl[tno].m[mno].desc))
                {
                  show_sexp ("sign result:\n", sig);
                  fail ("mismatch in gcry_pk_sign\n");
                }
              gcry_sexp_release (sig);
              sig = NULL;
            }
          gcry_sexp_release (sigtmpl);
          sigtmpl = NULL;

          /* Now test the verification.  */
          sign = data_from_hex (tbl[tno].m[mno].sign, &sign_len);

          err = gcry_sexp_build (&sig, NULL,
                                 "(sig-val(rsa(s %b)))",
                                 (int)sign_len, sign);
          if (err)
            die ("constructing verify data failed: %s\n", gpg_strerror (err));
          err = gcry_sexp_build (&sigtmpl, NULL,
                                 "(data (flags pkcs1)"
                                 "(hash sha1 %b))",
                                 20, mhash);
          if (err)
            die ("constructing verify tmpl failed: %s\n", gpg_strerror (err));
          gcry_free (sign);

          err = gcry_pk_verify (sig, sigtmpl, pub_key);
          if (err)
            {
              show_sexp ("sig:\n", sig);
              show_sexp ("sigtmpl:\n", sigtmpl);
              fail ("gcry_pk_verify failed: %s\n", gpg_strerror (err));
            }
          gcry_sexp_release (sig);
          sig = NULL;
          gcry_sexp_release (sigtmpl);
          sigtmpl = NULL;
        }

      gcry_sexp_release (sec_key);
      gcry_sexp_release (pub_key);
    }
}


int
main (int argc, char **argv)
{
  int last_argc = -1;
  int debug = 0;
  int run_oaep = 0;
  int run_pss = 0;
  int run_v15c = 0;
  int run_v15s = 0;

  if (argc)
    { argc--; argv++; }

  while (argc && last_argc != argc )
    {
      last_argc = argc;
      if (!strcmp (*argv, "--"))
        {
          argc--; argv++;
          break;
        }
      else if (!strcmp (*argv, "--verbose"))
        {
          verbose++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--debug"))
        {
          verbose = 2;
          debug = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--die"))
        {
          die_on_error = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--oaep"))
        {
          run_oaep = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--pss"))
        {
          run_pss = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--v15c"))
        {
          run_v15c = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--v15s"))
        {
          run_v15s = 1;
          argc--; argv++;
        }
    }

  if (!run_oaep && !run_pss && !run_v15c && !run_v15s)
    run_oaep = run_pss = run_v15c = run_v15s = 1;

  gcry_control (GCRYCTL_SET_VERBOSITY, (int)verbose);
  gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
  if (!gcry_check_version ("1.5.0"))
    die ("version mismatch\n");
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
  if (debug)
    gcry_control (GCRYCTL_SET_DEBUG_FLAGS, 1u, 0);
  /* No valuable keys are create, so we can speed up our RNG. */
  gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);

  if (run_oaep)
    check_oaep ();
  if (run_pss)
    check_pss ();
  if (run_v15c)
    check_v15crypt ();
  if (run_v15s)
    check_v15sign ();

  if (verbose)
    fprintf (stderr, "\nAll tests completed.  Errors: %i\n", error_count);

  return error_count ? 1 : 0;
}
