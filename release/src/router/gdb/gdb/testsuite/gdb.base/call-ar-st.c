
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**************************************************************************
 * TESTS :
 *   -- function arguments that are enumerated types
 *   -- small structure arguments ( <= 64 bits ) 
 *            -- stored in registers
 *            -- stored on the stack
 *   -- large structure arguments ( > 64 bits )
 *            -- stored in registers
 *            -- stored on the stack
 *   -- array arguments
 *   -- caller is a leaf routine :
 *            -- use the call command from within an init routine (i.e.
 *               init_bit_flags, init_bit_flags_combo, init_array_rep)
 *   -- caller doesn't have enough space for all the function arguments :
 *            -- call print_long_arg_list from inside print_small_structs
 ***************************************************************************/

/* Some enumerated types -- used to test that the structureal data type is
 * retrieved for function arguments with typedef data types.
 */
typedef int id_int;

typedef enum { 
	      BLACK,
	      BLUE,
	      BROWN,
	      ECRUE,
	      GOLD,
	      GRAY,
	      GREEN,
	      IVORY,
	      MAUVE,
	      ORANGE,
	      PINK,
	      PURPLE,
	      RED,
	      SILVER,
	      TAN,
	      VIOLET,
	      WHITE,
	      YELLOW} colors;

/* A large structure (> 64 bits) used to test passing large structures as
 * parameters
 */

struct array_rep_info_t {
   int   next_index[10]; 
   int   values[10];
   int   head;
};

/*****************************************************************************
 * Small structures ( <= 64 bits). These are used to test passing small 
 * structures as parameters and test argument size promotion.
 *****************************************************************************/

 /* 64 bits
  */
struct small_rep_info_t {
   int   value;
   int   head;
};

/* 6 bits : really fits in 8 bits and is promoted to 32 bits
 */
struct bit_flags_t {
       unsigned alpha   :1;
       unsigned beta    :1;
       unsigned gamma   :1;
       unsigned delta   :1;
       unsigned epsilon :1;
       unsigned omega   :1;
};

/* 22 bits : really fits in 40 bits and is promoted to 64 bits
 */
struct bit_flags_combo_t {
       unsigned alpha   :1;
       unsigned beta    :1;
       char     ch1;
       unsigned gamma   :1;
       unsigned delta   :1;
       char     ch2;
       unsigned epsilon :1;
       unsigned omega   :1;
};

/* 64 bits
 */
struct one_double_t {
       double double1;
};

/* 64 bits
 */
struct two_floats_t {
       float float1;
       float float2;
};

/* 16 bits : promoted to 32 bits
 */
struct two_char_t {
       char ch1;
       char ch2;
};

/* 24 bits : promoted to 32 bits
 */
struct three_char_t {
       char ch1;
       char ch2;
       char ch3;
};

/* 40 bits : promoted to 64 bits
 */
struct five_char_t {
       char ch1;
       char ch2;
       char ch3;
       char ch4;
       char ch5;
};

/* 40 bits : promoted to 64 bits
 */
struct int_char_combo_t {
       int  int1;
       char ch1;
};

/*****************************************************************
 * PRINT_STUDENT_ID_SHIRT_COLOR : 
 * IN     id_int student       -- enumerated type
 * IN     colors shirt         -- enumerated type
 *****************************************************************/
#ifdef PROTOTYPES
void print_student_id_shirt_color (id_int student, colors shirt)
#else
void print_student_id_shirt_color ( student, shirt ) 
 id_int student;
 colors shirt;
#endif
{

 printf("student id : %d\t", student);
 printf("shirt color : ");
 switch (shirt) {
   case BLACK :  printf("BLACK\n"); 
		 break;
   case BLUE :   printf("BLUE\n");
		 break;
   case BROWN :  printf("BROWN\n");
		 break;
   case ECRUE :  printf("ECRUE\n");
		 break;
   case GOLD :   printf("GOLD\n");
		 break;
   case GRAY :   printf("GRAY\n");
		 break;
   case GREEN :  printf("GREEN\n");
		 break;
   case IVORY :  printf("IVORY\n");
		 break;
   case MAUVE :  printf("MAUVE\n");
		 break;
   case ORANGE : printf("ORANGE\n");
		 break;
   case PINK :   printf("PINK\n");
		 break;
   case PURPLE : printf("PURPLE\n");
		 break;
   case RED :    printf("RED\n");
		 break;
   case SILVER : printf("SILVER\n");
		 break;
   case TAN :    printf("TAN\n");
		 break;
   case VIOLET : printf("VIOLET\n");
		 break;
   case WHITE :  printf("WHITE\n");
		 break;
   case YELLOW : printf("YELLOW\n");
		 break;
 }
}

/*****************************************************************
 * PRINT_CHAR_ARRAY : 
 * IN     char  array_c[]      -- character array 
 *****************************************************************/
#ifdef PROTOTYPES
void print_char_array (char array_c[])
#else
void print_char_array ( array_c ) 
     char    array_c[];
#endif
{

  int index;

  printf("array_c :\n");
  printf("=========\n\n");
  for (index = 0; index < 120; index++) {
      printf("%1c", array_c[index]); 
      if ((index%50) == 0) printf("\n");
  }
  printf("\n\n");
}

