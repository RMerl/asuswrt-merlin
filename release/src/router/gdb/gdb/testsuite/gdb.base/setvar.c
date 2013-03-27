#include <stdlib.h>

#ifdef PROTOTYPES
int main (int argc, char **argv, char **envp)
#else
main (argc, argv, envp)
     int argc;
     char **argv;
     char **envp;
#endif
{
    extern void dummy();
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
    dummy();
    return 0;
}

/* We put main() right up front so its line number doesn't keep changing.  */

/*
 *	Test file with lots of different types, for testing the
 *	"whatis" command.
 */

/*
 *	First the basic C types.
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

/**** Enumerations *******/

enum colors {red, green, blue} color;
enum cars {chevy, ford, porsche} clunker;

/**** Enumeration bitfields, supported by GNU C *******/

#ifdef __GNUC__
enum senum {sm1 = -1, s1 = 1};
struct senum_field {enum senum field:2; } sef;
enum uenum {u1 = 1, u2 = 2};
struct uenum_field {enum uenum field:2; } uef;
#endif

void
dummy ()
{
  /* setvar.exp wants to allocate memory for constants.  So make sure malloc
     gets linked into the program.  */
  malloc (1);

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

  v_struct2.v_int_member = v_struct1.v_int_member;
  v_union2.v_short_member = v_union.v_short_member;

#ifdef __GNUC__
  sef.field = s1;
  uef.field = u1;
#endif
}
