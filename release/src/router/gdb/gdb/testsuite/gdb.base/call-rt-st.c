#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**************************************************************************
 * TESTS :
 * function returning large structures, which go on the stack
 * functions returning varied sized structs which go on in the registers.
 ***************************************************************************/


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

/* 6 bits : really fits in 8 bits and is promoted to 8 bits
 */
struct bit_flags_char_t {
       unsigned char alpha   :1;
       unsigned char beta    :1;
       unsigned char gamma   :1;
       unsigned char delta   :1;
       unsigned char epsilon :1;
       unsigned char omega   :1;
};

/* 6 bits : really fits in 8 bits and is promoted to 16 bits
 */
struct bit_flags_short_t {
       unsigned short alpha   :1;
       unsigned short beta    :1;
       unsigned short gamma   :1;
       unsigned short delta   :1;
       unsigned short epsilon :1;
       unsigned short omega   :1;
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
 * LOOP_COUNT : 
 * A do nothing function. Used to provide a point at which calls can be made.  
 *****************************************************************/
void loop_count () {

     int index;

     for (index=0; index<4; index++);
}

/*****************************************************************
 * INIT_BIT_FLAGS_CHAR :
 * Initializes a bit_flags_char_t structure. Can call this function see
 * the call command behavior when integer arguments do not fit into
 * registers and must be placed on the stack.
 * OUT struct bit_flags_char_t *bit_flags -- structure to be filled
 * IN  unsigned a  -- 0 or 1 
 * IN  unsigned b  -- 0 or 1 
 * IN  unsigned g  -- 0 or 1 
 * IN  unsigned d  -- 0 or 1 
 * IN  unsigned e  -- 0 or 1 
 * IN  unsigned o  -- 0 or 1 
 *****************************************************************/
#ifdef PROTOTYPES
void init_bit_flags_char (
struct bit_flags_char_t *bit_flags,
unsigned a,
unsigned b,
unsigned g,
unsigned d,
unsigned e,
unsigned o)
#else
void init_bit_flags_char (bit_flags,a,b,g,d,e,o) 
struct bit_flags_char_t *bit_flags;
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
 * INIT_BIT_FLAGS_SHORT :
 * Initializes a bit_flags_short_t structure. Can call this function see
 * the call command behavior when integer arguments do not fit into
 * registers and must be placed on the stack.
 * OUT struct bit_flags_short_t *bit_flags -- structure to be filled
 * IN  unsigned a  -- 0 or 1 
 * IN  unsigned b  -- 0 or 1 
 * IN  unsigned g  -- 0 or 1 
 * IN  unsigned d  -- 0 or 1 
 * IN  unsigned e  -- 0 or 1 
 * IN  unsigned o  -- 0 or 1 
 *****************************************************************/
#ifdef PROTOTYPES
void init_bit_flags_short (
struct bit_flags_short_t *bit_flags,
unsigned a,
unsigned b,
unsigned g,
unsigned d,
unsigned e,
unsigned o)
#else
void init_bit_flags_short (bit_flags,a,b,g,d,e,o) 
struct bit_flags_short_t *bit_flags;
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
void init_bit_flags (
struct bit_flags_t *bit_flags,
unsigned a,
unsigned b,
unsigned g,
unsigned d,
unsigned e,
unsigned o)
#else
void init_bit_flags (bit_flags,a,b,g,d,e,o) 
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
void init_bit_flags_combo (
struct bit_flags_combo_t *bit_flags_combo,
unsigned a,
unsigned b,
char ch1,
unsigned g,
unsigned d,
char ch2,
unsigned e,
unsigned o)
#else
void init_bit_flags_combo (bit_flags_combo, a, b, ch1, g, d, ch2, e, o)
struct bit_flags_combo_t *bit_flags_combo;
unsigned a;
unsigned b;
char ch1;
unsigned g;
unsigned d;
char ch2;
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
void init_one_double ( struct one_double_t *one_double, double init_val)
#else
void init_one_double (one_double, init_val)
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
void init_two_floats (
     struct two_floats_t *two_floats,
     float init_val1,
     float init_val2)
#else
void init_two_floats (two_floats, init_val1, init_val2)
struct two_floats_t *two_floats;
float init_val1;
float init_val2;
#endif
{

     two_floats->float1 = init_val1;
     two_floats->float2 = init_val2;
}

/*****************************************************************
 * INIT_THREE_CHARS : 
 * OUT struct three_char_t *three_char -- structure to be filled
 * IN  char init_val1 
 * IN  char init_val2 
 * IN  char init_val3 
 *****************************************************************/
#ifdef PROTOTYPES
void init_three_chars (
struct three_char_t *three_char,
char init_val1,
char init_val2,
char init_val3)
#else
void init_three_chars ( three_char, init_val1, init_val2, init_val3)
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
void init_five_chars (
struct five_char_t *five_char,
char init_val1,
char init_val2,
char init_val3,
char init_val4,
char init_val5)
#else
void init_five_chars ( five_char, init_val1, init_val2, init_val3, init_val4, init_val5)
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
void init_int_char_combo (
struct int_char_combo_t *combo,
int init_val1,
char init_val2)
#else
void init_int_char_combo ( combo, init_val1, init_val2)
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
void init_struct_rep(
     struct small_rep_info_t *small_struct,
     int seed)
#else
void init_struct_rep( small_struct, seed)
struct small_rep_info_t *small_struct;
int    seed;
#endif
{

      small_struct->value = 2 + (seed*2); 
      small_struct->head = 0; 
}

/*****************************************************************
 * PRINT_BIT_FLAGS_CHAR : 
 * IN struct bit_flags_char_t bit_flags 
 ****************************************************************/
#ifdef PROTOTYPES
struct bit_flags_char_t print_bit_flags_char (struct bit_flags_char_t bit_flags)
#else
struct bit_flags_char_t print_bit_flags_char ( bit_flags)
struct bit_flags_char_t bit_flags;
#endif
{

     if (bit_flags.alpha) printf("alpha\n");
     if (bit_flags.beta) printf("beta\n");
     if (bit_flags.gamma) printf("gamma\n");
     if (bit_flags.delta) printf("delta\n");
     if (bit_flags.epsilon) printf("epsilon\n");
     if (bit_flags.omega) printf("omega\n");
     return bit_flags;
     
}

/*****************************************************************
 * PRINT_BIT_FLAGS_SHORT : 
 * IN struct bit_flags_short_t bit_flags 
 ****************************************************************/
#ifdef PROTOTYPES
struct bit_flags_short_t print_bit_flags_short (struct bit_flags_short_t bit_flags)
#else
struct bit_flags_short_t print_bit_flags_short ( bit_flags)
struct bit_flags_short_t bit_flags;
#endif
{

     if (bit_flags.alpha) printf("alpha\n");
     if (bit_flags.beta) printf("beta\n");
     if (bit_flags.gamma) printf("gamma\n");
     if (bit_flags.delta) printf("delta\n");
     if (bit_flags.epsilon) printf("epsilon\n");
     if (bit_flags.omega) printf("omega\n");
     return bit_flags;
     
}

/*****************************************************************
 * PRINT_BIT_FLAGS : 
 * IN struct bit_flags_t bit_flags 
 ****************************************************************/
#ifdef PROTOTYPES
struct bit_flags_t print_bit_flags (struct bit_flags_t bit_flags)
#else
struct bit_flags_t print_bit_flags ( bit_flags)
struct bit_flags_t bit_flags;
#endif
{

     if (bit_flags.alpha) printf("alpha\n");
     if (bit_flags.beta) printf("beta\n");
     if (bit_flags.gamma) printf("gamma\n");
     if (bit_flags.delta) printf("delta\n");
     if (bit_flags.epsilon) printf("epsilon\n");
     if (bit_flags.omega) printf("omega\n");
     return bit_flags;
     
}

/*****************************************************************
 * PRINT_BIT_FLAGS_COMBO : 
 * IN struct bit_flags_combo_t bit_flags_combo 
 ****************************************************************/
#ifdef PROTOTYPES
struct bit_flags_combo_t print_bit_flags_combo (struct bit_flags_combo_t bit_flags_combo)
#else
struct bit_flags_combo_t print_bit_flags_combo ( bit_flags_combo )
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
     return bit_flags_combo;
     
}