/*****************************************************************
 * PRINT_DOUBLE_ARRAY : 
 * IN     double array_d[]      -- array of doubles
 *****************************************************************/
#ifdef PROTOTYPES
void print_double_array (double  array_d[])
#else
void print_double_array (array_d) 
     double  array_d[];
#endif
{

  int index;

  printf("array_d :\n");
  printf("=========\n\n");
  for (index = 0; index < 9; index++) {
      printf("%f  ", array_d[index]); 
      if ((index%8) == 0) printf("\n");
  }
  printf("\n\n");
}

/*****************************************************************
 * PRINT_FLOAT_ARRAY: 
 * IN     float array_f[]      -- array of floats 
 *****************************************************************/
#ifdef PROTOTYPES
void print_float_array (float array_f[])
#else
void print_float_array ( array_f )
     float array_f[];
#endif
{

  int index;

  printf("array_f :\n");
  printf("=========\n\n");
  for (index = 0; index < 15; index++) {
      printf("%f  ", array_f[index]); 
      if ((index%8) == 0) printf("\n");

  }
  printf("\n\n");
}

/*****************************************************************
 * PRINT_INT_ARRAY: 
 * IN     int  array_i[]      -- array of integers 
 *****************************************************************/
#ifdef PROTOTYPES
void print_int_array (int array_i[])
#else
void print_int_array ( array_i )
     int array_i[];
#endif
{

  int index;

  printf("array_i :\n");
  printf("=========\n\n");
  for (index = 0; index < 50; index++) {
      printf("%d  ", array_i[index]); 
      if ((index%8) == 0) printf("\n");
  }
  printf("\n\n");

}

/*****************************************************************
 * PRINT_ALL_ARRAYS: 
 * IN     int  array_i[]      -- array of integers 
 * IN     char array_c[]      -- array of characters 
 * IN     float array_f[]      -- array of floats 
 * IN     double array_d[]      -- array of doubles 
 *****************************************************************/
#ifdef PROTOTYPES
void print_all_arrays(int array_i[], char array_c[], float array_f[], double array_d[])
#else
void print_all_arrays( array_i, array_c, array_f, array_d )
     int array_i[];
     char array_c[];
     float array_f[];
     double array_d[];
#endif
{
  print_int_array(array_i);
  print_char_array(array_c);
  print_float_array(array_f);
  print_double_array(array_d);
}

/*****************************************************************
 * LOOP_COUNT : 
 * A do nothing function. Used to provide a point at which calls can be made.  
 *****************************************************************/
void loop_count () {

     int index;

     for (index=0; index<4; index++);
}

/*****************************************************************
 * COMPUTE_WITH_SMALL_STRUCTS : 
 * A do nothing function. Used to provide a point at which calls can be made.  
 * IN  int seed
 *****************************************************************/
#ifdef PROTOTYPES
void compute_with_small_structs (int seed)
#else
void compute_with_small_structs ( seed ) 
 int seed;
#endif
{

     struct small_rep_info_t array[4];
     int index;

     for (index = 0; index < 4; index++) {
         array[index].value = index*seed;
	 array[index].head = (index+1)*seed;
     }

     for (index = 1; index < 4; index++) {
	 array[index].value = array[index].value + array[index-1].value;
	 array[index].head = array[index].head + array[index-1].head;
     }
}

/*****************************************************************
 * INIT_BIT_FLAGS :
 * Initializes a bit_flags_t structure. Can call this function see
 * the call command behavior when integer arguments do not fit into
 * registers and must be placed on the stack.
 * OUT struct bit_flags_t *bit_flags -- structure to be filled
 * IN  unsigned a  -- 0 or 1 
 * IN  unsigned b  -- 0 or 1 
 * IN  unsigned g  -- 0 or 1 
 * IN  unsigned d  -- 0 or 1 
 * IN  unsigned e  -- 0 or 1 
 * IN  unsigned o  -- 0 or 1 
 *****************************************************************/
#ifdef PROTOTYPES
void init_bit_flags (struct bit_flags_t *bit_flags, unsigned a, unsigned b, unsigned g, unsigned d, unsigned e, unsigned o)
#else
void init_bit_flags ( bit_flags, a, b, g, d, e, o )
struct bit_flags_t *bit_flags;
unsigned a;
unsigned b;
unsigned g;
unsigned d;
unsigned e;
unsigned o; 
#endif
{

   bit_flags->alpha = a;
   bit_flags->beta = b;
   bit_flags->gamma = g;
   bit_flags->delta = d;
   bit_flags->epsilon = e;
   bit_flags->omega = o;
}

/*****************************************************************
 * INIT_BIT_FLAGS_COMBO :
 * Initializes a bit_flags_combo_t structure. Can call this function
 * to see the call command behavior when integer and character arguments
 * do not fit into registers and must be placed on the stack.
 * OUT struct bit_flags_combo_t *bit_flags_combo -- structure to fill
 * IN  unsigned a  -- 0 or 1 
 * IN  unsigned b  -- 0 or 1 
 * IN  char     ch1
 * IN  unsigned g  -- 0 or 1 
 * IN  unsigned d  -- 0 or 1 
 * IN  char     ch2
 * IN  unsigned e  -- 0 or 1 
 * IN  unsigned o  -- 0 or 1 
 *****************************************************************/
