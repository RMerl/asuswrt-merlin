/* Tune the Karatsuba parameters
 *
 * Tom St Denis, tomstdenis@gmail.com
 */
#include <tommath.h>
#include <time.h>

/* how many times todo each size mult.  Depends on your computer.  For slow computers
 * this can be low like 5 or 10.  For fast [re: Athlon] should be 25 - 50 or so 
 */
#define TIMES (1UL<<14UL)

/* RDTSC from Scott Duplichan */
static ulong64 TIMFUNC (void)
   {
   #if defined __GNUC__
      #if defined(__i386__) || defined(__x86_64__)
         unsigned long long a;
         __asm__ __volatile__ ("rdtsc\nmovl %%eax,%0\nmovl %%edx,4+%0\n"::"m"(a):"%eax","%edx");
         return a;
      #else /* gcc-IA64 version */
         unsigned long result;
         __asm__ __volatile__("mov %0=ar.itc" : "=r"(result) :: "memory");
         while (__builtin_expect ((int) result == -1, 0))
         __asm__ __volatile__("mov %0=ar.itc" : "=r"(result) :: "memory");
         return result;
      #endif

   // Microsoft and Intel Windows compilers
   #elif defined _M_IX86
     __asm rdtsc
   #elif defined _M_AMD64
     return __rdtsc ();
   #elif defined _M_IA64
     #if defined __INTEL_COMPILER
       #include <ia64intrin.h>
     #endif
      return __getReg (3116);
   #else
     #error need rdtsc function for this build
   #endif
   }


#ifndef X86_TIMER

/* generic ISO C timer */
ulong64 LBL_T;
void t_start(void) { LBL_T = TIMFUNC(); }
ulong64 t_read(void) { return TIMFUNC() - LBL_T; }

#else
extern void t_start(void);
extern ulong64 t_read(void);
#endif

ulong64 time_mult(int size, int s)
{
  unsigned long     x;
  mp_int  a, b, c;
  ulong64 t1;

  mp_init (&a);
  mp_init (&b);
  mp_init (&c);

  mp_rand (&a, size);
  mp_rand (&b, size);

  if (s == 1) { 
      KARATSUBA_MUL_CUTOFF = size;
  } else {
      KARATSUBA_MUL_CUTOFF = 100000;
  }

  t_start();
  for (x = 0; x < TIMES; x++) {
      mp_mul(&a,&b,&c);
  }
  t1 = t_read();
  mp_clear (&a);
  mp_clear (&b);
  mp_clear (&c);
  return t1;
}

ulong64 time_sqr(int size, int s)
{
  unsigned long     x;
  mp_int  a, b;
  ulong64 t1;

  mp_init (&a);
  mp_init (&b);

  mp_rand (&a, size);

  if (s == 1) { 
      KARATSUBA_SQR_CUTOFF = size;
  } else {
      KARATSUBA_SQR_CUTOFF = 100000;
  }

  t_start();
  for (x = 0; x < TIMES; x++) {
      mp_sqr(&a,&b);
  }
  t1 = t_read();
  mp_clear (&a);
  mp_clear (&b);
  return t1;
}

int
main (void)
{
  ulong64 t1, t2;
  int x, y;

  for (x = 8; ; x += 2) { 
     t1 = time_mult(x, 0);
     t2 = time_mult(x, 1);
     printf("%d: %9llu %9llu, %9llu\n", x, t1, t2, t2 - t1);
     if (t2 < t1) break;
  }
  y = x;

  for (x = 8; ; x += 2) { 
     t1 = time_sqr(x, 0);
     t2 = time_sqr(x, 1);
     printf("%d: %9llu %9llu, %9llu\n", x, t1, t2, t2 - t1);
     if (t2 < t1) break;
  }
  printf("KARATSUBA_MUL_CUTOFF = %d\n", y);
  printf("KARATSUBA_SQR_CUTOFF = %d\n", x);

  return 0;
}

/* $Source: /cvs/libtom/libtommath/etc/tune.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2006/03/31 14:18:47 $ */
