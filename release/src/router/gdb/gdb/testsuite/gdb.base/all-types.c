/*
 *	the basic C types.
 */

#if !defined (__STDC__) && !defined (_AIX)
#define signed  /**/
#endif

char		v_char;
signed char	v_signed_char;
unsigned char	v_unsigned_char;

short		v_short;
signed short	v_signed_short;
unsigned short	v_unsigned_short;

int		v_int;
signed int	v_signed_int;
unsigned int	v_unsigned_int;

long		v_long;
signed long	v_signed_long;
unsigned long	v_unsigned_long;

float		v_float;
double		v_double;

int main ()
{
    extern void dummy();
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
    dummy();
    return 0;
    
}

void dummy()
{
  /* Some linkers (e.g. on AIX) remove unreferenced variables,
     so make sure to reference them. */
  v_char = 'A';
  v_signed_char = 'B';
  v_unsigned_char = 'C';

  v_short = 3;
  v_signed_short = 4;
  v_unsigned_short = 5;    

  v_int = 6;
  v_signed_int = 7;
  v_unsigned_int = 8;    

  v_long = 9;
  v_signed_long = 10;
  v_unsigned_long = 11;    
  
  v_float = 100.343434;
  v_double = 200.565656;
}
