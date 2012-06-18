/* Finds Mersenne primes using the Lucas-Lehmer test 
 *
 * Tom St Denis, tomstdenis@gmail.com
 */
#include <time.h>
#include <tommath.h>

int
is_mersenne (long s, int *pp)
{
  mp_int  n, u;
  int     res, k;
  
  *pp = 0;

  if ((res = mp_init (&n)) != MP_OKAY) {
    return res;
  }

  if ((res = mp_init (&u)) != MP_OKAY) {
    goto LBL_N;
  }

  /* n = 2^s - 1 */
  if ((res = mp_2expt(&n, s)) != MP_OKAY) {
     goto LBL_MU;
  }
  if ((res = mp_sub_d (&n, 1, &n)) != MP_OKAY) {
    goto LBL_MU;
  }

  /* set u=4 */
  mp_set (&u, 4);

  /* for k=1 to s-2 do */
  for (k = 1; k <= s - 2; k++) {
    /* u = u^2 - 2 mod n */
    if ((res = mp_sqr (&u, &u)) != MP_OKAY) {
      goto LBL_MU;
    }
    if ((res = mp_sub_d (&u, 2, &u)) != MP_OKAY) {
      goto LBL_MU;
    }

    /* make sure u is positive */
    while (u.sign == MP_NEG) {
      if ((res = mp_add (&u, &n, &u)) != MP_OKAY) {
         goto LBL_MU;
      }
    }

    /* reduce */
    if ((res = mp_reduce_2k (&u, &n, 1)) != MP_OKAY) {
      goto LBL_MU;
    }
  }

  /* if u == 0 then its prime */
  if (mp_iszero (&u) == 1) {
    mp_prime_is_prime(&n, 8, pp);
  if (*pp != 1) printf("FAILURE\n");
  }

  res = MP_OKAY;
LBL_MU:mp_clear (&u);
LBL_N:mp_clear (&n);
  return res;
}

/* square root of a long < 65536 */
long
i_sqrt (long x)
{
  long    x1, x2;

  x2 = 16;
  do {
    x1 = x2;
    x2 = x1 - ((x1 * x1) - x) / (2 * x1);
  } while (x1 != x2);

  if (x1 * x1 > x) {
    --x1;
  }

  return x1;
}

/* is the long prime by brute force */
int
isprime (long k)
{
  long    y, z;

  y = i_sqrt (k);
  for (z = 2; z <= y; z++) {
    if ((k % z) == 0)
      return 0;
  }
  return 1;
}


int
main (void)
{
  int     pp;
  long    k;
  clock_t tt;

  k = 3;

  for (;;) {
    /* start time */
    tt = clock ();

    /* test if 2^k - 1 is prime */
    if (is_mersenne (k, &pp) != MP_OKAY) {
      printf ("Whoa error\n");
      return -1;
    }

    if (pp == 1) {
      /* count time */
      tt = clock () - tt;

      /* display if prime */
      printf ("2^%-5ld - 1 is prime, test took %ld ticks\n", k, tt);
    }

    /* goto next odd exponent */
    k += 2;

    /* but make sure its prime */
    while (isprime (k) == 0) {
      k += 2;
    }
  }
  return 0;
}

/* $Source: /cvs/libtom/libtommath/etc/mersenne.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2006/03/31 14:18:47 $ */
