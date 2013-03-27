#define ASSERT(EXPRESSION) \
do { \
  if (!(EXPRESSION)) { \
    fprintf (stderr, "%s:%d: assertion failed - %s\n", \
             __FILE__, __LINE__, #EXPRESSION); \
    abort (); \
  } \
} while (0)

#define SIM_BITS_INLINE (INCLUDE_MODULE | INCLUDED_BY_MODULE)

#include "milieu.h"
#include "softfloat.h"
#include "systfloat.h"
#include "systmodes.h"

/* #define SIM_FPU_INLINE (INCLUDE_MODULE | INCLUDED_BY_MODULE) */


#include "sim-bits.h"
#include "sim-fpu.h"
#include "sim-fpu.c"



static int flags;

int8
syst_float_flags_clear ()
{
  int old_flags = 0;
  int i = 1;
  while (flags >= i)
    {
      switch ((sim_fpu_status) (flags & i))
	{
	case sim_fpu_status_denorm:
	  break;
	case sim_fpu_status_invalid_snan:
	case sim_fpu_status_invalid_qnan:
	case sim_fpu_status_invalid_isi:
	case sim_fpu_status_invalid_idi:
	case sim_fpu_status_invalid_zdz:
	case sim_fpu_status_invalid_imz:
	case sim_fpu_status_invalid_cvi:
	case sim_fpu_status_invalid_cmp:
	case sim_fpu_status_invalid_sqrt:
	  old_flags |= float_flag_invalid; /* v */
	  break;
	case sim_fpu_status_inexact:
	  old_flags |= float_flag_inexact; /* x */
	  break;
	case sim_fpu_status_overflow:
	  old_flags |= float_flag_overflow; /* o */
	  break;
	case sim_fpu_status_underflow:
	  old_flags |= float_flag_underflow; /* u */
	  break;
	case sim_fpu_status_invalid_div0:
	  old_flags |= float_flag_divbyzero; /* z */
	  break;
	case sim_fpu_status_rounded:
	  break;
	}
      i <<= 1;
    }
  flags = 0;
  return old_flags;
}


sim_fpu_round rounding_mode;

void
syst_float_set_rounding_mode(int8 mode)
{
  switch (mode)
    {
    case float_round_nearest_even:
      rounding_mode = sim_fpu_round_near;
      break;
    case float_round_down:
      rounding_mode = sim_fpu_round_down;
      break;
    case float_round_up:
      rounding_mode = sim_fpu_round_up;
      break;
    case float_round_to_zero:
      rounding_mode = sim_fpu_round_zero;
      break;
    }
}


float32
syst_int32_to_float32(int32 a)
{
  float32 z;
  sim_fpu s;
  flags |= sim_fpu_i32to (&s, a, rounding_mode);
  flags |= sim_fpu_round_32 (&s, rounding_mode, 0);
  sim_fpu_to32 (&z, &s);
  return z;
}

float64
syst_int32_to_float64( int32 a )
{
  float64 z;
  sim_fpu s;
  flags |= sim_fpu_i32to (&s, a, rounding_mode);
  sim_fpu_to64 (&z, &s);
  return z;
}

int32
syst_float32_to_int32_round_to_zero( float32 a )
{
  int32 z;
  sim_fpu s;
  sim_fpu_32to (&s, a);
  flags |= sim_fpu_to32i (&z, &s, sim_fpu_round_zero);
  return z;
}

float64
syst_float32_to_float64 (float32 a)
{
  float64 z;
  sim_fpu s;
  sim_fpu_32to (&s, a);
  flags |= sim_fpu_round_64 (&s, rounding_mode, 0);
  sim_fpu_to64 (&z, &s);
  return z;
}

float32 syst_float32_add( float32 a, float32 b )
{
  float32 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
#if 0
  fprintf (stdout, "\n  ");
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n+ ");
  sim_fpu_print_fpu (&B, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n= ");
#endif
  flags |= sim_fpu_add (&ans, &A, &B);
  flags |= sim_fpu_round_32 (&ans, rounding_mode, 0);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n");
#endif
  sim_fpu_to32 (&z, &ans);
  return z;
}

float32 syst_float32_sub( float32 a, float32 b )
{
  float32 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " + ");
  sim_fpu_print_fpu (&B, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " = ");
#endif
  flags |= sim_fpu_sub (&ans, &A, &B);
  flags |= sim_fpu_round_32 (&ans, rounding_mode, 0);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n");
#endif
  sim_fpu_to32 (&z, &ans);
  return z;
}

float32 syst_float32_mul( float32 a, float32 b )
{
  float32 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " + ");
  sim_fpu_print_fpu (&B, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " = ");
#endif
  flags |= sim_fpu_mul (&ans, &A, &B);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
#endif
  flags |= sim_fpu_round_32 (&ans, rounding_mode, 0);
#if 0
  sim_fpu_print_status (flags, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n");
#endif
  sim_fpu_to32 (&z, &ans);
  return z;
}

float32 syst_float32_div( float32 a, float32 b )
{
  float32 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
  flags |= sim_fpu_div (&ans, &A, &B);
  flags |= sim_fpu_round_32 (&ans, rounding_mode, 0);
  sim_fpu_to32 (&z, &ans);
  return z;
}

float32 syst_float32_sqrt( float32 a )
{
  float32 z;
  sim_fpu A;
  sim_fpu ans;
  sim_fpu_32to (&A, a);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " sqrt> ");
#endif
  flags |= sim_fpu_sqrt (&ans, &A);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
#endif
  flags |= sim_fpu_round_32 (&ans, rounding_mode, 0);
#if 0
  fprintf (stdout, " (%x)\n", flags);
#endif
  sim_fpu_to32 (&z, &ans);
  return z;
}

