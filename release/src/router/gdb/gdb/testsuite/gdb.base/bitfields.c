/* Test program to test bit field operations */

/* For non-ANSI compilers, use plain ints for the signed bit fields.  However,
   whether they actually end up signed or not is implementation defined, so
   this may cause some tests to fail.  But at least we can still compile
   the test program and run the tests... */

#if !defined(__STDC__) && !defined(__cplusplus)
#define signed  /**/
#endif

struct fields
{
  unsigned char	uc    ;
  signed int	s1 : 1;
  unsigned int	u1 : 1;
  signed int	s2 : 2;
  unsigned int	u2 : 2;
  signed int	s3 : 3;
  unsigned int	u3 : 3;
  signed int	s9 : 9;
  unsigned int  u9 : 9;
  signed char	sc    ;
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

int main ()
{
  /* For each member, set that member to 1, allow gdb to verify that the
     member (and only that member) is 1, and then reset it back to 0. */

#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  flags.uc = 1;
  break1 ();
  flags.uc = 0;

  flags.s1 = -1;
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

  flags.s9 = 1;
  break1 ();
  flags.s9 = 0;

  flags.u9 = 1;
  break1 ();
  flags.u9 = 0;

  flags.sc = 1;
  break1 ();
  flags.sc = 0;

  /* Fill alternating fields with all 1's and verify that none of the bits
     "bleed over" to the other fields. */

  flags.uc = 0xFF;
  flags.u1 = 0x1;
  flags.u2 = 0x3;
  flags.u3 = 0x7;
  flags.u9 = 0x1FF;
  break2 ();
  flags.uc = 0;
  flags.u1 = 0;
  flags.u2 = 0;
  flags.u3 = 0;
  flags.u9 = 0;

  flags.s1 = -1;
  flags.s2 = -1;
  flags.s3 = -1;
  flags.s9 = -1;
  flags.sc = 0xFF;
  break2 ();
  flags.s1 = 0;
  flags.s2 = 0;
  flags.s3 = 0;
  flags.s9 = 0;
  flags.sc = 0;

  /* Fill the unsigned fields with the maximum positive value and verify
     that the values are printed correctly. */

  /* Maximum positive values */
  flags.u1 = 0x1;
  flags.u2 = 0x3;
  flags.u3 = 0x7;
  flags.u9 = 0x1FF;
  break3 ();
  flags.u1 = 0;
  flags.u2 = 0;
  flags.u3 = 0;
  flags.u9 = 0;

  /* Fill the signed fields with the maximum positive value, then the maximally
     negative value, then -1, and verify in each case that the values are
     printed correctly. */

  /* Maximum positive values */
  flags.s1 = 0x0;
  flags.s2 = 0x1;
  flags.s3 = 0x3;
  flags.s9 = 0xFF;
  break4 ();

  /* Maximally negative values */
  flags.s1 = -0x1;
  flags.s2 = -0x2;
  flags.s3 = -0x4;
  flags.s9 = -0x100;
  /* Extract bitfield value so that bitfield.exp can check if the target
     understands signed bitfields.  */
  i = flags.s9;
  break4 ();

  /* -1 */
  flags.s1 = -1;
  flags.s2 = -1;
  flags.s3 = -1;
  flags.s9 = -1;
  break4 ();

  flags.s1 = 0;
  flags.s2 = 0;
  flags.s3 = 0;
  flags.s9 = 0;

  return 0;
}
