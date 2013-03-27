/*
 * Test program for trace action commands
 */

static char   gdb_char_test;
static short  gdb_short_test;
static long   gdb_long_test;
static char   gdb_arr_test[25];
static struct GDB_STRUCT_TEST
{
  char   c;
  short  s;
  long   l;
  int    bfield : 11;	/* collect bitfield */
  char   arr[25];
  struct GDB_STRUCT_TEST *next;
} gdb_struct1_test, gdb_struct2_test, *gdb_structp_test, **gdb_structpp_test;

static union GDB_UNION_TEST
{
  char   c;
  short  s;
  long   l;
  int    bfield : 11;	/* collect bitfield */
  char   arr[4];
  union GDB_UNION_TEST *next;
} gdb_union1_test;

void gdb_recursion_test (int, int, int, int,  int,  int,  int);

void gdb_recursion_test (int depth, 
			 int q1, 
			 int q2, 
			 int q3, 
			 int q4, 
			 int q5, 
			 int q6)
{	/* gdb_recursion_test line 0 */
  int q = q1;						/* gdbtestline 1 */

  q1 = q2;						/* gdbtestline 2 */
  q2 = q3;						/* gdbtestline 3 */
  q3 = q4;						/* gdbtestline 4 */
  q4 = q5;						/* gdbtestline 5 */
  q5 = q6;						/* gdbtestline 6 */
  q6 = q;						/* gdbtestline 7 */
  if (depth--)						/* gdbtestline 8 */
    gdb_recursion_test (depth, q1, q2, q3, q4, q5, q6);	/* gdbtestline 9 */
}


unsigned long   gdb_c_test( unsigned long *parm )

{
   char *p = "gdb_c_test";
   char *ridiculously_long_variable_name_with_equally_long_string_assignment;
   register long local_reg = 7;
   static unsigned long local_static, local_static_sizeof;
   long local_long;
   unsigned long *stack_ptr;
   unsigned long end_of_stack;

   ridiculously_long_variable_name_with_equally_long_string_assignment = 
     "ridiculously long variable name with equally long string assignment";
   local_static = 9;
   local_static_sizeof = sizeof (struct GDB_STRUCT_TEST);
   local_long = local_reg + 1;
   stack_ptr  = (unsigned long *) &local_long;
   end_of_stack = 
     (unsigned long) &stack_ptr + sizeof(stack_ptr) + sizeof(end_of_stack) - 1;

   gdb_char_test   = gdb_struct1_test.c = (char)   ((long) parm[1] & 0xff);
   gdb_short_test  = gdb_struct1_test.s = (short)  ((long) parm[2] & 0xffff);
   gdb_long_test   = gdb_struct1_test.l = (long)   ((long) parm[3] & 0xffffffff);
   gdb_union1_test.l = (long) parm[4];
   gdb_arr_test[0] = gdb_struct1_test.arr[0] = (char) ((long) parm[1] & 0xff);
   gdb_arr_test[1] = gdb_struct1_test.arr[1] = (char) ((long) parm[2] & 0xff);
   gdb_arr_test[2] = gdb_struct1_test.arr[2] = (char) ((long) parm[3] & 0xff);
   gdb_arr_test[3] = gdb_struct1_test.arr[3] = (char) ((long) parm[4] & 0xff);
   gdb_arr_test[4] = gdb_struct1_test.arr[4] = (char) ((long) parm[5] & 0xff);
   gdb_arr_test[5] = gdb_struct1_test.arr[5] = (char) ((long) parm[6] & 0xff);
   gdb_struct1_test.bfield = 144;
   gdb_struct1_test.next = &gdb_struct2_test;
   gdb_structp_test      = &gdb_struct1_test;
   gdb_structpp_test     = &gdb_structp_test;

   gdb_recursion_test (3, (long) parm[1], (long) parm[2], (long) parm[3],
		       (long) parm[4], (long) parm[5], (long) parm[6]);

   gdb_char_test = gdb_short_test = gdb_long_test = 0;
   gdb_structp_test  = (void *) 0;
   gdb_structpp_test = (void *) 0;
   memset ((char *) &gdb_struct1_test, 0, sizeof (gdb_struct1_test));
   memset ((char *) &gdb_struct2_test, 0, sizeof (gdb_struct2_test));
   local_static_sizeof = 0;
   local_static = 0;
   return ( (unsigned long) 0 );
}

static void gdb_asm_test (void)
{
}

static void begin ()	/* called before anything else */
{
}

static void end ()	/* called after everything else */
{
}

int
main (argc, argv, envp)
     int argc;
     char *argv[], **envp;
{
  int i;
  unsigned long myparms[10];

#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif

  begin ();
  for (i = 0; i < sizeof (myparms) / sizeof (myparms[0]); i++)
    myparms[i] = i;

  gdb_c_test (&myparms[0]);

  end ();
  return 0;
}

