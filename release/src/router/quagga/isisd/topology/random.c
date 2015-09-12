/*********************************************************************/
/*                                                                   */
/* current processor time in seconds                                 */
/* difference between two calls is processor time spent by your code */
/* needs: <sys/types.h>, <sys/times.h>                               */
/* depends on compiler and OS                                        */
/*                                                                   */
/*********************************************************************/

#include <sys/types.h>
#include <sys/times.h>

/*
 * Prototypes.
 */
unsigned long timer(void);
void init_rand(long);
double rand01(void);
double randg01(void);
long nrand(long);
void free_arc(void *);

unsigned long timer ()
   { struct tms hold;

        times(&hold);
        return (unsigned long) ((float) (hold.tms_utime) / 60.0);
   }


/*********************************************************************/
/*                                                                   */
/*            Family of random number generators                     */
/*                                                                   */
/*  Initialisation:                                                  */
/*            void init_rand ( seed );                               */
/*                 long seed     - any positive number               */
/*                                 if seed<=0 init_rand takes time   */
/*                                 from timer instead of seed        */
/*                                                                   */
/*  Whole number uniformly distributed on [0,n):                     */
/*            long nrand (n);                                        */
/*                 long n                                            */
/*                                                                   */
/*  Real number uniformly distributed on [0,1]                       */
/*            double rand01();                                       */
/*                                                                   */
/*  Real number with Gauss(0,1) disitribution:                       */
/*            double randg01();                                      */
/*                                                                   */
/*  Algorithm:                                                       */
/*            x(n+1) = (x(n) * 5^13) mod 2^31                        */
/*                                                                   */
/*********************************************************************/

unsigned long internal_seed;

void init_rand ( init_seed )

long init_seed;

{ internal_seed = ( init_seed > 0 )
                 ? (unsigned long) init_seed
                 : (unsigned long) timer(); 
                

  /* only odd numbers are acceptable */
  if ( internal_seed % 2 == 0 ) internal_seed --;
}

/*********************************************************************/
/*                                                                   */
/* Internal function  irand  may depend on OS and compiler           */
/*                                                                   */
/* irand assumption:                                                 */
/* unsigned long i,j;                                                */
/*   if i*j > max(unsigned long)                                     */
/*           1. No overflow interruption                             */
/*           2. i*j = i*j mod max(unsigned long)                     */
/*                                                                   */
/* This assumption is true for a lot of computers.                   */
/* If your computer fails:                                           */
/*   rename: irand <---> xrand                                       */
/*                                                                   */
/*********************************************************************/
 
#define  A   1220703125
#define  B   2147483647
#define  BF  2147483647.

static long irand ()

{ internal_seed = ( internal_seed * A ) & B;
  return (long) internal_seed ;
}

#if 0 /* Not used. */
/*********************************************************************/
/*                                                                   */
/*              computer independent variant of  irand               */
/*                                                                   */
/*********************************************************************/


#define T15  32768 
#define T16  65536
#define A1   37252
#define A2   29589

static long xrand()

{ unsigned long is1, is2;

  is1 = internal_seed / T15;
  is2 = internal_seed % T15;

  internal_seed = ( (((is2 * A1) + (is1 * A2))% T16 )* T15 + (is2 * A2) ) & B;
  return (long) ( internal_seed ) ;
}
#endif

/*********************************************************************/


double rand01()

{ return  (double) (irand() / BF) ;
}
  
/*********************************************************************/

#define NK  12

double randg01()

{ int i;
  double sum = 0;

  for ( i = 0; i < NK; i++ ) sum += rand01();
  return sum - 6.;

  /* if   NK != 12  then you must return (12/NK)*sum - (NK/2) */
}

#undef NK
     
  
/*********************************************************************/

long nrand ( n )

long n;

{ return (long) ( rand01() * (double) n );
}

/*********************************************************************/

#undef A
#undef A1
#undef A2
#undef B
#undef BF
#undef T15
#undef T16
