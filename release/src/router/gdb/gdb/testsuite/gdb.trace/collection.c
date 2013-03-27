/*
 * Test program for trace collection
 */

/*
 * Typedefs
 */

typedef struct TEST_STRUCT {
  char   memberc;
  int    memberi;
  float  memberf;
  double memberd;
} test_struct;

typedef int test_array [4];

/* 
 * Global variables to be collected
 */

char         globalc;
int          globali;
float        globalf;
double       globald;
test_struct  globalstruct;
test_struct *globalp;
int          globalarr[16];

/*
 * Additional globals used in arithmetic tests
 */

signed char  c0,  c1,  c2,  c3,  c4,  c5,  c6,  c7, 
             c8,  c9,  c10, c11, c12, c13, c14, c15, cminus;
signed short s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,
             s8,  s9,  s10, s11, s12, s13, s14, s15, sminus;
signed long  l0,  l1,  l2,  l3,  l4,  l5,  l6,  l7,
             l8,  l9,  l10, l11, l12, l13, l14, l15, lminus;


/*
 * Test functions
 */

static void begin ()	/* called before anything else */
{
}

static void end ()	/* called after everything else */
{
}

/* Test collecting args. */
int args_test_func (argc, argi, argf, argd, argstruct, argarray)
     char   argc;
     int    argi;
     float  argf;
     double argd;
     test_struct argstruct;
     int argarray[4];
{
  int i;

  i =  (int) argc + argi + argf + argd + argstruct.memberi + argarray[1];

  return i;
}

/* Test collecting struct args. */
int argstruct_test_func (argstruct)
     test_struct argstruct;
{
  return (int) argstruct.memberc + argstruct.memberi + 
                 argstruct.memberf + argstruct.memberd;
}

/* Test collecting array args. */
int argarray_test_func (argarray)
     int argarray[4];
{
  return (int) argarray[0] + argarray[1] + argarray[2] + argarray[3];
}



int local_test_func ()			/* test collecting locals */
{
  char        locc  = 11;
  int         loci  = 12;
  float       locf  = 13.3;
  double      locd  = 14.4;
  test_struct locst;
  int         locar[4];
  int         i;

  locst.memberc  = 15;
  locst.memberi  = 16;
  locst.memberf  = 17.7;
  locst.memberd  = 18.8;
  locar[0] = 121;
  locar[1] = 122;
  locar[2] = 123;
  locar[3] = 124;

  i = /* Set_Tracepoint_Here */
    (int) locc + loci + locf + locd + locst.memberi + locar[1];

  return i;
}

int reglocal_test_func ()		/* test collecting register locals */
{
  register char        locc = 11;
  register int         loci = 12;
  register float       locf = 13.3;
  register double      locd = 14.4;
  register test_struct locst;
  register int         locar[4];
  int                  i;

  locst.memberc  = 15;
  locst.memberi  = 16;
  locst.memberf  = 17.7;
  locst.memberd  = 18.8;
  locar[0] = 121;
  locar[1] = 122;
  locar[2] = 123;
  locar[3] = 124;

  i = /* Set_Tracepoint_Here */
    (int) locc + loci + locf + locd + locst.memberi + locar[1];

  return i;
}

int statlocal_test_func ()		/* test collecting static locals */
{
  static   char        locc;
  static   int         loci;
  static   float       locf;
  static   double      locd;
  static   test_struct locst;
  static   int         locar[4];
  int                  i;

  locc = 11;
  loci = 12;
  locf = 13.3;
  locd = 14.4;
  locst.memberc = 15;
  locst.memberi = 16;
  locst.memberf = 17.7;
  locst.memberd = 18.8;
  locar[0] = 121;
  locar[1] = 122;
  locar[2] = 123;
  locar[3] = 124;

  i = /* Set_Tracepoint_Here */
    (int) locc + loci + locf + locd + locst.memberi + locar[1];

  /* Set static locals back to zero so collected values are clearly special. */
  locc = 0;
  loci = 0;
  locf = 0;
  locd = 0;
  locst.memberc = 0;
  locst.memberi = 0;
  locst.memberf = 0;
  locst.memberd = 0;
  locar[0] = 0;
  locar[1] = 0;
  locar[2] = 0;
  locar[3] = 0;

  return i;
}


