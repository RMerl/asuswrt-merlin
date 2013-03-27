/* Check that GDB can correctly update a value, living in a register,
   in the target.  This pretty much relies on the compiler taking heed
   of requests for values to be stored in registers.  */

/* NOTE: carlton/2002-12-05: These functions were all static, but for
   whatever reason that caused GCC 3.1 to optimize away some of the
   function calls within main even when no optimization flags were
   passed.  */

typedef signed char charest;

charest
add_charest (register charest u, register charest v)
{
  return u + v;
}

short
add_short (register short u, register short v)
{
  return u + v;
}

int
add_int (register int u, register int v)
{
  return u + v;
}

long
add_long (register long u, register long v)
{
  return u + v;
}

typedef long long longest;

longest
add_longest (register longest u, register longest v)
{
  return u + v;
}

float
add_float (register float u, register float v)
{
  return u + v;
}

double
add_double (register double u, register double v)
{
  return u + v;
}

typedef long double doublest;

doublest
add_doublest (register doublest u, register doublest v)
{
  return u + v;
}

/* */

charest
wack_charest (register charest u, register charest v)
{
  register charest l = u, r = v;
  l = add_charest (l, r);
  return l + r;
}

short
wack_short (register short u, register short v)
{
  register short l = u, r = v;
  l = add_short (l, r);
  return l + r;
}

int
wack_int (register int u, register int v)
{
  register int l = u, r = v;
  l = add_int (l, r);
  return l + r;
}

long
wack_long (register long u, register long v)
{
  register long l = u, r = v;
  l = add_long (l, r);
  return l + r;
}

long
wack_longest (register longest u, register longest v)
{
  register longest l = u, r = v;
  l = add_longest (l, r);
  return l + r;
}

float
wack_float (register float u, register float v)
{
  register float l = u, r = v;
  l = add_float (l, r);
  return l + r;
}

double
wack_double (register double u, register double v)
{
  register double l = u, r = v;
  l = add_double (l, r);
  return l + r;
}

doublest
wack_doublest (register doublest u, register doublest v)
{
  register doublest l = u, r = v;
  l = add_doublest (l, r);
  return l + r;
}

/* */

struct s_1 { short s[1]; } z_1, s_1;
struct s_2 { short s[2]; } z_2, s_2;
struct s_3 { short s[3]; } z_3, s_3;
struct s_4 { short s[4]; } z_4, s_4;

struct s_1
add_struct_1 (struct s_1 s)
{
  int i;
  for (i = 0; i < sizeof (s) / sizeof (s.s[0]); i++)
    {
      s.s[i] = s.s[i] + s.s[i];
    }
  return s;
}

struct s_2
add_struct_2 (struct s_2 s)
{
  int i;
  for (i = 0; i < sizeof (s) / sizeof (s.s[0]); i++)
    {
      s.s[i] = s.s[i] + s.s[i];
    }
  return s;
}

struct s_3
add_struct_3 (struct s_3 s)
{
  int i;
  for (i = 0; i < sizeof (s) / sizeof (s.s[0]); i++)
    {
      s.s[i] = s.s[i] + s.s[i];
    }
  return s;
}

struct s_4
add_struct_4 (struct s_4 s)
{
  int i;
  for (i = 0; i < sizeof (s) / sizeof (s.s[0]); i++)
    {
      s.s[i] = s.s[i] + s.s[i];
    }
  return s;
}

struct s_1
wack_struct_1 (void)
{
  int i; register struct s_1 u = z_1;
  for (i = 0; i < sizeof (s_1) / sizeof (s_1.s[0]); i++) { s_1.s[i] = i + 1; }
  u = add_struct_1 (u);
  return u;
}

struct s_2
wack_struct_2 (void)
{
  int i; register struct s_2 u = z_2;
  for (i = 0; i < sizeof (s_2) / sizeof (s_2.s[0]); i++) { s_2.s[i] = i + 1; }
  u = add_struct_2 (u);
  return u;
}

struct s_3
wack_struct_3 (void)
{
  int i; register struct s_3 u = z_3;
  for (i = 0; i < sizeof (s_3) / sizeof (s_3.s[0]); i++) { s_3.s[i] = i + 1; }
  u = add_struct_3 (u);
  return u;
}

struct s_4
wack_struct_4 (void)
{
  int i; register struct s_4 u = z_4;
  for (i = 0; i < sizeof (s_4) / sizeof (s_4.s[0]); i++) { s_4.s[i] = i + 1; }
  u = add_struct_4 (u);
  return u;
}

/* */

struct f_1 {unsigned i:1;unsigned j:1;unsigned k:1; } f_1 = {1,1,1}, F_1;
struct f_2 {unsigned i:2;unsigned j:2;unsigned k:2; } f_2 = {1,1,1}, F_2;
struct f_3 {unsigned i:3;unsigned j:3;unsigned k:3; } f_3 = {1,1,1}, F_3;
struct f_4 {unsigned i:4;unsigned j:4;unsigned k:4; } f_4 = {1,1,1}, F_4;

struct f_1
wack_field_1 (void)
{
  register struct f_1 u = f_1;
  return u;
}

struct f_2
wack_field_2 (void)
{
  register struct f_2 u = f_2;
  return u;
}

struct f_3
wack_field_3 (void)
{
  register struct f_3 u = f_3;
  return u;
}

struct f_4
wack_field_4 (void)
{
  register struct f_4 u = f_4;
  return u;
}

/* */

int
main ()
{
  /* These calls are for current frame test.  */
  wack_charest (-1, -2);
  wack_short (-1, -2);
  wack_int (-1, -2);
  wack_long (-1, -2);
  wack_longest (-1, -2);
  wack_float (-1, -2);
  wack_double (-1, -2);
  wack_doublest (-1, -2);

  /* These calls are for up frame.  */
  wack_charest (-1, -2);
  wack_short (-1, -2);
  wack_int (-1, -2);
  wack_long (-1, -2);
  wack_longest (-1, -2);
  wack_float (-1, -2);
  wack_double (-1, -2);
  wack_doublest (-1, -2);

  /* These calls are for current frame test.  */
  wack_struct_1 ();
  wack_struct_2 ();
  wack_struct_3 ();
  wack_struct_4 ();
  
  /* These calls are for up frame.  */
  wack_struct_1 ();
  wack_struct_2 ();
  wack_struct_3 ();
  wack_struct_4 ();
  
  wack_field_1 ();
  wack_field_2 ();
  wack_field_3 ();
  wack_field_4 ();

  return 0;
}