#ifdef PROTOTYPES
void init_bit_flags_combo (struct bit_flags_combo_t *bit_flags_combo, unsigned a, unsigned b, char ch1, unsigned g, unsigned d, char ch2, unsigned e, unsigned o)
#else
void init_bit_flags_combo ( bit_flags_combo, a, b, ch1, g, d, ch2, e, o )
     struct bit_flags_combo_t *bit_flags_combo;
     unsigned a;
     unsigned b;
     char     ch1;
     unsigned g;
     unsigned d;
     char     ch2;
     unsigned e;
     unsigned o; 
#endif
{

   bit_flags_combo->alpha = a;
   bit_flags_combo->beta = b;
   bit_flags_combo->ch1 = ch1;
   bit_flags_combo->gamma = g;
   bit_flags_combo->delta = d;
   bit_flags_combo->ch2 = ch2;
   bit_flags_combo->epsilon = e;
   bit_flags_combo->omega = o;
}


/*****************************************************************
 * INIT_ONE_DOUBLE : 
 * OUT  struct one_double_t *one_double  -- structure to fill 
 * IN   double init_val
 *****************************************************************/
#ifdef PROTOTYPES
void init_one_double (struct one_double_t *one_double, double init_val)
#else
void init_one_double ( one_double, init_val )
     struct one_double_t *one_double;
     double init_val; 
#endif
{

     one_double->double1  = init_val;
}

/*****************************************************************
 * INIT_TWO_FLOATS : 
 * OUT struct two_floats_t *two_floats -- structure to be filled
 * IN  float init_val1 
 * IN  float init_val2 
 *****************************************************************/
#ifdef PROTOTYPES
void init_two_floats (struct two_floats_t *two_floats, float init_val1, float init_val2)
#else
void init_two_floats ( two_floats, init_val1, init_val2 )
     struct two_floats_t *two_floats; 
     float init_val1;
     float init_val2;
#endif
{
     two_floats->float1 = init_val1;
     two_floats->float2 = init_val2;
}

/*****************************************************************
 * INIT_TWO_CHARS : 
 * OUT struct two_char_t *two_char -- structure to be filled
 * IN  char init_val1 
 * IN  char init_val2 
 *****************************************************************/
#ifdef PROTOTYPES
void init_two_chars (struct two_char_t *two_char, char init_val1, char init_val2)
#else
void init_two_chars ( two_char, init_val1, init_val2 )
     struct two_char_t *two_char;
     char init_val1;
     char init_val2; 
#endif
{

     two_char->ch1 = init_val1;
     two_char->ch2 = init_val2;
}

/*****************************************************************
 * INIT_THREE_CHARS : 
 * OUT struct three_char_t *three_char -- structure to be filled
 * IN  char init_val1 
 * IN  char init_val2 
 * IN  char init_val3 
 *****************************************************************/
#ifdef PROTOTYPES
void init_three_chars (struct three_char_t *three_char, char init_val1, char init_val2, char init_val3)
#else
void init_three_chars ( three_char, init_val1, init_val2, init_val3 )  
     struct three_char_t *three_char; 
     char init_val1;
     char init_val2;
     char init_val3;
#endif
{

     three_char->ch1 = init_val1;
     three_char->ch2 = init_val2;
     three_char->ch3 = init_val3;
}

/*****************************************************************
 * INIT_FIVE_CHARS : 
 * OUT struct five_char_t *five_char -- structure to be filled
 * IN  char init_val1 
 * IN  char init_val2 
 * IN  char init_val3 
 * IN  char init_val4 
 * IN  char init_val5 
 *****************************************************************/
#ifdef PROTOTYPES
void init_five_chars (struct five_char_t *five_char, char init_val1, char init_val2, char init_val3, char init_val4, char init_val5)
#else
void init_five_chars ( five_char, init_val1, init_val2, init_val3,init_val4,init_val5 )
     struct five_char_t *five_char;
     char init_val1;
     char init_val2;
     char init_val3;
     char init_val4;
     char init_val5;
#endif
{
     five_char->ch1 = init_val1;
     five_char->ch2 = init_val2;
     five_char->ch3 = init_val3;
     five_char->ch4 = init_val4;
     five_char->ch5 = init_val5;
}

/*****************************************************************
 * INIT_INT_CHAR_COMBO : 
 * OUT struct int_char_combo_t *combo -- structure to be filled
 * IN  int  init_val1 
 * IN  char init_val2 
 *****************************************************************/
#ifdef PROTOTYPES
void init_int_char_combo (struct int_char_combo_t *combo, int init_val1, char init_val2)
#else
void init_int_char_combo ( combo, init_val1, init_val2 )
     struct int_char_combo_t *combo;
     int init_val1; 
     char init_val2; 
#endif
{

     combo->int1 = init_val1;
     combo->ch1 = init_val2;
}

/*****************************************************************
 * INIT_STRUCT_REP : 
 * OUT struct small_rep_into_t *small_struct -- structure to be filled
 * IN  int  seed 
 *****************************************************************/
#ifdef PROTOTYPES
void init_struct_rep(struct small_rep_info_t *small_struct, int seed)
#else
void init_struct_rep( small_struct, seed )
     struct small_rep_info_t *small_struct;
     int    seed;
#endif
{

      small_struct->value = 2 + (seed*2); 
      small_struct->head = 0; 
}

