/* This test program is part of GDB, the GNU debugger.

   Copyright 1992, 1993, 1994, 1997, 1999, 2004, 2007
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   */

/*
 *	Test file with lots of different types, for testing the
 *	"whatis" command.
 */

/*
 *	First the basic C types.
 */

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

/*
 *	Now some derived types, which are arrays, functions-returning,
 *	pointers, structures, unions, and enumerations.
 */

/**** arrays *******/

char		v_char_array[2];
signed char	v_signed_char_array[2];
unsigned char	v_unsigned_char_array[2];

short		v_short_array[2];
signed short	v_signed_short_array[2];
unsigned short	v_unsigned_short_array[2];

int		v_int_array[2];
signed int	v_signed_int_array[2];
unsigned int	v_unsigned_int_array[2];

long		v_long_array[2];
signed long	v_signed_long_array[2];
unsigned long	v_unsigned_long_array[2];

float		v_float_array[2];
double		v_double_array[2];

/**** pointers *******/

/* Make sure they still print as pointer to foo even there is a typedef
   for that type.  Test this not just for char *, which might be
   a special case kludge in GDB (Unix system include files like to define
   caddr_t), but for a variety of types.  */
typedef char *char_addr;
static char_addr a_char_addr;
typedef unsigned short *ushort_addr;
static ushort_addr a_ushort_addr;
typedef signed long *slong_addr;
static slong_addr a_slong_addr;

char		*v_char_pointer;
signed char	*v_signed_char_pointer;
unsigned char	*v_unsigned_char_pointer;

short		*v_short_pointer;
signed short	*v_signed_short_pointer;
unsigned short	*v_unsigned_short_pointer;

int		*v_int_pointer;
signed int	*v_signed_int_pointer;
unsigned int	*v_unsigned_int_pointer;

long		*v_long_pointer;
signed long	*v_signed_long_pointer;
unsigned long	*v_unsigned_long_pointer;

float		*v_float_pointer;
double		*v_double_pointer;

/**** structs *******/

struct t_struct {
    char	v_char_member;
    short	v_short_member;
    int		v_int_member;
    long	v_long_member;
    float	v_float_member;
    double	v_double_member;
} v_struct1;

struct {
    char	v_char_member;
    short	v_short_member;
    int		v_int_member;
    long	v_long_member;
    float	v_float_member;
    double	v_double_member;
} v_struct2;

/**** unions *******/

union t_union {
    char	v_char_member;
    short	v_short_member;
    int		v_int_member;
    long	v_long_member;
    float	v_float_member;
    double	v_double_member;
} v_union;

union {
    char	v_char_member;
    short	v_short_member;
    int		v_int_member;
    long	v_long_member;
    float	v_float_member;
    double	v_double_member;
} v_union2;

/*** Functions returning type ********/

char		v_char_func () { return(0); }
signed char	v_signed_char_func () { return (0); }
unsigned char	v_unsigned_char_func () { return (0); }

short		v_short_func () { return (0); }
signed short	v_signed_short_func () { return (0); }
unsigned short	v_unsigned_short_func () { return (0); }

int		v_int_func () { return (0); }
signed int	v_signed_int_func () { return (0); }
unsigned int	v_unsigned_int_func () { return (0); }

long		v_long_func () { return (0); }
signed long	v_signed_long_func () { return (0); }
unsigned long	v_unsigned_long_func () { return (0); }

float		v_float_func () { return (0.0); }
double		v_double_func () { return (0.0); }

/**** Some misc more complicated things *******/

struct link {
	struct link *next;
#ifdef __STDC__
	struct link *(*linkfunc) (struct link *this, int flags);
#else
	struct link *(*linkfunc) ();
#endif
	struct t_struct stuff[1][2][3];
} *s_link;

union tu_link {
	struct link *next;
#ifdef __STDC__
	struct link *(*linkfunc) (struct link *this, int flags);
#else
	struct link *(*linkfunc) ();
#endif
	struct t_struct stuff[1][2][3];
} u_link;

struct outer_struct {
	int outer_int;
	struct inner_struct {
		int inner_int;
		long inner_long;
	}inner_struct_instance;
	union inner_union {
		int inner_union_int;
		long inner_union_long;
	}inner_union_instance;
	long outer_long;
} nested_su;

/**** Enumerations *******/

enum colors {red, green, blue} color;
enum cars {chevy, ford, porsche} clunker;

/***********/

int main ()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  /* Some linkers (e.g. on AIX) remove unreferenced variables,
     so make sure to reference them. */
  v_char = 0;
  v_signed_char = 1;
  v_unsigned_char = 2;

  v_short = 3;
  v_signed_short = 4;
  v_unsigned_short = 5;    

  v_int = 6;
  v_signed_int = 7;
  v_unsigned_int = 8;    

  v_long = 9;
  v_signed_long = 10;
  v_unsigned_long = 11;    
  
  v_float = 100.0;
  v_double = 200.0;


  v_char_array[0] = v_char;
  v_signed_char_array[0] = v_signed_char;
  v_unsigned_char_array[0] = v_unsigned_char;

  v_short_array[0] = v_short;
  v_signed_short_array[0] = v_signed_short;
  v_unsigned_short_array[0] = v_unsigned_short;

  v_int_array[0] = v_int;
  v_signed_int_array[0] = v_signed_int;
  v_unsigned_int_array[0] = v_unsigned_int;

  v_long_array[0] = v_long;
  v_signed_long_array[0] = v_signed_long;
  v_unsigned_long_array[0] = v_unsigned_long;

  v_float_array[0] = v_float;
  v_double_array[0] = v_double;

  v_char_pointer = &v_char;
  v_signed_char_pointer = &v_signed_char;
  v_unsigned_char_pointer = &v_unsigned_char;

  v_short_pointer = &v_short;
  v_signed_short_pointer = &v_signed_short;
  v_unsigned_short_pointer = &v_unsigned_short;

  v_int_pointer = &v_int;
  v_signed_int_pointer = &v_signed_int;
  v_unsigned_int_pointer = &v_unsigned_int;

  v_long_pointer = &v_long;
  v_signed_long_pointer = &v_signed_long;
  v_unsigned_long_pointer = &v_unsigned_long;

  v_float_pointer = &v_float;
  v_double_pointer = &v_double;

  color = red;
  clunker = porsche;

  u_link.next = s_link;

  v_union2.v_short_member = v_union.v_short_member;

  v_struct1.v_char_member = 0;
  v_struct2.v_char_member = 0;

  nested_su.outer_int = 0;
  return 0;
}