/*****************************************************************
 * PRINT_ONE_DOUBLE : 
 * IN struct one_double_t one_double 
 ****************************************************************/
#ifdef PROTOTYPES
struct one_double_t print_one_double (struct one_double_t one_double)
#else
struct one_double_t print_one_double ( one_double )
struct one_double_t one_double;
#endif
{

     printf("Contents of one_double_t: \n\n");
     printf("%f\n", one_double.double1);
     return one_double;
     
}

/*****************************************************************
 * PRINT_TWO_FLOATS : 
 * IN struct two_floats_t two_floats 
 ****************************************************************/
#ifdef PROTOTYPES
struct two_floats_t print_two_floats (struct two_floats_t two_floats)
#else
struct two_floats_t print_two_floats ( two_floats ) 
struct two_floats_t two_floats;
#endif
{

     printf("Contents of two_floats_t: \n\n");
     printf("%f\t%f\n", two_floats.float1, two_floats.float2);
     return two_floats;
     
}

/*****************************************************************
 * PRINT_THREE_CHARS : 
 * IN struct three_char_t three_char
 ****************************************************************/
#ifdef PROTOTYPES
struct three_char_t print_three_chars (struct three_char_t three_char)
#else
struct three_char_t print_three_chars ( three_char ) 
struct three_char_t three_char;
#endif
{

     printf("Contents of three_char_t: \n\n");
     printf("%c\t%c\t%c\n", three_char.ch1, three_char.ch2, three_char.ch3);
     return three_char;
     
}

