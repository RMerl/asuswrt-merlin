/* Test program to test bit field operations on bit fields of large
   integer types.  */

/* This file is expected to fail to compile if the type long long int
   is not supported, but in that case it is irrelevant.  */

#include <stdlib.h>
#include <string.h>

#if !defined(__STDC__) && !defined(__cplusplus)
#define signed  /**/
#endif

struct fields
{
  unsigned long long u1 : 15;
  unsigned long long u2 : 33;
  unsigned long long u3 : 16;
  signed long long   s1 : 15;
  signed long long   s2 : 33;
  signed long long   s3 : 16;
} flags;

void break1 ()
{
}

void break2 ()
{
}

void break3 ()
{
}

void break4 ()
{
}

void break5 ()
{
}

void break6 ()
{
}

void break7 ()
{
}

void break8 ()
{
}

void break9 ()
{
}

void break10 ()
{
}

/* This is used by bitfields.exp to determine if the target understands
   signed bitfields.  */
int i;

void tester ()
{
  memset ((char *) &flags, 0, sizeof (flags));

  /* For each member, set that member to 1, allow gdb to verify that the
     member (and only that member) is 1, and then reset it back to 0. */
  flags.s1 = 1;
  break1 ();
  flags.s1 = 0;

  flags.u1 = 1;
  break1 ();
  flags.u1 = 0;

  flags.s2  = 1;
  break1 ();
  flags.s2 = 0;

  flags.u2 = 1;
  break1 ();
  flags.u2 = 0;

  flags.s3  = 1;
  break1 ();
  flags.s3 = 0;

  flags.u3 = 1;
  break1 ();
  flags.u3 = 0;

  /* Fill alternating fields with all 1's and verify that none of the bits
     "bleed over" to the other fields. */

  flags.u1 = 0x7FFF;
  flags.u3 = 0xFFFF;
  flags.s2 = -1LL;
  break2 ();
  flags.u1 = 0;
  flags.u3 = 0;
  flags.s2 = 0;

  flags.u2 = 0x1FFFFFFFFLL;
  flags.s1 = -1;
  flags.s3 = -1;
  break2 ();

  flags.u2 = 0;
  flags.s1 = 0;
  flags.s3 = 0;

  /* Fill the unsigned fields with the maximum positive value and verify
     that the values are printed correctly. */

  flags.u1 = 0x7FFF;
  flags.u2 = 0x1FFFFFFFFLL;
  flags.u3 = 0xFFFF;
  break3 ();
  flags.u1 = 0;
  flags.u2 = 0;
  flags.u3 = 0;

  /* Fill the signed fields with the maximum positive value, then the maximally
     negative value, then -1, and verify in each case that the values are
     printed correctly. */

  /* Maximum positive values */
  flags.s1 = 0x3FFF;
  flags.s2 = 0xFFFFFFFFLL;
  flags.s3 = 0x7FFF;
  break4 ();

  /* Maximally negative values */
  flags.s1 = -0x4000;
  flags.s2 = -0x100000000LL;
  flags.s3 = -0x8000;

  /* Extract bitfield value so that bitfield.exp can check if the target
     understands signed bitfields.  */
  i = flags.s3;
  break4 ();

  /* -1 */
  flags.s1 = -1;
  flags.s2 = -1;
  flags.s3 = -1;
  break4 ();

  flags.s1 = 0;
  flags.s2 = 0;
  flags.s3 = 0;

  break5 ();
}

int main () 
{
  int i;
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  for (i = 0; i < 5; i += 1)
    tester ();
  return 0;
}
