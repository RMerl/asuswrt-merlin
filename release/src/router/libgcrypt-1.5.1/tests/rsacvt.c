/* rsacvt.c  -  A debug tool to convert RSA formats.
   Copyright (C) 2009 Free Software Foundation, Inc.

   This file is part of Libgcrypt.

   Libgcrypt is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Libgcrypt is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Input data format:

=======
# A hash denotes a comment line
e861b700e17e8afe68[...]f1
f7a7ca5367c661f8e6[...]61
10001

# After an empty line another input block may follow.
7861b700e17e8afe68[...]f3
e7a7ca5367c661f8e6[...]71
3
=========

*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#ifdef HAVE_W32_SYSTEM
# include <fcntl.h> /* We need setmode().  */
#else
# include <signal.h>
#endif
#include <assert.h>
#include <unistd.h>

#ifdef _GCRYPT_IN_LIBGCRYPT
# include "../src/gcrypt.h"
#else
# include <gcrypt.h>
# define PACKAGE_BUGREPORT "devnull@example.org"
# define PACKAGE_VERSION "[build on " __DATE__ " " __TIME__ "]"
#endif


#define PGM "rsacvt"

#define my_isascii(c) (!((c) & 0x80))
#define digitp(p)   (*(p) >= '0' && *(p) <= '9')
#define hexdigitp(a) (digitp (a)                     \
                      || (*(a) >= 'A' && *(a) <= 'F')  \
                      || (*(a) >= 'a' && *(a) <= 'f'))
#define xtoi_1(p)   (*(p) <= '9'? (*(p)- '0'): \
                     *(p) <= 'F'? (*(p)-'A'+10):(*(p)-'a'+10))
#define xtoi_2(p)   ((xtoi_1(p) * 16) + xtoi_1((p)+1))
#define DIM(v)               (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)


/* Verbose mode flag.  */
static int verbose;

/* Prefix output with labels.  */
static int with_labels;

/* Do not suppress leading zeroes.  */
static int keep_lz;

/* Create parameters as specified by OpenPGP (rfc4880).  That is we
   don't store dmp1 and dmp1 but d and make sure that p is less than  q.  */
static int openpgp_mode;


/* Print a error message and exit the process with an error code.  */
static void
die (const char *format, ...)
{
  va_list arg_ptr;

  va_start (arg_ptr, format);
  fputs (PGM ": ", stderr);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  exit (1);
}


static char *
read_textline (FILE *fp)
{
  char line[4096];
  char *p;
  int any = 0;

  /* Read line but skip over initial empty lines.  */
  do
    {
      do
        {
          if (!fgets (line, sizeof line, fp))
            {
              if (feof (fp))
                return NULL;
              die ("error reading input line: %s\n", strerror (errno));
            }
          p = strchr (line, '\n');
          if (p)
            *p = 0;
          p = line + (*line? (strlen (line)-1):0);
          for ( ;p > line; p--)
            if (my_isascii (*p) && isspace (*p))
              *p = 0;
        }
      while (!any && !*line);
      any = 1;
    }
  while (*line == '#');  /* Always skip comment lines.  */
  if (verbose > 1)
    fprintf (stderr, PGM ": received line: %s\n", line);
  return gcry_xstrdup (line);
}


static gcry_mpi_t
read_hexmpi_line (FILE *fp, int *got_eof)
{
  gpg_error_t err;
  gcry_mpi_t a;
  char *line;

  *got_eof = 0;
  line = read_textline (fp);
  if (!line)
    {
      *got_eof = 1;
      return NULL;
    }
  err = gcry_mpi_scan (&a, GCRYMPI_FMT_HEX, line, 0, NULL);
  gcry_free (line);
  if (err)
    a = NULL;
  return a;
}


static int
skip_to_empty_line (FILE *fp)
{
  char line[256];
  char *p;

  do
    {
      if (!fgets (line, sizeof line, fp))
        {
          if (feof (fp))
            return -1;
          die ("error reading input line: %s\n", strerror (errno));
        }
      p = strchr (line, '\n');
      if (p)
        *p =0;
    }
  while (*line);
  return 0;
}


