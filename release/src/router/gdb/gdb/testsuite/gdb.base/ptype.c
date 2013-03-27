/*
 *	Test file with lots of different types, for testing the
 *	"ptype" command.
 */

/*
 *	First the basic C types.
 */
#include <stdlib.h>

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
t_char_array *pv_char_array;

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

/**** Pointers to functions *******/

typedef int (*func_type) (int (*) (int, float), float);
double (*old_fptr) ();
double (*new_fptr) (void);
int (*fptr) (int, float);
int *(*fptr2) (int (*) (int, float), float);
int (*xptr) (int (*) (), int (*) (void), int);
int (*(*ffptr) (char)) (short);
int (*(*(*fffptr) (char)) (short)) (long);

func_type v_func_type;

/* Here are the sort of stabs we expect to see for the above:

   .stabs "func_type:t(0,100)=*(0,101)=g(0,1)(0,102)=*(0,103)=g(0,1)(0,1)(0,14)#(0,14)#",128,0,234,0
   .stabs "old_fptr:G(0,110)=*(0,111)=f(0,15)",32,0,231,0
   .stabs "new_fptr:G(0,120)=*(0,121)=g(0,15)(0,122)=(0,122)#",32,0,232,0
   .stabs "fptr:G(0,130)=*(0,103)#",32,0,233,0
   .stabs "fptr2:G(0,140)=*(0,141)=g(0,142)=*(0,1)(0,102)(0,14)#",32,0,235,0
   .stabs "xptr:G(0,150)=*(0,151)=g(0,1)(0,152)=*(0,153)=f(0,1)(0,154)=*(0,155)=g(0,1)(0,122)#(0,1)#",32,0,236,0
   .stabs "ffptr:G(0,160)=*(0,161)=g(0,162)=*(0,163)=g(0,1)(0,8)#(0,2)#",32,0,237,0\
   .stabs "fffptr:G(0,170)=*(0,171)=g(0,172)=*(0,173)=g(0,174)=*(0,175)=g(0,1)(0,3)#(0,8)#(0,2)#",32,0,237,0

   Most of these use Sun's extension for prototyped function types ---
   the 'g' type descriptor.  As of around 9 Feb 2002, GCC didn't emit
   those, but GDB can read them, so the related tests in ptype.exp
   will all xfail.  */


/***********/

typedef int foo;

foo intfoo (afoo)
{
  return (afoo * 2);
}

/***********/

int main ()
{
  /* Ensure that malloc is a pointer type; avoid use of "void" and any include files. */
/*  extern char *malloc();*/

  /* Some of the tests in ptype.exp require invoking malloc, so make
     sure it is linked in to this program.  */
  v_char_pointer = (char *) malloc (1);

#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  /* Some linkers (e.g. on AIX) remove unreferenced variables,
     so make sure to reference them. */
  primary = blue;
  primary1 = blue1;
  nonprimary = pink;
  sportscar = porsche;
  clunker = ford;
  v_struct1.v_int_member = 5;
  v_struct2.v_int_member = 6;
  v_struct3.v_int_member = 7;

  v_char = 0;
  v_signed_char = 0;
  v_unsigned_char = 0;

  v_short = 0;
  v_signed_short = 0;
  v_unsigned_short = 0;

  v_int = 0;
  v_signed_int = 0;
  v_unsigned_int = 0;

  v_long = 0;
  v_signed_long = 0;
  v_unsigned_long = 0;

  v_float = 0;
  v_double = 0;

  v_char_array[0] = 0;
  v_signed_char_array[0] = 0;
  v_unsigned_char_array[0] = 0;

  v_short_array[0] = 0;
  v_signed_short_array[0] = 0;
  v_unsigned_short_array[0] = 0;

  v_int_array[0] = 0;
  v_signed_int_array[0] = 0;
  v_unsigned_int_array[0] = 0;

  v_long_array[0] = 0;
  v_signed_long_array[0] = 0;
  v_unsigned_long_array[0] = 0;

  v_float_array[0] = 0;
  v_double_array[0] = 0;

  v_char_pointer = 0;
  v_signed_char_pointer = 0;
  v_unsigned_char_pointer = 0;

  v_short_pointer = 0;
  v_signed_short_pointer = 0;
  v_unsigned_short_pointer = 0;

  v_int_pointer = 0;
  v_signed_int_pointer = 0;
  v_unsigned_int_pointer = 0;

  v_long_pointer = 0;
  v_signed_long_pointer = 0;
  v_unsigned_long_pointer = 0;

  v_float_pointer = 0;
  v_double_pointer = 0;

  nested_su.outer_int = 0;
  v_t_struct_p = 0;

  v_boolean = FALSE;
  v_boolean2 = my_false;
  return 0;
}
