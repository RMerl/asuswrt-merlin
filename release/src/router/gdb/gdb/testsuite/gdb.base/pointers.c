
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



char		*v_char_pointer;
signed char	*v_signed_char_pointer;
unsigned char	*v_unsigned_char_pointer;

short		*v_short_pointer;
signed short	*v_signed_short_pointer;
unsigned short	*v_unsigned_short_pointer;

int		*v_int_pointer;
int             *v_int_pointer2;
signed int	*v_signed_int_pointer;
unsigned int	*v_unsigned_int_pointer;

long		*v_long_pointer;
signed long	*v_signed_long_pointer;
unsigned long	*v_unsigned_long_pointer;

float		*v_float_pointer;
double		*v_double_pointer;


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

int matrix[2][3] = { { 0, 1, 2}, {3, 4, 5}};
int (*rptr)[3] = matrix;

float ** ptr_to_ptr_to_float;

int y;

/* Do nothing function used for forcing some of the above variables to
   be referenced by the program source.  If the variables are not
   referenced, some linkers will remove the symbol from the symbol
   table making it impossible to refer to the variable in gdb.  */
void usevar (void *var) {}

int main ()
{
  void dummy();
  int more_code();
  
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  dummy();

  more_code ();

  usevar (&v_int_pointer2);
  usevar (&rptr);
  usevar (&y);

  return 0;
  
}

void dummy()
{
    
  
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

  ptr_to_ptr_to_float = &v_float_pointer;
  
  
  v_char_array[0] = v_char;
  v_signed_char_array[0] = v_signed_char;
  v_unsigned_char_array[0] = v_unsigned_char;

  v_short_array[0] = v_short;
  v_signed_short_array[0] = v_signed_short;
  v_unsigned_short_array[0] = v_unsigned_short;

  v_int_array[0] = v_int;
  v_int_array[1] = v_int * 3;
  
  v_signed_int_array[0] = v_signed_int;
  v_unsigned_int_array[0] = v_unsigned_int;

  v_long_array[0] = v_long;
  v_signed_long_array[0] = v_signed_long;
  v_unsigned_long_array[0] = v_unsigned_long;

  v_float_array[0] = v_float;
  v_double_array[0] = v_double;

}

void marker1 ()
{
}

int more_code()
{
    char C, *pC, **ppC, ***pppC, ****ppppC, *****pppppC, ******ppppppC;
    unsigned char UC, *pUC;
    short S, *pS;
    unsigned short US, *pUS;
    int I, *pI;
    unsigned int UI, *pUI;
    long L, *pL;
    unsigned long UL, *pUL;
    float F, *pF;
    double D, *pD;

    C = 'A';
    UC = 21;
    S = -14;
    US = 7;
    I = 102;
    UI = 1002;
    L = -234;
    UL = 234;
    F = 1.25E10;
    D = -1.25E-37;
    pC = &C;
    ppC = &pC;
    pppC = &ppC;
    ppppC = &pppC;
    pppppC = &ppppC;
    ppppppC = &pppppC;
    pUC = &UC;
    pS = &S;
    pUS = &US;
    pI = &I;
    pUI = &UI;
    pL = &L;
    pUL = &UL;
    pF = &F;
    pD = &D;
    
    marker1();
    return 0;
}
