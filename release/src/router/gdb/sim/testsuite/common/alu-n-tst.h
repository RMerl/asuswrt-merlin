#ifndef N
#error "N must be #defined"
#endif

#include "symcat.h"

/* NOTE: see end of file for #undef of these macros */
#define unsignedN    XCONCAT2(unsigned,N)
#define MAX_INT      XCONCAT2(MAX_INT,N)
#define MIN_INT      XCONCAT2(MIN_INT,N)
#define alu_N_tests     XCONCAT3(alu_,N,_tests)
#define do_alu_N_tests  XCONCAT3(do_alu_,N,_tests)
#define OP_BEGIN     XCONCAT3(ALU,N,_BEGIN)
#define OP_ADDC      XCONCAT3(ALU,N,_ADDC)
#define OP_ADDC_C    XCONCAT3(ALU,N,_ADDC_C)
#define OP_SUBC      XCONCAT3(ALU,N,_SUBC)
#define OP_SUBB      XCONCAT3(ALU,N,_SUBB)
#define OP_SUBB_B    XCONCAT3(ALU,N,_SUBB_B)
#define OP_SUBC_X    XCONCAT3(ALU,N,_SUBC_X)
#define OP_NEGC      XCONCAT3(ALU,N,_NEGC)
#define OP_NEGB      XCONCAT3(ALU,N,_NEGB)
#define HAD_CARRY_BORROW    (XCONCAT3(ALU,N,_HAD_CARRY_BORROW) != 0)
#define HAD_OVERFLOW (XCONCAT3(ALU,N,_HAD_OVERFLOW) != 0)
#define RESULT          XCONCAT3(ALU,N,_RESULT)
#define CARRY_BORROW_RESULT    XCONCAT3(ALU,N,_CARRY_BORROW_RESULT)
#define OVERFLOW_RESULT XCONCAT3(ALU,N,_OVERFLOW_RESULT)
#define do_op_N      XCONCAT2(do_op_,N)


void
do_op_N (const alu_test *tst)
{
  const alu_op *op;
  int borrow_p = 0;
  OP_BEGIN (tst->begin);
  print_hex (tst->begin, N);
  for (op = tst->ops; op->op != NULL; op++)
    {
      printf (" %s ", op->op);
      print_hex (op->arg, N);
      if (strcmp (op->op, "ADDC") == 0)
	OP_ADDC (op->arg);
      else if (strcmp (op->op, "ADDC_C0") == 0)
	OP_ADDC_C (op->arg, 0);
      else if (strcmp (op->op, "ADDC_C1") == 0)
	OP_ADDC_C (op->arg, 1);
      else if (strcmp (op->op, "SUBC") == 0)
	OP_SUBC (op->arg);
      else if (strcmp (op->op, "SUBC_X0") == 0)
	OP_SUBC_X (op->arg, 0);
      else if (strcmp (op->op, "SUBC_X1") == 0)
	OP_SUBC_X (op->arg, 1);
      else if (strcmp (op->op, "SUBB") == 0)
	{
	  OP_SUBB (op->arg);
	  borrow_p ++;
	}
      else if (strcmp (op->op, "SUBB_B0") == 0)
	{
	  OP_SUBB_B (op->arg, 0);
	  borrow_p ++;
	}
      else if (strcmp (op->op, "SUBB_B1") == 0)
	{
	  OP_SUBB_B (op->arg, 1);
	  borrow_p ++;
	}
      else if (strcmp (op->op, "NEGC") == 0)
	OP_NEGC ();
      else if (strcmp (op->op, "NEGB") == 0)
	{
	  OP_NEGB ();
	  borrow_p ++;
	}
      else
	{
	  printf (" -- operator unknown\n");
	  abort ();
	}
    }
  printf (" = ");
  print_hex (tst->result, N);
  if (borrow_p)
    printf (" B:%d", tst->carry_borrow);
  else
    printf (" C:%d", tst->carry_borrow);
  printf (" V:%d", tst->overflow);
  if (tst->carry_borrow != HAD_CARRY_BORROW)
    {
      if (borrow_p)
	printf (" -- borrow (%d) wrong", HAD_CARRY_BORROW);
      else
	printf (" -- carry (%d) wrong", HAD_CARRY_BORROW);
      errors ++;
    }
  if (tst->overflow != HAD_OVERFLOW)
    {
      printf (" -- overflow (%d) wrong", HAD_OVERFLOW);
      errors ++;
    }
  if ((unsignedN) CARRY_BORROW_RESULT != (unsignedN) tst->result)
    {
      printf (" -- result for carry/borrow wrong ");
      print_hex (CARRY_BORROW_RESULT, N);
      errors ++;
    }
  if ((unsignedN) OVERFLOW_RESULT != (unsignedN) tst->result)
    {
      printf (" -- result for overflow wrong ");
      print_hex (OVERFLOW_RESULT, N);
      errors ++;
    }
  if ((unsignedN) RESULT != (unsignedN) tst->result)
    {
      printf (" -- result wrong ");
      print_hex (RESULT, N);
      errors ++;
    }
  printf ("\n");
}


