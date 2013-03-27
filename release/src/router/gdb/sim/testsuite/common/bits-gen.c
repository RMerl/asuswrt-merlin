/* Miscellaneous simulator utilities.
   Copyright (C) 1997, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include <stdio.h>


void
gen_struct (void)
{
  printf ("\n");
  printf ("typedef struct _test_tuples {\n");
  printf ("  int line;\n");
  printf ("  int row;\n");
  printf ("  int col;\n");
  printf ("  long long val;\n");
  printf ("  long long check;\n");
  printf ("} test_tuples;\n");
  printf ("\n");
  printf ("typedef struct _test_spec {\n");
  printf ("  const char *file;\n");
  printf ("  const char *macro;\n");
  printf ("  int nr_rows;\n");
  printf ("  int nr_cols;\n");
  printf ("  test_tuples *tuples;\n");
  printf ("} test_spec;\n");
}


void
gen_bit (int bitsize,
	 int msb,
	 const char *macro,
	 int nr_bits)
{
  int i;

  printf ("\n/* Test the %s macro */\n", macro);
  printf ("test_tuples %s_tuples[%d] = {\n", macro, nr_bits);
  for (i = 0; i < nr_bits; i++)
    {
      /* compute what we think the value is */
      unsigned long long bit = 1;
      if (msb == 0)
	bit <<= nr_bits - i - 1;
      else
	bit <<= i;
      if (bitsize == 32)
	bit = (unsigned) bit; /* truncate it! */
      /* write it out */
      printf ("  { __LINE__, ");
      printf ("%d, %2d, ", -1, i);
      printf ("%s (%2d), ", macro, i);
      printf ("UNSIGNED64 (0x%08lx%08lx), ",
	      (long) (bit >> 32), (long) bit);
      printf ("},\n");
    }
  printf ("};\n");
  printf ("\n");
  printf ("test_spec %s_test = { __FILE__, \"%s\", 1, %d, %s_tuples, };\n",
	  macro, macro, nr_bits, macro);
  printf ("\n");
}


void
gen_enum (const char *macro,
	  int nr_bits)
{
  int i;

  printf ("\n/* Test the %s macro in an enum */\n", macro);
  printf ("enum enum_%s {\n", macro);
  for (i = 0; i < nr_bits; i++)
    {
      printf ("  elem_%s_%d = %s (%d),\n", macro, i, macro, i);
    }
  printf ("};\n");
  printf ("\n");
}


void
gen_mask (int bitsize,
	  const char *msb,
	  const char *macro,
	  int nr_bits)
{
  int l;
  int h;
  printf ("\n/* Test the %s%s macro */\n", msb, macro);
  printf ("test_tuples %s_tuples[%d][%d] = {\n", macro, nr_bits, nr_bits);
  for (l = 0; l < nr_bits; l++)
    {
      printf ("  {\n");
      for (h = 0; h < nr_bits; h++)
	{
	  printf ("    { __LINE__, ");
	  if ((strcmp (msb, "MS") == 0 && l <= h)
	      || (strcmp (msb, "MS") != 0 && l >= h)
	      || (strcmp (macro, "") == 0))
	    {
	      /* compute the mask */
	      unsigned long long mask = 0;
	      int b;
	      for (b = 0; b < nr_bits; b++)
		{
		  unsigned long long bit = 1;
		  if (strcmp (msb, "MS") == 0)
		    {
		      if ((l <= b && b <= h)
			  || (h < l && (b <= h || b >= l)))
			bit <<= (nr_bits - b - 1);
		      else
			bit = 0;
		    }
		  else
		    {
		      if ((l >= b && b >= h)
			  || (h > l && (b >= h || b <= l)))
			bit <<= b;
		      else
			bit = 0;
		    }
		  mask |= bit;
		}
	      if (bitsize == 32)
		mask = (unsigned long) mask;
	      printf ("%d, %d, ", l, h);
	      printf ("%s%s (%2d, %2d), ", msb, macro, l, h);
	      printf ("UNSIGNED64 (0x%08lx%08lx), ",
		      (long) (mask >> 32), (long) mask);
	    }
	  else
	    printf ("-1, -1, ");
	  printf ("},\n");
	}
      printf ("  },\n");
    }
  printf ("};\n");
  printf ("\n");
  printf ("test_spec %s_test = { __FILE__, \"%s%s\", %d, %d, &%s_tuples[0][0], };\n",
	  macro, msb, macro, nr_bits, nr_bits, macro);
  printf ("\n");
}


void
usage (int reason)
{
  fprintf (stderr, "Usage:\n");
  fprintf (stderr, "  bits-gen <nr-bits> <msb> <byte-order>\n");
  fprintf (stderr, "Generate a test case for the simulator bit manipulation code\n");
  fprintf (stderr, "  <nr-bits> = { 32 | 64 }\n");
  fprintf (stderr, "  <msb> = { 0 | { 31 | 63 } }\n");
  fprintf (stderr, "  <byte-order> = { big | little }\n");

  switch (reason)
    {
    case 1: fprintf (stderr, "Wrong number of arguments\n");
      break;
    case 2:
      fprintf (stderr, "Invalid <nr-bits> argument\n");
      break;
    case 3:
      fprintf (stderr, "Invalid <msb> argument\n");
      break;
    case 4:
      fprintf (stderr, "Invalid <byte-order> argument\n");
      break;
    default:
    }

  exit (1);
}