/*****************************************************************
 * INIT_SMALL_STRUCTS : 
 * Takes all the small structures as input and calls the appropriate
 * initialization routine for each structure
 *****************************************************************/
#ifdef PROTOTYPES
void init_small_structs (
     struct small_rep_info_t  *struct1,
     struct small_rep_info_t  *struct2,
     struct small_rep_info_t  *struct3,
     struct small_rep_info_t  *struct4,
     struct bit_flags_t       *flags,
     struct bit_flags_combo_t *flags_combo,
     struct three_char_t      *three_char,
     struct five_char_t       *five_char,
     struct int_char_combo_t  *int_char_combo,
     struct one_double_t      *d1,
     struct one_double_t      *d2,
     struct one_double_t      *d3,
     struct two_floats_t      *f1,
     struct two_floats_t      *f2,
     struct two_floats_t      *f3)
#else
void init_small_structs (struct1, struct2, struct3,struct4,flags,flags_combo,
three_char, five_char,int_char_combo, d1, d2,d3,f1,f2,f3)
     struct small_rep_info_t  *struct1;
     struct small_rep_info_t  *struct2;
     struct small_rep_info_t  *struct3;
     struct small_rep_info_t  *struct4;
     struct bit_flags_t       *flags;
     struct bit_flags_combo_t *flags_combo;
     struct three_char_t      *three_char;
     struct five_char_t       *five_char;
     struct int_char_combo_t  *int_char_combo;
     struct one_double_t      *d1;
     struct one_double_t      *d2;
     struct one_double_t      *d3;
     struct two_floats_t      *f1;
     struct two_floats_t      *f2;
     struct two_floats_t      *f3;
#endif
{

     init_bit_flags(flags, (unsigned)1, (unsigned)0, (unsigned)1, 
		           (unsigned)0, (unsigned)1, (unsigned)0 ); 
     init_bit_flags_combo(flags_combo, (unsigned)1, (unsigned)0, 'y',
				       (unsigned)1, (unsigned)0, 'n',
                    		       (unsigned)1, (unsigned)0 ); 
     init_three_chars(three_char, 'a', 'b', 'c');
     init_five_chars(five_char, 'l', 'm', 'n', 'o', 'p');
     init_int_char_combo(int_char_combo, 123, 'z');
     init_struct_rep(struct1, 2);
     init_struct_rep(struct2, 4);
     init_struct_rep(struct3, 5);
     init_struct_rep(struct4, 6);
     init_one_double ( d1, 10.5); 
     init_one_double ( d2, -3.375); 
     init_one_double ( d3, 675.09375); 
     init_two_floats ( f1, 45.234, 43.6); 
     init_two_floats ( f2, 78.01, 122.10); 
     init_two_floats ( f3, -1232.345, -199.21); 
}

/*****************************************************************
 * PRINT_TEN_DOUBLES : 
 * ?????????????????????????????
 ****************************************************************/
#ifdef PROTOTYPES
void print_ten_doubles (
     double d1,
     double d2,
     double d3,
     double d4,
     double d5,
     double d6,
     double d7,
     double d8,
     double d9,
     double d10)
#else
void print_ten_doubles ( d1, d2, d3, d4, d5, d6, d7, d8, d9, d10 )
     double d1;
     double d2;
     double d3;
     double d4;
     double d5;
     double d6;
     double d7;
     double d8;
     double d9;
     double d10; 
#endif
{

  printf("Two Doubles : %f\t%f\n", d1, d2);
  printf("Two Doubles : %f\t%f\n", d3, d4);
  printf("Two Doubles : %f\t%f\n", d5, d6);
  printf("Two Doubles : %f\t%f\n", d7, d8);
  printf("Two Doubles : %f\t%f\n", d9, d10);
}

/*****************************************************************
 * PRINT_BIT_FLAGS : 
 * IN struct bit_flags_t bit_flags 
 ****************************************************************/
#ifdef PROTOTYPES
void print_bit_flags (struct bit_flags_t bit_flags)
#else
void print_bit_flags ( bit_flags )
struct bit_flags_t bit_flags;
#endif
{

     if (bit_flags.alpha) printf("alpha\n");
     if (bit_flags.beta) printf("beta\n");
     if (bit_flags.gamma) printf("gamma\n");
     if (bit_flags.delta) printf("delta\n");
     if (bit_flags.epsilon) printf("epsilon\n");
     if (bit_flags.omega) printf("omega\n");
}

/*****************************************************************
 * PRINT_BIT_FLAGS_COMBO : 
 * IN struct bit_flags_combo_t bit_flags_combo 
 ****************************************************************/
#ifdef PROTOTYPES
void print_bit_flags_combo (struct bit_flags_combo_t bit_flags_combo)
#else
void print_bit_flags_combo ( bit_flags_combo )
     struct bit_flags_combo_t bit_flags_combo;
#endif
{

     if (bit_flags_combo.alpha) printf("alpha\n");
     if (bit_flags_combo.beta) printf("beta\n");
     if (bit_flags_combo.gamma) printf("gamma\n");
     if (bit_flags_combo.delta) printf("delta\n");
     if (bit_flags_combo.epsilon) printf("epsilon\n");
     if (bit_flags_combo.omega) printf("omega\n");
     printf("ch1: %c\tch2: %c\n", bit_flags_combo.ch1, bit_flags_combo.ch2);
}

