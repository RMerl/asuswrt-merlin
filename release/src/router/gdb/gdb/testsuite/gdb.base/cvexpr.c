/* Copyright (C) 2001, 2004, 2007 Free Software Foundation, Inc.

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

   Please email any bugs, comments, and/or additions to this file to:
   bug-gdb@prep.ai.mit.edu  */


/*
 * Initial set of typed variables borrowed from ptype.c
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

long long	   v_long_long;
signed long long   v_signed_long_long;
unsigned long long v_unsigned_long_long;

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

/* PR 3742 */
typedef char t_char_array[];

/**** pointers *******/

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

char		**v_char_pointer_pointer;
signed char	**v_signed_char_pointer_pointer;
unsigned char	**v_unsigned_char_pointer_pointer;

short		**v_short_pointer_pointer;
signed short	**v_signed_short_pointer_pointer;
unsigned short	**v_unsigned_short_pointer_pointer;

int		**v_int_pointer_pointer;
signed int	**v_signed_int_pointer_pointer;
unsigned int	**v_unsigned_int_pointer_pointer;

long		**v_long_pointer_pointer;
signed long	**v_signed_long_pointer_pointer;
unsigned long	**v_unsigned_long_pointer_pointer;

float		**v_float_pointer_pointer;
double		**v_double_pointer_pointer;

/**** pointers to arrays, arrays of pointers *******/

char		*v_char_array_pointer[2];
signed char	*v_signed_char_array_pointer[2];
unsigned char	*v_unsigned_char_array_pointer[2];

short		*v_short_array_pointer[2];
signed short	*v_signed_short_array_pointer[2];
unsigned short	*v_unsigned_short_array_pointer[2];

int		*v_int_array_pointer[2];
signed int	*v_signed_int_array_pointer[2];
unsigned int	*v_unsigned_int_array_pointer[2];

long		*v_long_array_pointer[2];
signed long	*v_signed_long_array_pointer[2];
unsigned long	*v_unsigned_long_array_pointer[2];

float		*v_float_array_pointer[2];
double		*v_double_array_pointer[2];

char		(*v_char_pointer_array)[2];
signed char	(*v_signed_char_pointer_array)[2];
unsigned char	(*v_unsigned_char_pointer_array)[2];

short		(*v_short_pointer_array)[2];
signed short	(*v_signed_short_pointer_array)[2];
unsigned short	(*v_unsigned_short_pointer_array)[2];

int		(*v_int_pointer_array)[2];
signed int	(*v_signed_int_pointer_array)[2];
unsigned int	(*v_unsigned_int_pointer_array)[2];

long		(*v_long_pointer_array)[2];
signed long	(*v_signed_long_pointer_array)[2];
unsigned long	(*v_unsigned_long_pointer_array)[2];

float		(*v_float_pointer_array)[2];
double		(*v_double_pointer_array)[2];


/**** structs *******/

struct t_struct {
    char	v_char_member;
    short	v_short_member;
    int		v_int_member;
    long	v_long_member;
    float	v_float_member;
    double	v_double_member;
} v_struct1;

struct t_struct *v_t_struct_p;

struct {
    char	v_char_member;
    short	v_short_member;
    int		v_int_member;
    long	v_long_member;
    float	v_float_member;
    double	v_double_member;
} v_struct2;

/* typedef'd struct without a tag.  */
typedef struct {
  double v_double_member;
  int v_int_member;
} t_struct3;
/* GCC seems to want a variable of this type, or else it won't put out
   a symbol.  */
t_struct3 v_struct3;

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

/* typedef'd union without a tag.  */
typedef union {
  double v_double_member;
  int v_int_member;
} t_union3;
/* GCC seems to want a variable of this type, or else it won't put out
   a symbol.  */
t_union3 v_union3;

/**** Enumerations *******/

enum 
/* Work around the bug for compilers which don't put out the right stabs.  */
#if __GNUC__ < 2 && !defined (_AIX)
primary1_tag
#endif
{red1, green1, blue1} primary1;

enum {red, green, blue} primary;
enum colors {yellow, purple, pink} nonprimary;

enum {chevy, ford} clunker;
enum cars {bmw, porsche} sportscar;

#undef FALSE
#undef TRUE
typedef enum {FALSE, TRUE} boolean;
boolean v_boolean;
/*note: aCC has bool type predefined with 'false' and 'true'*/
typedef enum bvals {my_false, my_true} boolean2;
boolean2 v_boolean2;

enum misordered {two = 2, one = 1, zero = 0, three = 3};

/* Seems like we need a variable of this type to get the type to be put
   in the executable, at least for AIX xlc.  */
enum misordered v_misordered = three;

/**** Function pointers *******/

char		(*v_char_func) (int, int*);
signed char	(*v_signed_char_func) (int, int*);
unsigned char	(*v_unsigned_char_func) (int, int*);