/* Print an MPI on a line.  */
static void
print_mpi_line (const char *label, gcry_mpi_t a)
{
  unsigned char *buf, *p;
  gcry_error_t err;
  int writerr = 0;

  if (with_labels && label)
    printf ("%s = ", label);

  err = gcry_mpi_aprint (GCRYMPI_FMT_HEX, &buf, NULL, a);
  if (err)
    die ("gcry_mpi_aprint failed: %s\n", gpg_strerror (err));

  p = buf;
  if (!keep_lz && p[0] == '0' && p[1] == '0' && p[2])
    p += 2;

  printf ("%s\n", p);
  if (ferror (stdout))
    writerr++;
  if (!writerr && fflush (stdout) == EOF)
    writerr++;
  if (writerr)
    die ("writing output failed: %s\n", strerror (errno));
  gcry_free (buf);
}


/* Compute and print missing RSA parameters.  */
static void
compute_missing (gcry_mpi_t rsa_p, gcry_mpi_t rsa_q, gcry_mpi_t rsa_e)
{
  gcry_mpi_t rsa_n, rsa_d, rsa_pm1, rsa_qm1, rsa_u;
  gcry_mpi_t phi, tmp_g, tmp_f;

  rsa_n = gcry_mpi_new (0);
  rsa_d = gcry_mpi_new (0);
  rsa_pm1 = gcry_mpi_new (0);
  rsa_qm1 = gcry_mpi_new (0);
  rsa_u = gcry_mpi_new (0);

  phi = gcry_mpi_new (0);
  tmp_f = gcry_mpi_new (0);
  tmp_g = gcry_mpi_new (0);

  /* Check that p < q; if not swap p and q.  */
  if (openpgp_mode && gcry_mpi_cmp (rsa_p, rsa_q) > 0)
    {
      fprintf (stderr, PGM ": swapping p and q\n");
      gcry_mpi_swap (rsa_p, rsa_q);
    }

  gcry_mpi_mul (rsa_n, rsa_p, rsa_q);


  /* Compute the Euler totient:  phi = (p-1)(q-1)  */
  gcry_mpi_sub_ui (rsa_pm1, rsa_p, 1);
  gcry_mpi_sub_ui (rsa_qm1, rsa_q, 1);
  gcry_mpi_mul (phi, rsa_pm1, rsa_qm1);

  if (!gcry_mpi_gcd (tmp_g, rsa_e, phi))
    die ("parameter 'e' does match 'p' and 'q'\n");

  /* Compute: f = lcm(p-1,q-1) = phi / gcd(p-1,q-1) */
  gcry_mpi_gcd (tmp_g, rsa_pm1, rsa_qm1);
  gcry_mpi_div (tmp_f, NULL, phi, tmp_g, -1);

  /* Compute the secret key:  d = e^{-1} mod lcm(p-1,q-1) */
  gcry_mpi_invm (rsa_d, rsa_e, tmp_f);

  /* Compute the CRT helpers: d mod (p-1), d mod (q-1)   */
  gcry_mpi_mod (rsa_pm1, rsa_d, rsa_pm1);
  gcry_mpi_mod (rsa_qm1, rsa_d, rsa_qm1);

  /* Compute the CRT value:   OpenPGP:    u = p^{-1} mod q
                             Standard: iqmp = q^{-1} mod p */
  if (openpgp_mode)
    gcry_mpi_invm (rsa_u, rsa_p, rsa_q);
  else
    gcry_mpi_invm (rsa_u, rsa_q, rsa_p);

  gcry_mpi_release (phi);
  gcry_mpi_release (tmp_f);
  gcry_mpi_release (tmp_g);

  /* Print everything.  */
  print_mpi_line ("n", rsa_n);
  print_mpi_line ("e", rsa_e);
  if (openpgp_mode)
    print_mpi_line ("d", rsa_d);
  print_mpi_line ("p", rsa_p);
  print_mpi_line ("q", rsa_q);
  if (openpgp_mode)
    print_mpi_line ("u", rsa_u);
  else
    {
      print_mpi_line ("dmp1", rsa_pm1);
      print_mpi_line ("dmq1", rsa_qm1);
      print_mpi_line ("iqmp", rsa_u);
    }

  gcry_mpi_release (rsa_n);
  gcry_mpi_release (rsa_d);
  gcry_mpi_release (rsa_pm1);
  gcry_mpi_release (rsa_qm1);
  gcry_mpi_release (rsa_u);
}