/*****************************************************************
 * PRINT_ONE_DOUBLE : 
 * IN struct one_double_t one_double 
 ****************************************************************/
#ifdef PROTOTYPES
void print_one_double (struct one_double_t one_double)
#else
void print_one_double ( one_double )
struct one_double_t one_double;
#endif
{

     printf("Contents of one_double_t: \n\n");
     printf("%f\n", one_double.double1);
}

/*****************************************************************
 * PRINT_TWO_FLOATS : 
 * IN struct two_floats_t two_floats 
 ****************************************************************/
#ifdef PROTOTYPES
void print_two_floats (struct two_floats_t two_floats)
#else
void print_two_floats ( two_floats )
struct two_floats_t two_floats; 
#endif
{

     printf("Contents of two_floats_t: \n\n");
     printf("%f\t%f\n", two_floats.float1, two_floats.float2);
}

/*****************************************************************
 * PRINT_TWO_CHARS : 
 * IN struct two_char_t two_char
 ****************************************************************/
#ifdef PROTOTYPES
void print_two_chars (struct two_char_t two_char)
#else
void print_two_chars ( two_char )
struct two_char_t two_char; 
#endif
{

     printf("Contents of two_char_t: \n\n");
     printf("%c\t%c\n", two_char.ch1, two_char.ch2);
}

/*****************************************************************
 * PRINT_THREE_CHARS : 
 * IN struct three_char_t three_char
 ****************************************************************/
#ifdef PROTOTYPES
void print_three_chars (struct three_char_t three_char)
#else
void print_three_chars ( three_char )
struct three_char_t three_char;
#endif
{

     printf("Contents of three_char_t: \n\n");
     printf("%c\t%c\t%c\n", three_char.ch1, three_char.ch2, three_char.ch3);
}

/*****************************************************************
 * PRINT_FIVE_CHARS : 
 * IN struct five_char_t five_char
 ****************************************************************/
#ifdef PROTOTYPES
void print_five_chars (struct five_char_t five_char)
#else
void print_five_chars ( five_char ) 
struct five_char_t five_char; 
#endif
{

     printf("Contents of five_char_t: \n\n");
     printf("%c\t%c\t%c\t%c\t%c\n", five_char.ch1, five_char.ch2, 
				    five_char.ch3, five_char.ch4, 
				    five_char.ch5);
}

/*****************************************************************
 * PRINT_INT_CHAR_COMBO : 
 * IN struct int_char_combo_t int_char_combo
 ****************************************************************/
#ifdef PROTOTYPES
void print_int_char_combo (struct int_char_combo_t int_char_combo)
#else
void print_int_char_combo ( int_char_combo )
struct int_char_combo_t int_char_combo;
#endif
{

     printf("Contents of int_char_combo_t: \n\n");
     printf("%d\t%c\n", int_char_combo.int1, int_char_combo.ch1);
}     

/*****************************************************************
 * PRINT_STRUCT_REP : 
 * The last parameter must go onto the stack rather than into a register.
 * This is a good function to call to test small structures.
 * IN struct small_rep_info_t  struct1
 * IN struct small_rep_info_t  struct2
 * IN struct small_rep_info_t  struct3
 ****************************************************************/
#ifdef PROTOTYPES
void print_struct_rep(
     struct small_rep_info_t struct1,
     struct small_rep_info_t struct2,
     struct small_rep_info_t struct3)
#else
void print_struct_rep( struct1, struct2, struct3)
     struct small_rep_info_t struct1;
     struct small_rep_info_t struct2;
     struct small_rep_info_t struct3;
#endif
{


  printf("Contents of struct1: \n\n");
  printf("%10d%10d\n", struct1.value, struct1.head); 
  printf("Contents of struct2: \n\n");
  printf("%10d%10d\n", struct2.value, struct2.head); 
  printf("Contents of struct3: \n\n");
  printf("%10d%10d\n", struct3.value, struct3.head); 

}

/*****************************************************************
 * SUM_STRUCT_PRINT : 
 * The last two parameters must go onto the stack rather than into a register.
 * This is a good function to call to test small structures.
 * IN struct small_rep_info_t  struct1
 * IN struct small_rep_info_t  struct2
 * IN struct small_rep_info_t  struct3
 * IN struct small_rep_info_t  struct4
 ****************************************************************/
#ifdef PROTOTYPES
void sum_struct_print (
     int seed,
     struct small_rep_info_t struct1,
     struct small_rep_info_t struct2, 
     struct small_rep_info_t struct3,
     struct small_rep_info_t struct4)
#else
void sum_struct_print ( seed, struct1, struct2, struct3, struct4) 
     int seed;
     struct small_rep_info_t struct1;
     struct small_rep_info_t struct2; 
     struct small_rep_info_t struct3; 
     struct small_rep_info_t struct4; 
#endif
{
     int sum;

     printf("Sum of the 4 struct values and seed : \n\n");
     sum = seed + struct1.value + struct2.value + struct3.value + struct4.value;
     printf("%10d\n", sum);
}

/*****************************************************************
 * PRINT_SMALL_STRUCTS : 
 * This is a good function to call to test small structures.
 * All of the small structures of odd sizes (40 bits, 8bits, etc.)
 * are pushed onto the stack.
 ****************************************************************/