int globals_test_func ()
{
  int i = 0;

  i += globalc + globali + globalf + globald;
  i += globalstruct.memberc + globalstruct.memberi;
  i += globalstruct.memberf + globalstruct.memberd;
  i += globalarr[1];

  return i;	/* Set_Tracepoint_Here */
}

int
main (argc, argv, envp)
     int argc;
     char *argv[], **envp;
{
  int         i = 0;
  test_struct mystruct;
  int         myarray[4];

#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif

  begin ();
  /* Assign collectable values to global variables. */
  l0  = s0  = c0  = 0;     l1  = s1  = c1  = 1;
  l2  = s2  = c2  = 2;     l3  = s3  = c3  = 3;
  l4  = s4  = c4  = 4;     l5  = s5  = c5  = 5;
  l6  = s6  = c6  = 6;     l7  = s7  = c7  = 7;
  l8  = s8  = c8  = 8;     l9  = s9  = c9  = 9;
  l10 = s10 = c10 = 10;    l11 = s11 = c11 = 11;
  l12 = s12 = c12 = 12;    l13 = s13 = c13 = 13;
  l14 = s14 = c14 = 14;    l15 = s15 = c15 = 15;
  lminus = sminus = cminus = -2;
  globalc = 71;
  globali = 72;
  globalf = 73.3;
  globald = 74.4;
  globalstruct.memberc = 81;
  globalstruct.memberi = 82;
  globalstruct.memberf = 83.3;
  globalstruct.memberd = 84.4;
  globalp = &globalstruct;

  for (i = 0; i < 15; i++)
    globalarr[i] = i;

  mystruct.memberc = 101;
  mystruct.memberi = 102;
  mystruct.memberf = 103.3;
  mystruct.memberd = 104.4;
  myarray[0] = 111;
  myarray[1] = 112;
  myarray[2] = 113;
  myarray[3] = 114;

  /* Call test functions, so they can be traced and data collected.  */
  i = 0;
  i += args_test_func (1, 2, 3.3, 4.4, mystruct, myarray);
  i += argstruct_test_func (mystruct);
  i += argarray_test_func (myarray);
  i += local_test_func ();
  i += reglocal_test_func ();
  i += statlocal_test_func ();
  i += globals_test_func ();

  /* Values of globals at end of test should be different from
     values that they had when trace data was captured.  */

  l0  = s0  = c0  = 0;     l1  = s1  = c1  = 0;
  l2  = s2  = c2  = 0;     l3  = s3  = c3  = 0;
  l4  = s4  = c4  = 0;     l5  = s5  = c5  = 0;
  l6  = s6  = c6  = 0;     l7  = s7  = c7  = 0;
  l8  = s8  = c8  = 0;     l9  = s9  = c9  = 0;
  l10 = s10 = c10 = 0;     l11 = s11 = c11 = 0;
  l12 = s12 = c12 = 0;     l13 = s13 = c13 = 0;
  l14 = s14 = c14 = 0;     l15 = s15 = c15 = 0;
  lminus = sminus = cminus = 0;

  /* Set 'em back to zero, so that the collected values will be
     distinctly different from the "realtime" (end of test) values.  */

  globalc = 0;
  globali = 0;
  globalf = 0;
  globald = 0;
  globalstruct.memberc = 0;
  globalstruct.memberi = 0;
  globalstruct.memberf = 0;
  globalstruct.memberd = 0;
  globalp = 0;
  for (i = 0; i < 15; i++)
    globalarr[i] = 0;

  end ();
  return 0;
}