flag syst_float32_eq( float32 a, float32 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
  flags |= (sim_fpu_eq (&is, &A, &B) & ~sim_fpu_status_invalid_qnan);
  return is;
}

flag syst_float32_eq_signaling( float32 a, float32 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
  flags |= sim_fpu_eq (&is, &A, &B);
  return is;
}

flag syst_float32_le( float32 a, float32 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
  flags |= sim_fpu_le (&is, &A, &B);
  return is;
}

flag syst_float32_le_quiet( float32 a, float32 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
  flags |= (sim_fpu_le (&is, &A, &B) & ~sim_fpu_status_invalid_qnan);
  return is;
}

flag syst_float32_lt( float32 a, float32 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
  flags |= sim_fpu_lt (&is, &A, &B);
  return is;
}

flag syst_float32_lt_quiet( float32 a, float32 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_32to (&A, a);
  sim_fpu_32to (&B, b);
  flags |= (sim_fpu_lt (&is, &A, &B) & ~sim_fpu_status_invalid_qnan);
  return is;
}

int32 syst_float64_to_int32_round_to_zero( float64 a )
{
  int32 z;
  sim_fpu s;
  sim_fpu_64to (&s, a);
  flags |= sim_fpu_to32i (&z, &s, sim_fpu_round_zero);
  return z;
}

float32 syst_float64_to_float32( float64 a )
{
  float32 z;
  sim_fpu s;
  sim_fpu_64to (&s, a);
#if 0
  sim_fpu_print_fpu (&s, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " -> ");
#endif
  flags |= sim_fpu_round_32 (&s, rounding_mode, 0);
#if 0
  sim_fpu_print_fpu (&s, (sim_fpu_print_func*) fprintf, stdout);
  sim_fpu_print_status (flags, (sim_fpu_print_func*) fprintf, stdout);
  printf ("\n");
#endif
  sim_fpu_to32 (&z, &s);
  return z;
}

float64 syst_float64_add( float64 a, float64 b )
{
  float64 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " + ");
  sim_fpu_print_fpu (&B, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " = ");
#endif
  flags |= sim_fpu_add (&ans, &A, &B);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
#endif
  flags |= sim_fpu_round_64 (&ans, rounding_mode, 0);
#if 0
  fprintf (stdout, " (%x)\n", flags);
#endif
  sim_fpu_to64 (&z, &ans);
  return z;
}

float64 syst_float64_sub( float64 a, float64 b )
{
  float64 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " + ");
  sim_fpu_print_fpu (&B, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " = ");
#endif
  flags |= sim_fpu_sub (&ans, &A, &B);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
#endif
  flags |= sim_fpu_round_64 (&ans, rounding_mode, 0);
#if 0
  fprintf (stdout, " (%x)\n", flags);
#endif
  sim_fpu_to64 (&z, &ans);
  return z;
}

float64 syst_float64_mul( float64 a, float64 b )
{
  float64 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " * ");
  sim_fpu_print_fpu (&B, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " = ");
#endif
  flags |= sim_fpu_mul (&ans, &A, &B);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
#endif
  flags |= sim_fpu_round_64 (&ans, rounding_mode, 0);
#if 0
  sim_fpu_print_status (flags, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n");
#endif
  sim_fpu_to64 (&z, &ans);
  return z;
}

float64 syst_float64_div( float64 a, float64 b )
{
  float64 z;
  sim_fpu A;
  sim_fpu B;
  sim_fpu ans;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " + ");
  sim_fpu_print_fpu (&B, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, " = ");
#endif
  flags |= sim_fpu_div (&ans, &A, &B);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
#endif
  flags |= sim_fpu_round_64 (&ans, rounding_mode, 0);
#if 0
  sim_fpu_print_status (flags, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n");
#endif
  sim_fpu_to64 (&z, &ans);
  return z;
}

float64 syst_float64_sqrt( float64 a )
{
  float64 z;
  sim_fpu A;
  sim_fpu ans;
  sim_fpu_64to (&A, a);
#if 0
  sim_fpu_print_fpu (&A, (sim_fpu_print_func*) fprintf, stdout);
  printf (" sqrt> ");
  printf ("\n");
#endif
  flags |= sim_fpu_sqrt (&ans, &A);
#if 0
  sim_fpu_print_fpu (&ans, (sim_fpu_print_func*) fprintf, stdout);
#endif
  flags |= sim_fpu_round_64 (&ans, rounding_mode, 0);
#if 0
  sim_fpu_print_status (flags, (sim_fpu_print_func*) fprintf, stdout);
  fprintf (stdout, "\n");
#endif
  sim_fpu_to64 (&z, &ans);
  return z;
}

flag syst_float64_eq( float64 a, float64 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
  flags |= (sim_fpu_eq (&is, &A, &B) & ~sim_fpu_status_invalid_qnan);
  return is;
}

flag syst_float64_eq_signaling( float64 a, float64 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
  flags |= sim_fpu_eq (&is, &A, &B);
  return is;
}

flag syst_float64_le( float64 a, float64 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
  flags |= sim_fpu_le (&is, &A, &B);
  return is;
}

flag syst_float64_le_quiet( float64 a, float64 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
  flags |= (sim_fpu_le (&is, &A, &B) & ~sim_fpu_status_invalid_qnan);
  return is;
}

flag syst_float64_lt( float64 a, float64 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
  flags |= sim_fpu_lt (&is, &A, &B);
  return is;
}

flag syst_float64_lt_quiet( float64 a, float64 b )
{
  sim_fpu A;
  sim_fpu B;
  int is;
  sim_fpu_64to (&A, a);
  sim_fpu_64to (&B, b);
  flags |= (sim_fpu_lt (&is, &A, &B) & ~sim_fpu_status_invalid_qnan);
  return is;
}