/*****************************************************************
 * PRINT_FIVE_CHARS : 
 * IN struct five_char_t five_char
 ****************************************************************/
#ifdef PROTOTYPES
struct five_char_t print_five_chars (struct five_char_t five_char)
#else
struct five_char_t print_five_chars ( five_char )
struct five_char_t five_char;
#endif
{

     printf("Contents of five_char_t: \n\n");
     printf("%c\t%c\t%c\t%c\t%c\n", five_char.ch1, five_char.ch2, 
				    five_char.ch3, five_char.ch4, 
				    five_char.ch5);
     return five_char;
     
}

/*****************************************************************
 * PRINT_INT_CHAR_COMBO : 
 * IN struct int_char_combo_t int_char_combo
 ****************************************************************/
#ifdef PROTOTYPES
struct int_char_combo_t print_int_char_combo (struct int_char_combo_t int_char_combo)
#else
struct int_char_combo_t print_int_char_combo ( int_char_combo )
struct int_char_combo_t int_char_combo;
#endif
{

     printf("Contents of int_char_combo_t: \n\n");
     printf("%d\t%c\n", int_char_combo.int1, int_char_combo.ch1);
     return int_char_combo;
     
}     

/*****************************************************************
 * PRINT_STRUCT_REP : 
 ****************************************************************/
#ifdef PROTOTYPES
struct small_rep_info_t print_struct_rep(struct small_rep_info_t struct1)
#else
struct small_rep_info_t print_struct_rep( struct1 )
struct small_rep_info_t struct1;
#endif
{

  printf("Contents of struct1: \n\n");
  printf("%10d%10d\n", struct1.value, struct1.head); 
  struct1.value =+5;
  
  return struct1;
  

}


#ifdef PROTOTYPES
struct array_rep_info_t print_one_large_struct(struct array_rep_info_t linked_list1)
#else
struct array_rep_info_t print_one_large_struct( linked_list1 )
struct array_rep_info_t linked_list1;
#endif
{


      printf("%10d%10d\n", linked_list1.values[0], 
			   linked_list1.next_index[0]); 
 
      return linked_list1;
      
}