int
main (argc, argv)
     int argc;
     char **argv;
{
  int bitsize;
  int msb;
  char *ms;
  int big_endian;

  if (argc != 4)
    usage (1);

  if (strcmp (argv [1], "32") == 0)
    bitsize = 32;
  else if (strcmp (argv [1], "64") == 0)
    bitsize = 64;
  else
    usage (2);

  if (strcmp (argv [2], "0") == 0)
    msb = 0;
  else if (strcmp (argv [2], "31") == 0 && bitsize == 32)
    msb = 31;
  else if (strcmp (argv [2], "63") == 0 && bitsize == 64)
    msb = 63;
  else
    usage (3);
  if (msb == 0)
    ms = "MS";
  else
    ms = "LS";
  
  if (strcmp (argv [3], "big") == 0)
    big_endian = 1;
  else if (strcmp (argv [3], "little") == 0)
    big_endian = 0;
  else
    usage (4);
    
  printf ("#define WITH_TARGET_WORD_BITSIZE %d\n", bitsize);
  printf ("#define WITH_TARGET_WORD_MSB %d\n", msb);
  printf ("#define WITH_HOST_WORD_BITSIZE %d\n", sizeof (int) * 8);
  printf ("#define WITH_TARGET_BYTE_ORDER %s\n", big_endian ? "BIG_ENDIAN" : "LITTLE_ENDIAN");
  printf ("\n");
  printf ("#define SIM_BITS_INLINE (ALL_H_INLINE)\n");
  printf ("\n");
  printf ("#define ASSERT(X) do { if (!(X)) abort(); } while (0)\n");
  printf ("\n");
  printf ("#include \"sim-basics.h\"\n");

  gen_struct ();



  printf ("#define DO_BIT_TESTS\n");
  gen_bit ( 4, msb, "BIT4",  4);
  gen_bit ( 5, msb, "BIT5",  5);
  gen_bit ( 8, msb, "BIT8",  8);
  gen_bit (10, msb, "BIT10", 10);
  gen_bit (16, msb, "BIT16", 16);
  gen_bit (32, msb, "BIT32", 32);
  gen_bit (64, msb, "BIT64", 64);
  gen_bit (bitsize, msb, "BIT", 64);

  gen_bit ( 8,  8 - 1, "LSBIT8",   8);
  gen_bit (16, 16 - 1, "LSBIT16", 16);
  gen_bit (32, 32 - 1, "LSBIT32", 32);
  gen_bit (64, 64 - 1, "LSBIT64", 64);
  gen_bit (bitsize, bitsize - 1, "LSBIT",   64);

  gen_bit ( 8,  0, "MSBIT8",   8);
  gen_bit (16,  0, "MSBIT16", 16);
  gen_bit (32,  0, "MSBIT32", 32);
  gen_bit (64,  0, "MSBIT64", 64);
  gen_bit (bitsize,  0, "MSBIT",   64);

  printf ("test_spec *(bit_tests[]) = {\n");
  printf ("  &BIT4_test,\n");
  printf ("  &BIT5_test,\n");
  printf ("  &BIT8_test,\n");
  printf ("  &BIT10_test,\n");
  printf ("  &BIT16_test,\n");
  printf ("  &BIT32_test,\n");
  printf ("  &BIT64_test,\n");
  printf ("  &BIT_test,\n");
  printf ("  &LSBIT8_test,\n");
  printf ("  &LSBIT16_test,\n");
  printf ("  &LSBIT32_test,\n");
  printf ("  &LSBIT64_test,\n");
  printf ("  &LSBIT_test,\n");
  printf ("  &MSBIT8_test,\n");
  printf ("  &MSBIT16_test,\n");
  printf ("  &MSBIT32_test,\n");
  printf ("  &MSBIT64_test,\n");
  printf ("  &MSBIT_test,\n");
  printf ("  0,\n");
  printf ("};\n\n");

  gen_enum ("BIT",   64);
  gen_enum ("LSBIT", 64);
  gen_enum ("MSBIT", 64);
  gen_enum ("BIT32",   32);
  gen_enum ("LSBIT32", 32);
  gen_enum ("MSBIT32", 32);

  printf ("#define DO_MASK_TESTS\n");
  gen_mask ( 8, ms, "MASK8",  8);
  gen_mask (16, ms, "MASK16", 16);
  gen_mask (32, ms, "MASK32", 32);
  gen_mask (64, ms, "MASK64", 64);
  gen_mask (bitsize, ms, "MASK", 64);

  printf ("test_spec *(mask_tests[]) = {\n");
  printf ("  &MASK8_test,\n");
  printf ("  &MASK16_test,\n");
  printf ("  &MASK32_test,\n");
  printf ("  &MASK64_test,\n");
  printf ("  &MASK_test,\n");
  printf ("  0,\n");
  printf ("};\n\n");

  return 0;
}
