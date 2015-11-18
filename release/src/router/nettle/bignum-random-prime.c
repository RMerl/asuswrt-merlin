/* bignum-random-prime.c

   Generation of random provable primes.

   Copyright (C) 2010, 2013 Niels Möller

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

#ifndef RANDOM_PRIME_VERBOSE
#define RANDOM_PRIME_VERBOSE 0
#endif

#include <assert.h>
#include <stdlib.h>

#if RANDOM_PRIME_VERBOSE
#include <stdio.h>
#define VERBOSE(x) (fputs((x), stderr))
#else
#define VERBOSE(x)
#endif

#include "bignum.h"

#include "macros.h"

/* Use a table of p_2 = 3 to p_{172} = 1021, used for sieving numbers
   of up to 20 bits. */

#define NPRIMES 171
#define TRIAL_DIV_BITS 20
#define TRIAL_DIV_MASK ((1 << TRIAL_DIV_BITS) - 1)

/* A 20-bit number x is divisible by p iff

     ((x * inverse) & TRIAL_DIV_MASK) <= limit
*/
struct trial_div_info {
  uint32_t inverse; /* p^{-1} (mod 2^20) */
  uint32_t limit;   /* floor( (2^20 - 1) / p) */
};

static const uint16_t
primes[NPRIMES] = {
  3,5,7,11,13,17,19,23,
  29,31,37,41,43,47,53,59,
  61,67,71,73,79,83,89,97,
  101,103,107,109,113,127,131,137,
  139,149,151,157,163,167,173,179,
  181,191,193,197,199,211,223,227,
  229,233,239,241,251,257,263,269,
  271,277,281,283,293,307,311,313,
  317,331,337,347,349,353,359,367,
  373,379,383,389,397,401,409,419,
  421,431,433,439,443,449,457,461,
  463,467,479,487,491,499,503,509,
  521,523,541,547,557,563,569,571,
  577,587,593,599,601,607,613,617,
  619,631,641,643,647,653,659,661,
  673,677,683,691,701,709,719,727,
  733,739,743,751,757,761,769,773,
  787,797,809,811,821,823,827,829,
  839,853,857,859,863,877,881,883,
  887,907,911,919,929,937,941,947,
  953,967,971,977,983,991,997,1009,
  1013,1019,1021,
};

static const uint32_t
prime_square[NPRIMES+1] = {
  9,25,49,121,169,289,361,529,
  841,961,1369,1681,1849,2209,2809,3481,
  3721,4489,5041,5329,6241,6889,7921,9409,
  10201,10609,11449,11881,12769,16129,17161,18769,
  19321,22201,22801,24649,26569,27889,29929,32041,
  32761,36481,37249,38809,39601,44521,49729,51529,
  52441,54289,57121,58081,63001,66049,69169,72361,
  73441,76729,78961,80089,85849,94249,96721,97969,
  100489,109561,113569,120409,121801,124609,128881,134689,
  139129,143641,146689,151321,157609,160801,167281,175561,
  177241,185761,187489,192721,196249,201601,208849,212521,
  214369,218089,229441,237169,241081,249001,253009,259081,
  271441,273529,292681,299209,310249,316969,323761,326041,
  332929,344569,351649,358801,361201,368449,375769,380689,
  383161,398161,410881,413449,418609,426409,434281,436921,
  452929,458329,466489,477481,491401,502681,516961,528529,
  537289,546121,552049,564001,573049,579121,591361,597529,
  619369,635209,654481,657721,674041,677329,683929,687241,
  703921,727609,734449,737881,744769,769129,776161,779689,
  786769,822649,829921,844561,863041,877969,885481,896809,
  908209,935089,942841,954529,966289,982081,994009,1018081,
  1026169,1038361,1042441,1L<<20
};