#ifdef PROTOTYPES
void print_small_structs (
     struct small_rep_info_t  struct1,
     struct small_rep_info_t  struct2,
     struct small_rep_info_t  struct3,
     struct small_rep_info_t  struct4,
     struct bit_flags_t       flags,
     struct bit_flags_combo_t flags_combo,
     struct three_char_t      three_char,
     struct five_char_t       five_char,
     struct int_char_combo_t  int_char_combo,
     struct one_double_t      d1,
     struct one_double_t      d2,
     struct one_double_t      d3,
     struct two_floats_t      f1,
     struct two_floats_t      f2,
     struct two_floats_t      f3)
#else
void print_small_structs ( struct1, struct2, struct3,  struct4, flags, 
flags_combo, three_char, five_char, int_char_combo, d1, d2,d3,f1,f2,f3)
     struct small_rep_info_t  struct1;
     struct small_rep_info_t  struct2;
     struct small_rep_info_t  struct3;
     struct small_rep_info_t  struct4;
     struct bit_flags_t       flags;
     struct bit_flags_combo_t flags_combo;
     struct three_char_t      three_char;
     struct five_char_t       five_char;
     struct int_char_combo_t  int_char_combo;
     struct one_double_t      d1;
     struct one_double_t      d2;
     struct one_double_t      d3;
     struct two_floats_t      f1;
     struct two_floats_t      f2;
     struct two_floats_t      f3;
#endif
{
   print_bit_flags(flags);
   print_bit_flags_combo(flags_combo);
   print_three_chars(three_char);
   print_five_chars(five_char);
   print_int_char_combo(int_char_combo);
   sum_struct_print(10, struct1, struct2, struct3, struct4);
   print_struct_rep(struct1, struct2, struct3);
   print_one_double(d1);
   print_one_double(d2);
   print_one_double(d3);
   print_two_floats(f1);
   print_two_floats(f2);
   print_two_floats(f3);
}

/*****************************************************************
 * PRINT_LONG_ARG_LIST : 
 * This is a good function to call to test small structures.
 * The first two parameters ( the doubles ) go into registers. The
 * remaining arguments are pushed onto the stack. Depending on where
 * print_long_arg_list is called from, the size of the argument list 
 * may force more space to be pushed onto the stack as part of the callers
 * frame.
 ****************************************************************/
#ifdef PROTOTYPES
void print_long_arg_list (
     double a,
     double b,
     int c,
     int d,
     int e,
     int f,
     struct small_rep_info_t  struct1,
     struct small_rep_info_t  struct2,
     struct small_rep_info_t  struct3,
     struct small_rep_info_t  struct4,
     struct bit_flags_t       flags,
     struct bit_flags_combo_t flags_combo,
     struct three_char_t      three_char,
     struct five_char_t       five_char,
     struct int_char_combo_t  int_char_combo,
     struct one_double_t      d1,
     struct one_double_t      d2,
     struct one_double_t      d3,
     struct two_floats_t      f1,
     struct two_floats_t      f2,
     struct two_floats_t      f3)
#else
void print_long_arg_list ( a, b, c, d, e, f, struct1, struct2, struct3, 
struct4, flags, flags_combo, three_char, five_char, int_char_combo, d1,d2,d3,
f1, f2, f3 )
     double a;
     double b;
     int c;
     int d;
     int e;
     int f;
     struct small_rep_info_t  struct1;
     struct small_rep_info_t  struct2;
     struct small_rep_info_t  struct3;
     struct small_rep_info_t  struct4;
     struct bit_flags_t       flags;
     struct bit_flags_combo_t flags_combo;
     struct three_char_t      three_char;
     struct five_char_t       five_char;
     struct int_char_combo_t  int_char_combo;
     struct one_double_t      d1;
     struct one_double_t      d2;
     struct one_double_t      d3;
     struct two_floats_t      f1;
     struct two_floats_t      f2;
     struct two_floats_t      f3;
#endif
{
    printf("double : %f\n", a);
    printf("double : %f\n", b);
    printf("int : %d\n", c);
    printf("int : %d\n", d);
    printf("int : %d\n", e);
    printf("int : %d\n", f);
    print_small_structs( struct1, struct2, struct3, struct4, flags, flags_combo,
			 three_char, five_char, int_char_combo, d1, d2, d3, 
			 f1, f2, f3);
}


#ifdef PROTOTYPES
void print_one_large_struct (struct array_rep_info_t linked_list1)
#else
void print_one_large_struct( linked_list1 )
     struct array_rep_info_t linked_list1;
#endif
{

 /* printf("Contents of linked list1: \n\n");
  printf("Element Value | Index of Next Element\n");
  printf("-------------------------------------\n");
  printf("              |                      \n");*/
  /*for (index = 0; index < 10; index++) {*/

      printf("%10d%10d\n", linked_list1.values[0], 
			   linked_list1.next_index[0]); 
  /*}*/
}

/*****************************************************************
 * PRINT_ARRAY_REP : 
 * The three structure parameters should fit into registers. 
 * IN struct array_rep_info_t linked_list1
 * IN struct array_rep_info_t linked_list2
 * IN struct array_rep_info_t linked_list3
 ****************************************************************/
#ifdef PROTOTYPES
void print_array_rep(
     struct array_rep_info_t linked_list1,
     struct array_rep_info_t linked_list2,
     struct array_rep_info_t linked_list3)
