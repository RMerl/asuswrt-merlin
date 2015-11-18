/* eratosthenes.c

   An implementation of the sieve of Eratosthenes, to generate a list of primes.

   Copyright (C) 2007 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "getopt.h"

#ifdef SIZEOF_LONG
# define BITS_PER_LONG (CHAR_BIT * SIZEOF_LONG)
# if BITS_PER_LONG > 32
#  define NEED_HANDLE_LARGE_LONG 1
# else
#  define NEED_HANDLE_LARGE_LONG 0
# endif
#else
# define BITS_PER_LONG (CHAR_BIT * sizeof(unsigned long))
# define NEED_HANDLE_LARGE_LONG 1
#endif


static void
usage(void)
{
  fprintf(stderr, "Usage: erathostenes [OPTIONS] [LIMIT]\n\n"
	  "Options:\n"
	  "      -?         Display this message.\n"
	  "      -b SIZE    Block size.\n"
	  "      -v         Verbose output.\n"
	  "      -s         No output.\n");
}

static unsigned
isqrt(unsigned long n)
{
  unsigned long x;

  /* FIXME: Better initialization. */
  if (n < ULONG_MAX)
    x = n;
  else
    /* Must avoid overflow in the first step. */
    x = n-1;

  for (;;)
    {
      unsigned long y = (x + n/x) / 2;
      if (y >= x)
	return x;

      x = y;
    }
}

/* Size is in bits */
static unsigned long *
vector_alloc(unsigned long size)
{
  unsigned long end = (size + BITS_PER_LONG - 1) / BITS_PER_LONG;
  unsigned long *vector = malloc (end * sizeof(*vector));

  if (!vector)
    {
      fprintf(stderr, "Insufficient memory.\n");
      exit(EXIT_FAILURE);
    }
  return vector;
}

static void
vector_init(unsigned long *vector, unsigned long size)
{
  unsigned long end = (size + BITS_PER_LONG - 1) / BITS_PER_LONG;
  unsigned long i;

  for (i = 0; i < end; i++)
    vector[i] = ~0UL;
}

static void
vector_clear_bits (unsigned long *vector, unsigned long step,
		   unsigned long start, unsigned long size)
{
  unsigned long bit;

  for (bit = start; bit < size; bit += step)
    {
      unsigned long i = bit / BITS_PER_LONG;
      unsigned long mask = 1L << (bit % BITS_PER_LONG);

      vector[i] &= ~mask;
    }
}

static unsigned
find_first_one (unsigned long x)
{  
  static const unsigned char table[0x101] =
    {
     15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     12, 0, 0, 0, 0, 0, 0, 0,11, 0, 0, 0,10, 0, 9, 8,
      0, 0, 1, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0,
      4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      7,
    };

  unsigned i = 0;

  /* Isolate least significant bit */
  x &= -x;

#if NEED_HANDLE_LARGE_LONG
#ifndef SIZEOF_LONG
  /* Can not be tested by the preprocessor. May generate warnings
     when long is 32 bits. */
  if (BITS_PER_LONG > 32)
#endif
    while (x >= 0x100000000L)
      {
	x >>= 32;
	i += 32;
      }
#endif /* NEED_HANDLE_LARGE_LONG */

  if (x >= 0x10000)
    {
      x >>= 16;
      i += 16;
    }
  return i + table[128 + (x & 0xff) - (x >> 8)];
}

/* Returns size if there's no more bits set */
static unsigned long
vector_find_next (const unsigned long *vector, unsigned long bit, unsigned long size)
{
  unsigned long end = (size + BITS_PER_LONG - 1) / BITS_PER_LONG;
  unsigned long i = bit / BITS_PER_LONG;
  unsigned long mask = 1L << (bit % BITS_PER_LONG);
  unsigned long word;

  if (i >= end)
    return size;

  for (word = vector[i] & ~(mask - 1); !word; word = vector[i])
    if (++i >= end)
      return size;

  /* Next bit is the least significant bit of word */
  return i * BITS_PER_LONG + find_first_one(word);
}

/* For benchmarking, define to do nothing (otherwise, most of the time
   will be spent converting the output to decimal). */
#define OUTPUT(n) printf("%lu\n", (n))

static long
atosize(const char *s)
{
  char *end;
  long value = strtol(s, &end, 10);

  if (value <= 0)
    return 0;

  /* FIXME: Doesn't check for overflow. */
  switch(*end)
    {
    default:
      return 0;
    case '\0':
      break;
    case 'k': case 'K':
      value <<= 10;
      break;
    case 'M':
      value <<= 20;
      break;
    }
  return value;
}