static const struct trial_div_info
trial_div_table[NPRIMES] = {
  {699051,349525},{838861,209715},{748983,149796},{953251,95325},
  {806597,80659},{61681,61680},{772635,55188},{866215,45590},
  {180789,36157},{1014751,33825},{793517,28339},{1023001,25575},
  {48771,24385},{870095,22310},{217629,19784},{710899,17772},
  {825109,17189},{281707,15650},{502135,14768},{258553,14364},
  {464559,13273},{934875,12633},{1001449,11781},{172961,10810},
  {176493,10381},{203607,10180},{568387,9799},{788837,9619},
  {770193,9279},{1032063,8256},{544299,8004},{619961,7653},
  {550691,7543},{182973,7037},{229159,6944},{427445,6678},
  {701195,6432},{370455,6278},{90917,6061},{175739,5857},
  {585117,5793},{225087,5489},{298817,5433},{228877,5322},
  {442615,5269},{546651,4969},{244511,4702},{83147,4619},
  {769261,4578},{841561,4500},{732687,4387},{978961,4350},
  {133683,4177},{65281,4080},{629943,3986},{374213,3898},
  {708079,3869},{280125,3785},{641833,3731},{618771,3705},
  {930477,3578},{778747,3415},{623751,3371},{40201,3350},
  {122389,3307},{950371,3167},{1042353,3111},{18131,3021},
  {285429,3004},{549537,2970},{166487,2920},{294287,2857},
  {919261,2811},{636339,2766},{900735,2737},{118605,2695},
  {10565,2641},{188273,2614},{115369,2563},{735755,2502},
  {458285,2490},{914767,2432},{370513,2421},{1027079,2388},
  {629619,2366},{462401,2335},{649337,2294},{316165,2274},
  {484655,2264},{65115,2245},{326175,2189},{1016279,2153},
  {990915,2135},{556859,2101},{462791,2084},{844629,2060},
  {404537,2012},{457123,2004},{577589,1938},{638347,1916},
  {892325,1882},{182523,1862},{1002505,1842},{624371,1836},
  {69057,1817},{210787,1786},{558769,1768},{395623,1750},
  {992745,1744},{317855,1727},{384877,1710},{372185,1699},
  {105027,1693},{423751,1661},{408961,1635},{908331,1630},
  {74551,1620},{36933,1605},{617371,1591},{506045,1586},
  {24929,1558},{529709,1548},{1042435,1535},{31867,1517},
  {166037,1495},{928781,1478},{508975,1458},{4327,1442},
  {779637,1430},{742091,1418},{258263,1411},{879631,1396},
  {72029,1385},{728905,1377},{589057,1363},{348621,1356},
  {671515,1332},{710453,1315},{84249,1296},{959363,1292},
  {685853,1277},{467591,1274},{646643,1267},{683029,1264},
  {439927,1249},{254461,1229},{660713,1223},{554195,1220},
  {202911,1215},{753253,1195},{941457,1190},{776635,1187},
  {509511,1182},{986147,1156},{768879,1151},{699431,1140},
  {696417,1128},{86169,1119},{808997,1114},{25467,1107},
  {201353,1100},{708087,1084},{1018339,1079},{341297,1073},
  {434151,1066},{96287,1058},{950765,1051},{298257,1039},
  {675933,1035},{167731,1029},{815445,1027},
};

/* Element j gives the index of the first prime of size 3+j bits */
static uint8_t
prime_by_size[9] = {
  1,3,5,10,17,30,53,96,171
};

/* Combined Miller-Rabin test to the base a, and checking the
   conditions from Pocklington's theorem, nm1dq holds (n-1)/q, with q
   prime. */
static int
miller_rabin_pocklington(mpz_t n, mpz_t nm1, mpz_t nm1dq, mpz_t a)
{
  mpz_t r;
  mpz_t y;
  int is_prime = 0;

  /* Avoid the mp_bitcnt_t type for compatibility with older GMP
     versions. */
  unsigned k;
  unsigned j;

  VERBOSE(".");

  if (mpz_even_p(n) || mpz_cmp_ui(n, 3) < 0)
    return 0;

  mpz_init(r);
  mpz_init(y);

  k = mpz_scan1(nm1, 0);
  assert(k > 0);

  mpz_fdiv_q_2exp (r, nm1, k);

  mpz_powm(y, a, r, n);

  if (mpz_cmp_ui(y, 1) == 0 || mpz_cmp(y, nm1) == 0)
    goto passed_miller_rabin;
    
  for (j = 1; j < k; j++)
    {
      mpz_powm_ui (y, y, 2, n);

      if (mpz_cmp_ui (y, 1) == 0)
	break;

      if (mpz_cmp (y, nm1) == 0)
	{
	passed_miller_rabin:
	  /* We know that a^{n-1} = 1 (mod n)

	     Remains to check that gcd(a^{(n-1)/q} - 1, n) == 1 */      
	  VERBOSE("x");

	  mpz_powm(y, a, nm1dq, n);
	  mpz_sub_ui(y, y, 1);
	  mpz_gcd(y, y, n);
	  is_prime = mpz_cmp_ui (y, 1) == 0;
	  VERBOSE(is_prime ? "\n" : "");
	  break;
	}

    }

  mpz_clear(r);
  mpz_clear(y);

  return is_prime;
}