#else
void print_array_rep( linked_list1, linked_list2, linked_list3 )
     struct array_rep_info_t linked_list1;
     struct array_rep_info_t linked_list2;
     struct array_rep_info_t linked_list3;
#endif
{

  int index;

  printf("Contents of linked list1: \n\n");
  printf("Element Value | Index of Next Element\n");
  printf("-------------------------------------\n");
  printf("              |                      \n");
  for (index = 0; index < 10; index++) {

      printf("%10d%10d\n", linked_list1.values[index], 
			   linked_list1.next_index[index]); 
  }

  printf("Contents of linked list2: \n\n");
  printf("Element Value | Index of Next Element\n");
  printf("-------------------------------------\n");
  printf("              |                      \n");
  for (index = 0; index < 10; index++) {

      printf("%10d%10d\n", linked_list2.values[index], 
			   linked_list2.next_index[index]); 
  }

  printf("Contents of linked list3: \n\n");
  printf("Element Value | Index of Next Element\n");
  printf("-------------------------------------\n");
  printf("              |                      \n");
  for (index = 0; index < 10; index++) {

      printf("%10d%10d\n", linked_list3.values[index], 
			   linked_list3.next_index[index]); 
  }

}

/*****************************************************************
 * SUM_ARRAY_PRINT : 
 * The last structure parameter must be pushed onto the stack 
 * IN int    seed
 * IN struct array_rep_info_t linked_list1
 * IN struct array_rep_info_t linked_list2
 * IN struct array_rep_info_t linked_list3
 * IN struct array_rep_info_t linked_list4
 ****************************************************************/
#ifdef PROTOTYPES
void sum_array_print (
     int seed,
     struct array_rep_info_t linked_list1,
     struct array_rep_info_t linked_list2,
     struct array_rep_info_t linked_list3,
     struct array_rep_info_t linked_list4)
#else
void sum_array_print ( seed, linked_list1, linked_list2, linked_list3,linked_list4)
     int seed;
     struct array_rep_info_t linked_list1;
     struct array_rep_info_t linked_list2;
     struct array_rep_info_t linked_list3;
     struct array_rep_info_t linked_list4;
#endif
{
     int index;
     int sum;

     printf("Sum of 4 arrays, by element (add in seed as well): \n\n");
     printf("Seed: %d\n", seed);
     printf("Element Index | Sum \n");
     printf("-------------------------\n");
     printf("              |          \n");

     for (index = 0; index < 10; index++) {

         sum = seed + linked_list1.values[index] + linked_list2.values[index] +
	       linked_list3.values[index] + linked_list4.values[index];
         printf("%10d%10d\n", index, sum);
     }
}

/*****************************************************************
 * INIT_ARRAY_REP : 
 * IN struct array_rep_info_t *linked_list
 * IN int    seed
 ****************************************************************/
#ifdef PROTOTYPES
void init_array_rep(
     struct array_rep_info_t *linked_list,
     int    seed)
#else
void init_array_rep( linked_list, seed )
     struct array_rep_info_t *linked_list;
     int    seed;
#endif
{

  int index;

  for (index = 0; index < 10; index++) {

      linked_list->values[index] = (2*index) + (seed*2); 
      linked_list->next_index[index] = index + 1;
  }
  linked_list->head = 0; 
}