/*****************************************************************
 * INIT_ARRAY_REP :
 * IN struct array_rep_info_t *linked_list
 * IN int    seed
 ****************************************************************/
#ifdef PROTOTYPES
void init_array_rep(struct array_rep_info_t *linked_list, int seed)
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
   
  /* variables for large structure testing
   */
  int number = 10;
  struct array_rep_info_t *list1;

  /* variables for testing a small structures and a very long argument list
   */
   struct small_rep_info_t  *struct1;
   struct bit_flags_char_t  *cflags;
   struct bit_flags_short_t *sflags;
   struct bit_flags_t       *flags;
   struct bit_flags_combo_t *flags_combo;
   struct three_char_t      *three_char;
   struct five_char_t       *five_char;
   struct int_char_combo_t  *int_char_combo;
   struct one_double_t      *d1;
   struct two_floats_t      *f3;


  /* Allocate space for large structures 
   */
  list1 = (struct array_rep_info_t *)malloc(sizeof(struct array_rep_info_t));

  /* Initialize large structures 
   */
  init_array_rep(list1, 2);

  /* Print large structures 
   */
  print_one_large_struct(*list1);

  /* Allocate space for small structures 
   */
  struct1     = (struct small_rep_info_t  *)malloc(sizeof(struct small_rep_info_t));
  cflags       = (struct bit_flags_char_t *)malloc(sizeof(struct bit_flags_char_t));
  sflags       = (struct bit_flags_short_t *)malloc(sizeof(struct bit_flags_short_t));
  flags       = (struct bit_flags_t *)malloc(sizeof(struct bit_flags_t));
  flags_combo = (struct bit_flags_combo_t *)malloc(sizeof(struct bit_flags_combo_t));
  three_char  = (struct three_char_t *)malloc(sizeof(struct three_char_t));
  five_char   = (struct five_char_t *)malloc(sizeof(struct five_char_t));
  int_char_combo = (struct int_char_combo_t *)malloc(sizeof(struct int_char_combo_t));

  d1 = (struct one_double_t *)malloc(sizeof(struct one_double_t));
  f3 = (struct two_floats_t *)malloc(sizeof(struct two_floats_t));

  /* Initialize small structures 
   */
  init_one_double ( d1, 1.11111); 
  init_two_floats ( f3, -2.345, 1.0); 
  init_bit_flags_char(cflags, (unsigned)1, (unsigned)0, (unsigned)1, 
		      (unsigned)0, (unsigned)1, (unsigned)0 ); 
  init_bit_flags_short(sflags, (unsigned)1, (unsigned)0, (unsigned)1, 
		       (unsigned)0, (unsigned)1, (unsigned)0 ); 
  init_bit_flags(flags, (unsigned)1, (unsigned)0, (unsigned)1, 
		 (unsigned)0, (unsigned)1, (unsigned)0 ); 
  init_bit_flags_combo(flags_combo, (unsigned)1, (unsigned)0, 'y',
				     (unsigned)1, (unsigned)0, 'n',
				     (unsigned)1, (unsigned)0 ); 
  init_three_chars(three_char, 'x', 'y', 'z');
  init_five_chars(five_char, 'h', 'e', 'l', 'l', 'o');
  init_int_char_combo(int_char_combo, 13, '!');
  init_struct_rep(struct1, 10);
 
 
  /* Print small structures 
   */
  print_one_double(*d1);
  print_two_floats(*f3);
  print_bit_flags_char(*cflags);
  print_bit_flags_short(*sflags);
  print_bit_flags(*flags);
  print_bit_flags_combo(*flags_combo);
  print_three_chars(*three_char);
  print_five_chars(*five_char);
  print_int_char_combo(*int_char_combo);
  print_struct_rep(*struct1);

  loop_count();

  return 0;
}