/* The most basic variant of Pocklingtons theorem:

   Assume that q^e | (n-1), with q prime. If we can find an a such that

     a^{n-1} = 1 (mod n)
     gcd(a^{(n-1)/q} - 1, n) = 1

   then any prime divisor p of n satisfies p = 1 (mod q^e).

   Proof (Cohen, 8.3.2): Assume p is a prime factor of n. The central
   idea of the proof is to consider the order, modulo p, of a. Denote
   this by d.

   a^{n-1} = 1 (mod n) implies a^{n-1} = 1 (mod p), hence d | (n-1).
   Next, the condition gcd(a^{(n-1)/q} - 1, n) = 1 implies that
   a^{(n-1)/q} != 1, hence d does not divide (n-1)/q. Since q is
   prime, this means that q^e | d.

   Finally, we have a^{p-1} = 1 (mod p), hence d | (p-1). So q^e | d |
   (p-1), which gives the desired result: p = 1 (mod q^e).


   * Variant, slightly stronger than Fact 4.59, HAC:

   Assume n = 1 + 2rq, q an odd prime, r <= 2q, and 

     a^{n-1} = 1 (mod n)
     gcd(a^{(n-1)/q} - 1, n) = 1

   Then n is prime.

   Proof: By Pocklington's theorem, any prime factor p satisfies p = 1
   (mod q). Neither 1 or q+1 are primes, hence p >= 1 + 2q. If n is
   composite, we have n >= (1+2q)^2. But the assumption r <= 2q
   implies n <= 1 + 4q^2, a contradiction.

   In bits, the requirement is that #n <= 2 #q, then

     r = (n-1)/2q < 2^{#n - #q} <= 2^#q = 2 2^{#q-1}< 2 q


   * Another variant with an extra test (Variant of Fact 4.42, HAC):

   Assume n = 1 + 2rq, n odd, q an odd prime, 8 q^3 >= n

     a^{n-1} = 1 (mod n)
     gcd(a^{(n-1)/q} - 1, n) = 1

   Also let x = floor(r / 2q), y = r mod 2q, 

   If y^2 - 4x is not a square, then n is prime.

   Proof (adapted from Maurer, Journal of Cryptology, 8 (1995)):

   Assume n is composite. There are at most two factors, both odd,

     n = (1+2m_1 q)(1+2m_2 q) = 1 + 4 m_1 m_2 q^2 + 2 (m_1 + m_2) q
     
   where we can assume m_1 >= m_2. Then the bound n <= 8 q^3 implies m_1
   m_2 < 2q, restricting (m_1, m_2) to the domain 0 < m_2 <
   sqrt(2q), 0 < m_1 < 2q / m_2.

   We have the bound

     m_1 + m_2 < 2q / m_2 + m_2 <= 2q + 1 (maximum value for m_2 = 1)

   And the case m_1 = 2q, m_2 = 1 can be excluded, because it gives n
   > 8q^3. So in fact, m_1 + m_2 < 2q.

   Next, write r = (n-1)/2q = 2 m_1 m_2 q + m_1 + m_2.
   
   If follows that m_1 + m_2 = y and m_1 m_2 = x. m_1 and m_2 are
   thus the roots of the equation

     m^2 - y m + x = 0

   which has integer roots iff y^2 - 4 x is the square of an integer.

   In bits, the requirement is that #n <= 3 #q, then

     n < 2^#n <= 2^{3 #q} = 8 2^{3 (#q-1)} < 8 q^3
*/

/* Generate a prime number p of size bits with 2 p0q dividing (p-1).
   p0 must be of size >= ceil(bits/3). The extra factor q can be
   omitted (then p0 and p0q should be equal). If top_bits_set is one,
   the topmost two bits are set to one, suitable for RSA primes. Also
   returns r = (p-1)/p0q. */