const alu_test alu_N_tests[] = {

  /* 0 + 0; 0 + 1; 1 + 0; 1 + 1 */
  { 0, { { "ADDC", 0 }, }, 0, 0, 0, },
  { 0, { { "ADDC", 1 }, }, 1, 0, 0, },
  { 1, { { "ADDC", 0 }, }, 1, 0, 0, },
  { 1, { { "ADDC", 1 }, }, 2, 0, 0, },

  /* 0 + 0 + 0; 0 + 0 + 1; 0 + 1 + 0; 0 + 1 + 1 */
  /* 1 + 0 + 0; 1 + 0 + 1; 1 + 1 + 0; 1 + 1 + 1 */
  { 0, { { "ADDC_C0", 0 }, }, 0, 0, 0, },
  { 0, { { "ADDC_C0", 1 }, }, 1, 0, 0, },
  { 0, { { "ADDC_C1", 0 }, }, 1, 0, 0, },
  { 0, { { "ADDC_C1", 1 }, }, 2, 0, 0, },
  { 1, { { "ADDC_C0", 0 }, }, 1, 0, 0, },
  { 1, { { "ADDC_C0", 1 }, }, 2, 0, 0, },
  { 1, { { "ADDC_C1", 0 }, }, 2, 0, 0, },
  { 1, { { "ADDC_C1", 1 }, }, 3, 0, 0, },

  /* */
  { MAX_INT, { { "ADDC", 1 }, }, MIN_INT, 0, 1, },
  { MIN_INT, { { "ADDC", -1 }, }, MAX_INT, 1, 1, },
  { MAX_INT, { { "ADDC", MIN_INT }, }, -1, 0, 0, },
  { MIN_INT, { { "ADDC", MAX_INT }, }, -1, 0, 0, },
  { MAX_INT, { { "ADDC", MAX_INT }, }, MAX_INT << 1, 0, 1, },
  { MIN_INT, { { "ADDC", MIN_INT }, }, 0, 1, 1, },
  /* */
  { 0, { { "ADDC_C1", -1 }, }, 0, 1, 0, },
  { 0, { { "ADDC_C1", -2 }, }, -1, 0, 0, },
  { -1, { { "ADDC_C1", 0 }, }, 0, 1, 0, },
  { 0, { { "ADDC_C0", 0 }, }, 0, 0, 0, },
  { -1, { { "ADDC_C1", -1 }, }, -1, 1, 0, },
  { -1, { { "ADDC_C1", 1 }, }, 1, 1, 0, },
  { 0, { { "ADDC_C1", MAX_INT }, }, MIN_INT, 0, 1, },
  { MAX_INT, { { "ADDC_C1", 1 }, }, MIN_INT + 1, 0, 1, },
  { MAX_INT, { { "ADDC_C1", MIN_INT }, }, 0, 1, 0, },
  { MAX_INT, { { "ADDC_C1", MAX_INT }, }, (MAX_INT << 1) + 1, 0, 1, },
  { MAX_INT, { { "ADDC_C0", MAX_INT }, }, MAX_INT << 1, 0, 1, },

  /* 0 - 0 */
  { 0, { { "SUBC",    0 }, },  0, 1, 0, },
  { 0, { { "SUBB",    0 }, },  0, 0, 0, },

  /* 0 - 1 */
  { 0, { { "SUBC",    1 }, }, -1, 0, 0, },
  { 0, { { "SUBB",    1 }, }, -1, 1, 0, },

  /* 1 - 0 */
  { 1, { { "SUBC",    0 }, },  1, 1, 0, },
  { 1, { { "SUBB",    0 }, },  1, 0, 0, },

  /* 1 - 1 */
  { 1, { { "SUBC",    1 }, },  0, 1, 0, },
  { 1, { { "SUBB",    1 }, },  0, 0, 0, },

  /* 0 - 0 - 0 */
  { 0, { { "SUBC_X0", 0 }, }, -1, 0, 0, },
  { 0, { { "SUBB_B0", 0 }, },  0, 0, 0, },

  /* 0 - 0 - 1 */
  { 0, { { "SUBC_X0", 1 }, }, -2, 0, 0, },
  { 0, { { "SUBB_B0", 1 }, }, -1, 1, 0, },

  /* 0 - 1 - 0 */
  { 0, { { "SUBC_X1", 0 }, },  0, 1, 0, },
  { 0, { { "SUBB_B1", 0 }, }, -1, 1, 0, },

  /* 0 - 1 - 1 */
  { 0, { { "SUBC_X1", 1 }, }, -1, 0, 0, },
  { 0, { { "SUBB_B1", 1 }, }, -2, 1, 0, },

  /* 1 - 0 - 0 */
  { 1, { { "SUBC_X0", 0 }, },  0, 1, 0, },
  { 1, { { "SUBB_B0", 0 }, },  1, 0, 0, },

  /* 1 - 0 - 1 */
  { 1, { { "SUBC_X0", 1 }, }, -1, 0, 0, },
  { 1, { { "SUBB_B0", 1 }, },  0, 0, 0, },

  /* 1 - 1 - 0 */
  { 1, { { "SUBC_X1", 0 }, },  1, 1, 0, },
  { 1, { { "SUBB_B1", 0 }, },  0, 0, 0, },

  /* 1 - 1 - 1 */
  { 1, { { "SUBC_X1", 1 }, },  0, 1, 0, },
  { 1, { { "SUBB_B1", 1 }, }, -1, 1, 0, },

  /* */
  { 0,       { { "SUBC", MIN_INT }, }, MIN_INT, 0, 1, },
  { MIN_INT, { { "SUBC", 1 }, }, MAX_INT, 1, 1, },
  { MAX_INT, { { "SUBC", MAX_INT }, }, 0, 1, 0, },

  /* */
  { 0,       { { "SUBC_X0", MIN_INT }, }, MAX_INT, 0, 0, },
  { MIN_INT, { { "SUBC_X1",       0 }, }, MIN_INT, 1, 0, },
  { MAX_INT, { { "SUBC_X0", MAX_INT }, },      -1, 0, 0, },

  /* */
  { MAX_INT, { { "NEGC", 0 }, }, MIN_INT + 1, 0, 0, },
  { MAX_INT, { { "NEGC", 0 }, }, MIN_INT + 1, 0, 0, },
  { MIN_INT, { { "NEGC", 0 }, }, MIN_INT, 0, 1, },
  { 0, { { "NEGC", 0 }, }, 0, 1, 0, },
  { -1, { { "NEGC", 0 }, }, 1, 0, 0, },
  { 1, { { "NEGC", 0 }, }, -1, 0, 0, },
};


static void
do_alu_N_tests (void)
{
  int i;
  for (i = 0; i < sizeof (alu_N_tests) / sizeof (*alu_N_tests); i++)
    {
      const alu_test *tst = &alu_N_tests[i];
      do_op_N (tst);
    }
}


#undef OP_BEGIN
#undef OP_ADDC
#undef OP_ADDC_C
#undef OP_SUBB
#undef OP_SUBC
#undef OP_SUBC_X
#undef OP_SUBB_B
#undef HAD_OVERFLOW
#undef HAD_CARRY_BORROW
#undef OVERFLOW_RESULT
#undef CARRY_BORROW_RESULT
#undef RESULT
#undef do_op_N
#undef unsignedN
#undef MAX_INT
#undef MIN_INT
#undef alu_N_tests
#undef do_alu_N_tests