static void
usage (int show_help)
{
  if (!show_help)
    {
      fputs ("usage: " PGM
             " [OPTION] [FILE] (try --help for more information)\n", stderr);
      exit (2);
    }
  fputs
    ("Usage: " PGM " [OPTIONS] [FILE]\n"
     "Take RSA parameters p, n, e and compute missing parameters.\n"
     "OPTIONS:\n"
     "  --openpgp        Compute as specified by RFC4880\n"
     "  --labels         Prefix output with labels\n"
     "  --keep-lz        Keep all leading zeroes in the output\n"
     "  --verbose        Print additional information\n"
     "  --version        Print version information\n"
     "  --help           Print this text\n"
     "With no FILE, or if FILE is -, read standard input.\n"
     "Report bugs to " PACKAGE_BUGREPORT ".\n" , stdout);
  exit (0);
}


int
main (int argc, char **argv)
{
  int last_argc = -1;
  FILE *input;
  gcry_mpi_t  rsa_p, rsa_q, rsa_e;
  int got_eof;
  int any = 0;

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
      else if (!strcmp (*argv, "--help"))
        {
          usage (1);
        }
      else if (!strcmp (*argv, "--version"))
        {
          fputs (PGM " (Libgcrypt) " PACKAGE_VERSION "\n", stdout);
          printf ("libgcrypt %s\n", gcry_check_version (NULL));
          exit (0);
        }
      else if (!strcmp (*argv, "--verbose"))
        {
          verbose++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--labels"))
        {
          with_labels = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--keep-lz"))
        {
          keep_lz = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--openpgp"))
        {
          openpgp_mode = 1;
          argc--; argv++;
        }
    }

  if (argc > 1)
    usage (0);

#if !defined (HAVE_W32_SYSTEM) && !defined (_WIN32)
  signal (SIGPIPE, SIG_IGN);
#endif

  if (argc == 1 && strcmp (argv[0], "-"))
    {
      input = fopen (argv[0], "r");
      if (!input)
        die ("can't open `%s': %s\n", argv[0], strerror (errno));
    }
  else
    input = stdin;

  gcry_control (GCRYCTL_SET_VERBOSITY, (int)verbose);
  if (!gcry_check_version ("1.4.0"))
    die ("Libgcrypt is not sufficient enough\n");
  gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

  do
    {
      rsa_p = read_hexmpi_line (input, &got_eof);
      if (!rsa_p && got_eof)
        break;
      if (!rsa_p)
        die ("RSA parameter 'p' missing or not properly hex encoded\n");
      rsa_q = read_hexmpi_line (input, &got_eof);
      if (!rsa_q)
        die ("RSA parameter 'q' missing or not properly hex encoded\n");
      rsa_e = read_hexmpi_line (input, &got_eof);
      if (!rsa_e)
        die ("RSA parameter 'e' missing or not properly hex encoded\n");
      got_eof = skip_to_empty_line (input);

      if (any)
        putchar ('\n');

      compute_missing (rsa_p, rsa_q, rsa_e);

      gcry_mpi_release (rsa_p);
      gcry_mpi_release (rsa_q);
      gcry_mpi_release (rsa_e);

      any = 1;
    }
  while (!got_eof);

  return 0;
}