void
_nettle_generate_pocklington_prime (mpz_t p, mpz_t r,
				    unsigned bits, int top_bits_set, 
				    void *ctx, nettle_random_func *random, 
				    const mpz_t p0,
				    const mpz_t q,
				    const mpz_t p0q)
{
  mpz_t r_min, r_range, pm1, a, e;
  int need_square_test;
  unsigned p0_bits;
  mpz_t x, y, p04;

  p0_bits = mpz_sizeinbase (p0, 2);

  assert (bits <= 3*p0_bits);
  assert (bits > p0_bits);

  need_square_test = (bits > 2 * p0_bits);

  mpz_init (r_min);
  mpz_init (r_range);
  mpz_init (pm1);
  mpz_init (a);

  if (need_square_test)
    {
      mpz_init (x);
      mpz_init (y);
      mpz_init (p04);
      mpz_mul_2exp (p04, p0, 2);
    }

  if (q)
    mpz_init (e);

  if (top_bits_set)
    {
      /* i = floor (2^{bits-3} / p0q), then 3I + 3 <= r <= 4I, with I
	 - 2 possible values. */
      mpz_set_ui (r_min, 1);
      mpz_mul_2exp (r_min, r_min, bits-3);
      mpz_fdiv_q (r_min, r_min, p0q);
      mpz_sub_ui (r_range, r_min, 2);
      mpz_mul_ui (r_min, r_min, 3);
      mpz_add_ui (r_min, r_min, 3);
    }
  else
    {
      /* i = floor (2^{bits-2} / p0q), I + 1 <= r <= 2I */
      mpz_set_ui (r_range, 1);
      mpz_mul_2exp (r_range, r_range, bits-2);
      mpz_fdiv_q (r_range, r_range, p0q);
      mpz_add_ui (r_min, r_range, 1);
    }

  for (;;)
    {
      uint8_t buf[1];

      nettle_mpz_random (r, ctx, random, r_range);
      mpz_add (r, r, r_min);

      /* Set p = 2*r*p0q + 1 */
      mpz_mul_2exp(r, r, 1);
      mpz_mul (pm1, r, p0q);
      mpz_add_ui (p, pm1, 1);

      assert(mpz_sizeinbase(p, 2) == bits);

      /* Should use GMP trial division interface when that
	 materializes, we don't need any testing beyond trial
	 division. */
      if (!mpz_probab_prime_p (p, 1))
	continue;

      random(ctx, sizeof(buf), buf);
	  
      mpz_set_ui (a, buf[0] + 2);

      if (q)
	{
	  mpz_mul (e, r, q);
	  if (!miller_rabin_pocklington(p, pm1, e, a))
	    continue;

	  if (need_square_test)
	    {
	      /* Our e corresponds to 2r in the theorem */
	      mpz_tdiv_qr (x, y, e, p04);
	      goto square_test;
	    }
	}
      else
	{
	  if (!miller_rabin_pocklington(p, pm1, r, a))
	    continue;
	  if (need_square_test)
	    {
	      mpz_tdiv_qr (x, y, r, p04);
	    square_test:
	      /* We have r' = 2r, x = floor (r/2q) = floor(r'/2q),
		 and y' = r' - x 4q = 2 (r - x 2q) = 2y.

		 Then y^2 - 4x is a square iff y'^2 - 16 x is a
		 square. */
		 
	      mpz_mul (y, y, y);
	      mpz_submul_ui (y, x, 16);
	      if (mpz_perfect_square_p (y))
		continue;
	    }
	}

      /* If we passed all the tests, we have found a prime. */
      break;
    }
  mpz_clear (r_min);
  mpz_clear (r_range);
  mpz_clear (pm1);
  mpz_clear (a);

  if (need_square_test)
    {
      mpz_clear (x);
      mpz_clear (y);
      mpz_clear (p04);
    }
  if (q)
    mpz_clear (e);
}

/* Generate random prime of a given size. Maurer's algorithm (Alg.
   6.42 Handbook of applied cryptography), but with ratio = 1/2 (like
   the variant in fips186-3). */
void
nettle_random_prime(mpz_t p, unsigned bits, int top_bits_set,
		    void *random_ctx, nettle_random_func *random,
		    void *progress_ctx, nettle_progress_func *progress)
{
  assert (bits >= 3);
  if (bits <= 10)
    {
      unsigned first;
      unsigned choices;
      uint8_t buf;

      assert (!top_bits_set);

      random (random_ctx, sizeof(buf), &buf);

      first = prime_by_size[bits-3];
      choices = prime_by_size[bits-2] - first;
      
      mpz_set_ui (p, primes[first + buf % choices]);
    }
  else if (bits <= 20)
    {
      unsigned long highbit;
      uint8_t buf[3];
      unsigned long x;
      unsigned j;
      
      assert (!top_bits_set);

      highbit = 1L << (bits - 1);

    again:
      random (random_ctx, sizeof(buf), buf);
      x = READ_UINT24(buf);
      x &= (highbit - 1);
      x |= highbit | 1;

      for (j = 0; prime_square[j] <= x; j++)
	{
	  unsigned q = x * trial_div_table[j].inverse & TRIAL_DIV_MASK;
	  if (q <= trial_div_table[j].limit)
	    goto again;
	}
      mpz_set_ui (p, x);
    }
  else
    {
      mpz_t q, r;

      mpz_init (q);
      mpz_init (r);

     /* Bit size ceil(k/2) + 1, slightly larger than used in Alg. 4.62
	in Handbook of Applied Cryptography (which seems to be
	incorrect for odd k). */
      nettle_random_prime (q, (bits+3)/2, 0, random_ctx, random,
			   progress_ctx, progress);

      _nettle_generate_pocklington_prime (p, r, bits, top_bits_set,
					  random_ctx, random,
					  q, NULL, q);
      
      if (progress)
	progress (progress_ctx, 'x');

      mpz_clear (q);
      mpz_clear (r);
    }
}