short		(*v_short_func) (int, int*);
signed short	(*v_signed_short_func) (int, int*);
unsigned short	(*v_unsigned_short_func) (int, int*);

int		(*v_int_func) (int, int*);
signed int	(*v_signed_int_func) (int, int*);
unsigned int	(*v_unsigned_int_func) (int, int*);

long		(*v_long_func) (int, int*);
signed long	(*v_signed_long_func) (int, int*);
unsigned long	(*v_unsigned_long_func) (int, int*);

long long		(*v_long_long_func) (int, int*);
signed long long	(*v_signed_long_long_func) (int, int*);
unsigned long long	(*v_unsigned_long_long_func) (int, int*);

float		(*v_float_func) (int, int*);
double		(*v_double_func) (int, int*);

void use (void *p)
{
}

int main ()
{
  use (&v_char);
  use (&v_signed_char);
  use (&v_unsigned_char);

  use (&v_short);
  use (&v_signed_short);
  use (&v_unsigned_short);

  use (&v_int);
  use (&v_signed_int);
  use (&v_unsigned_int);

  use (&v_long);
  use (&v_signed_long);
  use (&v_unsigned_long);

  use (&v_long_long);
  use (&v_signed_long_long);
  use (&v_unsigned_long_long);

  use (&v_float);
  use (&v_double);

  use (v_char_array);
  use (v_signed_char_array);
  use (v_unsigned_char_array);

  use (v_short_array);
  use (v_signed_short_array);
  use (v_unsigned_short_array);

  use (v_int_array);
  use (v_signed_int_array);
  use (v_unsigned_int_array);

  use (v_long_array);
  use (v_signed_long_array);
  use (v_unsigned_long_array);

  use (v_float_array);
  use (v_double_array);

  use (v_char_pointer);
  use (v_signed_char_pointer);
  use (v_unsigned_char_pointer);

  use (v_short_pointer);
  use (v_signed_short_pointer);
  use (v_unsigned_short_pointer);

  use (v_int_pointer);
  use (v_signed_int_pointer);
  use (v_unsigned_int_pointer);

  use (v_long_pointer);
  use (v_signed_long_pointer);
  use (v_unsigned_long_pointer);

  use (v_float_pointer);
  use (v_double_pointer);

  use (v_char_pointer_pointer);
  use (v_signed_char_pointer_pointer);
  use (v_unsigned_char_pointer_pointer);

  use (v_short_pointer_pointer);
  use (v_signed_short_pointer_pointer);
  use (v_unsigned_short_pointer_pointer);

  use (v_int_pointer_pointer);
  use (v_signed_int_pointer_pointer);
  use (v_unsigned_int_pointer_pointer);

  use (v_long_pointer_pointer);
  use (v_signed_long_pointer_pointer);
  use (v_unsigned_long_pointer_pointer);

  use (v_float_pointer_pointer);
  use (v_double_pointer_pointer);

  use (v_char_array_pointer);
  use (v_signed_char_array_pointer);
  use (v_unsigned_char_array_pointer);

  use (v_short_array_pointer);
  use (v_signed_short_array_pointer);
  use (v_unsigned_short_array_pointer);

  use (v_int_array_pointer);
  use (v_signed_int_array_pointer);
  use (v_unsigned_int_array_pointer);

  use (v_long_array_pointer);
  use (v_signed_long_array_pointer);
  use (v_unsigned_long_array_pointer);

  use (v_float_array_pointer);
  use (v_double_array_pointer);

  use (v_char_pointer_array);
  use (v_signed_char_pointer_array);
  use (v_unsigned_char_pointer_array);

  use (v_short_pointer_array);
  use (v_signed_short_pointer_array);
  use (v_unsigned_short_pointer_array);

  use (v_int_pointer_array);
  use (v_signed_int_pointer_array);
  use (v_unsigned_int_pointer_array);

  use (v_long_pointer_array);
  use (v_signed_long_pointer_array);
  use (v_unsigned_long_pointer_array);

  use (v_float_pointer_array);
  use (v_double_pointer_array);

  use (&v_struct1);
  use (&v_struct2);
  use (&v_struct3);

  use (&v_union);
  use (&v_union2);
  use (&v_union3);

  use (&v_boolean);
  use (&v_boolean2);
  use (&v_misordered);

  use (&v_char_func);
  use (&v_signed_char_func);
  use (&v_unsigned_char_func);

  use (&v_short_func);
  use (&v_signed_short_func);
  use (&v_unsigned_short_func);

  use (&v_int_func);
  use (&v_signed_int_func);
  use (&v_unsigned_int_func);

  use (&v_long_func);
  use (&v_signed_long_func);
  use (&v_unsigned_long_func);

  use (&v_long_long_func);
  use (&v_signed_long_long_func);
  use (&v_unsigned_long_long_func);

  use (&v_float_func);
  use (&v_double_func);
}