int main ()  {

  /* variables for array and enumerated type testing
   */
  static char     char_array[121];
  static double   double_array[9];
  static float    float_array[15];
  static int      integer_array[50]; 
  static int      index;
  static id_int   student_id = 23;
  static colors   my_shirt = YELLOW;
    
  /* variables for large structure testing
   */
  static int number = 10;
  static struct array_rep_info_t *list1;
  static struct array_rep_info_t *list2;
  static struct array_rep_info_t *list3;
  static struct array_rep_info_t *list4;

  /* variables for testing a very long argument list
   */
   static double                    a;
   static double                    b;
   static int                       c;
   static int                       d;
   static int                       e;
   static int                       f;

  /* variables for testing a small structures and a very long argument list
   */
   static struct small_rep_info_t  *struct1;
   static struct small_rep_info_t  *struct2;
   static struct small_rep_info_t  *struct3;
   static struct small_rep_info_t  *struct4;
   static struct bit_flags_t       *flags;
   static struct bit_flags_combo_t *flags_combo;
   static struct three_char_t      *three_char;
   static struct five_char_t       *five_char;
   static struct int_char_combo_t  *int_char_combo;
   static struct one_double_t      *d1;
   static struct one_double_t      *d2;
   static struct one_double_t      *d3;
   static struct two_floats_t      *f1;
   static struct two_floats_t      *f2;
   static struct two_floats_t      *f3;

  /* Initialize arrays
   */
  for (index = 0; index < 120; index++) {
      if ((index%2) == 0) char_array[index] = 'Z';
	 else char_array[index] = 'a';
  }
  char_array[120] = '\0';

  for (index = 0; index < 9; index++) {
      double_array[index] = index*23.4567;
  }

  for (index = 0; index < 15; index++) {
      float_array[index] = index/7.02;
  }

  for (index = 0; index < 50; index++) {
      integer_array[index] = -index;
  }

  /* Print arrays
   */
  print_char_array(char_array);
  print_double_array(double_array);
  print_float_array(float_array);
  print_student_id_shirt_color(student_id, my_shirt); 
  print_int_array(integer_array);
  print_all_arrays(integer_array, char_array, float_array, double_array);

  /* Allocate space for large structures 
   */
  list1 = (struct array_rep_info_t *)malloc(sizeof(struct array_rep_info_t));
  list2 = (struct array_rep_info_t *)malloc(sizeof(struct array_rep_info_t));
  list3 = (struct array_rep_info_t *)malloc(sizeof(struct array_rep_info_t));
  list4 = (struct array_rep_info_t *)malloc(sizeof(struct array_rep_info_t));

  /* Initialize large structures 
   */
  init_array_rep(list1, 2);
  init_array_rep(list2, 4);
  init_array_rep(list3, 5);
  init_array_rep(list4, 10);
  printf("HELLO WORLD\n");
  printf("BYE BYE FOR NOW\n");
  printf("VERY GREEN GRASS\n");

  /* Print large structures 
   */
  sum_array_print(10, *list1, *list2, *list3, *list4);
  print_array_rep(*list1, *list2, *list3);
  print_one_large_struct(*list1);

  /* Allocate space for small structures 
   */
  struct1     = (struct small_rep_info_t  *)malloc(sizeof(struct small_rep_info_t));
  struct2     = (struct small_rep_info_t  *)malloc(sizeof(struct small_rep_info_t));
  struct3     = (struct small_rep_info_t  *)malloc(sizeof(struct small_rep_info_t));
  struct4     = (struct small_rep_info_t  *)malloc(sizeof(struct small_rep_info_t));
  flags       = (struct bit_flags_t *)malloc(sizeof(struct bit_flags_t));
  flags_combo = (struct bit_flags_combo_t *)malloc(sizeof(struct bit_flags_combo_t));
  three_char  = (struct three_char_t *)malloc(sizeof(struct three_char_t));
  five_char   = (struct five_char_t *)malloc(sizeof(struct five_char_t));
  int_char_combo = (struct int_char_combo_t *)malloc(sizeof(struct int_char_combo_t));

  d1 = (struct one_double_t *)malloc(sizeof(struct one_double_t));
  d2 = (struct one_double_t *)malloc(sizeof(struct one_double_t));
  d3 = (struct one_double_t *)malloc(sizeof(struct one_double_t));

  f1 = (struct two_floats_t *)malloc(sizeof(struct two_floats_t));
  f2 = (struct two_floats_t *)malloc(sizeof(struct two_floats_t));
  f3 = (struct two_floats_t *)malloc(sizeof(struct two_floats_t));

  /* Initialize small structures 
   */
  init_small_structs ( struct1, struct2, struct3, struct4, flags, 
		       flags_combo, three_char, five_char, int_char_combo,
		       d1, d2, d3, f1, f2, f3);

  /* Print small structures 
   */
  print_small_structs ( *struct1, *struct2, *struct3, *struct4, *flags, 
			*flags_combo, *three_char, *five_char, *int_char_combo,
			*d1, *d2, *d3, *f1, *f2, *f3);

  /* Print a very long arg list 
   */
  a = 22.25;
  b = 33.375;
  c = 0;
  d = -25;
  e = 100;
  f = 2345;

  print_long_arg_list ( a, b, c, d, e, f, *struct1, *struct2, *struct3, *struct4, 
			*flags, *flags_combo, *three_char, *five_char, *int_char_combo,
			*d1, *d2, *d3, *f1, *f2, *f3);

  /* Initialize small structures 
   */
  init_one_double ( d1, 1.11111); 
  init_one_double ( d2, -345.34); 
  init_one_double ( d3, 546464.2); 
  init_two_floats ( f1, 0.234, 453.1); 
  init_two_floats ( f2, 78.345, 23.09); 
  init_two_floats ( f3, -2.345, 1.0); 
  init_bit_flags(flags, (unsigned)1, (unsigned)0, (unsigned)1, 
		 (unsigned)0, (unsigned)1, (unsigned)0 ); 
  init_bit_flags_combo(flags_combo, (unsigned)1, (unsigned)0, 'y',
				     (unsigned)1, (unsigned)0, 'n',
				     (unsigned)1, (unsigned)0 ); 
  init_three_chars(three_char, 'x', 'y', 'z');
  init_five_chars(five_char, 'h', 'e', 'l', 'l', 'o');
  init_int_char_combo(int_char_combo, 13, '!');
  init_struct_rep(struct1, 10);
  init_struct_rep(struct2, 20);
  init_struct_rep(struct3, 30);
  init_struct_rep(struct4, 40);

  compute_with_small_structs(35);
  loop_count();
  printf("HELLO WORLD\n");
  printf("BYE BYE FOR NOW\n");
  printf("VERY GREEN GRASS\n");

  /* Print small structures 
   */
  print_one_double(*d1);
  print_one_double(*d2);
  print_one_double(*d3);
  print_two_floats(*f1);
  print_two_floats(*f2);
  print_two_floats(*f3);
  print_bit_flags(*flags);
  print_bit_flags_combo(*flags_combo);
  print_three_chars(*three_char);
  print_five_chars(*five_char);
  print_int_char_combo(*int_char_combo);
  sum_struct_print(10, *struct1, *struct2, *struct3, *struct4);
  print_struct_rep(*struct1, *struct2, *struct3);

  return 0;
}





