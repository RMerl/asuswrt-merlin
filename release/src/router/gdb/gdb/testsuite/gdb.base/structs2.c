/* pr 13536 */

static void param_reg (register signed char pr_char,
		       register unsigned char pr_uchar,
		       register short pr_short,
		       register unsigned short pr_ushort);

int bkpt;

int
main ()
{
#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif

  bkpt = 0;
  param_reg (120, 130, 32000, 33000);
  param_reg (130, 120, 33000, 32000);

  return 0;
}

static void dummy () {}

static void
param_reg(register signed char pr_char,
	  register unsigned char pr_uchar,
	  register short pr_short,
	  register unsigned short pr_ushort)
{
  bkpt = 1;
  dummy ();
  pr_char = 1;
  pr_uchar = pr_short = pr_ushort = 1;
  dummy ();
}