int
main (int argc, char **argv)
{
  /* Generate all primes p <= limit */
  unsigned long limit;
  unsigned long root;

  unsigned long limit_nbits;

  /* Represents numbers up to sqrt(limit) */
  unsigned long sieve_nbits;
  unsigned long *sieve;
  /* Block for the rest of the sieving. Size should match the cache,
     the default value corresponds to 64 KB. */
  unsigned long block_nbits = 64L << 13;
  unsigned long block_start_bit;
  unsigned long *block;
  
  unsigned long bit;
  int silent = 0;
  int verbose = 0;
  int c;

  enum { OPT_HELP = 300 };
  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { "verbose", no_argument, NULL, 'v' },
      { "block-size", required_argument, NULL, 'b' },
      { "quiet", required_argument, NULL, 'q' },
      { NULL, 0, NULL, 0}
    };

  while ( (c = getopt_long(argc, argv, "svb:", options, NULL)) != -1)
    switch (c)
      {
      case OPT_HELP:
	usage();
	return EXIT_SUCCESS;
      case 'b':
	block_nbits = CHAR_BIT * atosize(optarg);
	if (!block_nbits)
	  {
	    usage();
	    return EXIT_FAILURE;
	  }
	break;

      case 'q':
	silent = 1;
	break;

      case 'v':
	verbose++;
	break;

      case '?':
	return EXIT_FAILURE;

      default:
	abort();
      }

  argc -= optind;
  argv += optind;

  if (argc == 0)
    limit = 1000;
  else if (argc == 1)
    {
      limit = atol(argv[0]);
      if (limit < 2)
	return EXIT_SUCCESS;
    }
  else
    {
      usage();
      return EXIT_FAILURE;
    }

  root = isqrt(limit);
  /* Round down to odd */
  root = (root - 1) | 1;
  /* Represents odd numbers from 3 up. */
  sieve_nbits = (root - 1) / 2;
  sieve = vector_alloc(sieve_nbits );
  vector_init(sieve, sieve_nbits);

  if (verbose)
    fprintf(stderr, "Initial sieve using %lu bits.\n", sieve_nbits);
      
  if (!silent)
    printf("2\n");

  if (limit == 2)
    return EXIT_SUCCESS;

  for (bit = 0;
       bit < sieve_nbits;
       bit = vector_find_next(sieve, bit + 1, sieve_nbits))
    {
      unsigned long n = 3 + 2 * bit;
      /* First bit to clear corresponds to n^2, which is bit

	 (n^2 - 3) / 2 = (n + 3) * bit + 3
      */      
      unsigned long n2_bit = (n+3)*bit + 3;

      if (!silent)
	printf("%lu\n", n);

      vector_clear_bits (sieve, n, n2_bit, sieve_nbits);
    }

  limit_nbits = (limit - 1) / 2;

  if (sieve_nbits + block_nbits > limit_nbits)
    block_nbits = limit_nbits - sieve_nbits;

  if (verbose)
    {
      double storage = block_nbits / 8.0;
      unsigned shift = 0;
      const char prefix[] = " KMG";

      while (storage > 1024 && shift < 3)
	{
	  storage /= 1024;
	  shift++;
	}
      fprintf(stderr, "Blockwise sieving using blocks of %lu bits (%.3g %cByte)\n",
	      block_nbits, storage, prefix[shift]);
    }

  block = vector_alloc(block_nbits);

  for (block_start_bit = bit; block_start_bit < limit_nbits; block_start_bit += block_nbits)
    {
      unsigned long block_start;
      
      if (block_start_bit + block_nbits > limit_nbits)
	block_nbits = limit_nbits - block_start_bit;

      vector_init(block, block_nbits);

      block_start = 3 + 2*block_start_bit;

      if (verbose > 1)
	fprintf(stderr, "Next block, n = %lu\n", block_start);

      /* Sieve */
      for (bit = 0; bit < sieve_nbits;
	   bit = vector_find_next(sieve, bit + 1, sieve_nbits))
	{	  
	  unsigned long n = 3 + 2 * bit;
	  unsigned long sieve_start_bit = (n + 3) * bit + 3;

	  if (sieve_start_bit < block_start_bit)
	    {
	      unsigned long k = (block_start + n - 1) / (2*n);
	      sieve_start_bit = n * k + bit;

	      assert(sieve_start_bit < block_start_bit + n);
	    }
	  assert(sieve_start_bit >= block_start_bit);

	  vector_clear_bits(block, n, sieve_start_bit - block_start_bit, block_nbits);
	}
      for (bit = vector_find_next(block, 0, block_nbits);
	   bit < block_nbits;
	   bit = vector_find_next(block, bit + 1, block_nbits))
	{
	  unsigned long n = block_start + 2 * bit;
	  if (!silent)
	    printf("%lu\n", n);
	}
    }

  free(sieve);
  free(block);

  return EXIT_SUCCESS;
}
