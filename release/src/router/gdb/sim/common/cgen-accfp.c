/* Accurate fp support for CGEN-based simulators.
   Copyright (C) 1999 Cygnus Solutions.

   This implemention assumes:
   typedef USI SF;
   typedef UDI DF;

   TODO:
   - lazy encoding/decoding
   - checking return code (say by callback)
   - proper rounding
*/

#include "sim-main.h"
#include "sim-fpu.h"

/* SF mode support */

static SF
addsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  status = sim_fpu_add (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
subsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  status = sim_fpu_sub (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
mulsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  status = sim_fpu_mul (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
divsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  status = sim_fpu_div (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
negsf (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  status = sim_fpu_neg (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
abssf (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  status = sim_fpu_abs (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
sqrtsf (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  status = sim_fpu_sqrt (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
invsf (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  status = sim_fpu_inv (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
minsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  status = sim_fpu_min (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static SF
maxsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned32 res;
  sim_fpu_status status;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  status = sim_fpu_max (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to32 (&res, &ans);

  return res;
}

static CGEN_FP_CMP
cmpsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);

  if (sim_fpu_is_nan (&op1)
      || sim_fpu_is_nan (&op2))
    return FP_CMP_NAN;

  if (x < y)
    return FP_CMP_LT;
  if (x > y)
    return FP_CMP_GT;
  return FP_CMP_EQ;
}

static int
eqsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  return sim_fpu_is_eq (&op1, &op2);
}

static int
nesf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  return sim_fpu_is_ne (&op1, &op2);
}

static int
ltsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  return sim_fpu_is_lt (&op1, &op2);
}

static int
lesf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  return sim_fpu_is_le (&op1, &op2);
}

static int
gtsf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  return sim_fpu_is_gt (&op1, &op2);
}

static int
gesf (CGEN_FPU* fpu, SF x, SF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_32to (&op1, x);
  sim_fpu_32to (&op2, y);
  return sim_fpu_is_ge (&op1, &op2);
}

static DF
fextsfdf (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;
  unsigned64 res;

  sim_fpu_32to (&op1, x);
  sim_fpu_to64 (&res, &op1);

  return res;
}

static SF
ftruncdfsf (CGEN_FPU* fpu, DF x)
{
  sim_fpu op1;
  unsigned32 res;

  sim_fpu_64to (&op1, x);
  sim_fpu_to32 (&res, &op1);

  return res;
}

static SF
floatsisf (CGEN_FPU* fpu, SI x)
{
  sim_fpu ans;
  unsigned32 res;

  sim_fpu_i32to (&ans, x, sim_fpu_round_near);
  sim_fpu_to32 (&res, &ans);
  return res;
}

static DF
floatsidf (CGEN_FPU* fpu, SI x)
{
  sim_fpu ans;
  unsigned64 res;

  sim_fpu_i32to (&ans, x, sim_fpu_round_near);
  sim_fpu_to64 (&res, &ans);
  return res;
}

static SF
ufloatsisf (CGEN_FPU* fpu, USI x)
{
  sim_fpu ans;
  unsigned32 res;

  sim_fpu_u32to (&ans, x, sim_fpu_round_near);
  sim_fpu_to32 (&res, &ans);
  return res;
}

static SI
fixsfsi (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;
  unsigned32 res;

  sim_fpu_32to (&op1, x);
  sim_fpu_to32i (&res, &op1, sim_fpu_round_near);
  return res;
}

static SI
fixdfsi (CGEN_FPU* fpu, DF x)
{
  sim_fpu op1;
  unsigned32 res;

  sim_fpu_64to (&op1, x);
  sim_fpu_to32i (&res, &op1, sim_fpu_round_near);
  return res;
}

static USI
ufixsfsi (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;
  unsigned32 res;

  sim_fpu_32to (&op1, x);
  sim_fpu_to32u (&res, &op1, sim_fpu_round_near);
  return res;
}

/* DF mode support */

static DF
adddf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  status = sim_fpu_add (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
subdf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  status = sim_fpu_sub (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
muldf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  status = sim_fpu_mul (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
divdf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  status = sim_fpu_div (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
negdf (CGEN_FPU* fpu, DF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  status = sim_fpu_neg (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
absdf (CGEN_FPU* fpu, DF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  status = sim_fpu_abs (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
sqrtdf (CGEN_FPU* fpu, DF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  status = sim_fpu_sqrt (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
invdf (CGEN_FPU* fpu, DF x)
{
  sim_fpu op1;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  status = sim_fpu_inv (&ans, &op1);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
mindf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  status = sim_fpu_min (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static DF
maxdf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;
  sim_fpu ans;
  unsigned64 res;
  sim_fpu_status status;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  status = sim_fpu_max (&ans, &op1, &op2);
  if (status != 0)
    (*fpu->ops->error) (fpu, status);
  sim_fpu_to64 (&res, &ans);

  return res;
}

static CGEN_FP_CMP
cmpdf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);

  if (sim_fpu_is_nan (&op1)
      || sim_fpu_is_nan (&op2))
    return FP_CMP_NAN;

  if (x < y)
    return FP_CMP_LT;
  if (x > y)
    return FP_CMP_GT;
  return FP_CMP_EQ;
}

static int
eqdf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  return sim_fpu_is_eq (&op1, &op2);
}

static int
nedf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  return sim_fpu_is_ne (&op1, &op2);
}

static int
ltdf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  return sim_fpu_is_lt (&op1, &op2);
}

static int
ledf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  return sim_fpu_is_le (&op1, &op2);
}

static int
gtdf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  return sim_fpu_is_gt (&op1, &op2);
}

static int
gedf (CGEN_FPU* fpu, DF x, DF y)
{
  sim_fpu op1;
  sim_fpu op2;

  sim_fpu_64to (&op1, x);
  sim_fpu_64to (&op2, y);
  return sim_fpu_is_ge (&op1, &op2);
}

/* Initialize FP_OPS to use accurate library.  */

void
cgen_init_accurate_fpu (SIM_CPU* cpu, CGEN_FPU* fpu, CGEN_FPU_ERROR_FN* error)
{
  CGEN_FP_OPS* o;

  fpu->owner = cpu;
  /* ??? small memory leak, not freed by sim_close */
  fpu->ops = (CGEN_FP_OPS*) xmalloc (sizeof (CGEN_FP_OPS));

  o = fpu->ops;
  memset (o, 0, sizeof (*o));

  o->error = error;

  o->addsf = addsf;
  o->subsf = subsf;
  o->mulsf = mulsf;
  o->divsf = divsf;
  o->negsf = negsf;
  o->abssf = abssf;
  o->sqrtsf = sqrtsf;
  o->invsf = invsf;
  o->minsf = minsf;
  o->maxsf = maxsf;
  o->cmpsf = cmpsf;
  o->eqsf = eqsf;
  o->nesf = nesf;
  o->ltsf = ltsf;
  o->lesf = lesf;
  o->gtsf = gtsf;
  o->gesf = gesf;

  o->adddf = adddf;
  o->subdf = subdf;
  o->muldf = muldf;
  o->divdf = divdf;
  o->negdf = negdf;
  o->absdf = absdf;
  o->sqrtdf = sqrtdf;
  o->invdf = invdf;
  o->mindf = mindf;
  o->maxdf = maxdf;
  o->cmpdf = cmpdf;
  o->eqdf = eqdf;
  o->nedf = nedf;
  o->ltdf = ltdf;
  o->ledf = ledf;
  o->gtdf = gtdf;
  o->gedf = gedf;
  o->fextsfdf = fextsfdf;
  o->ftruncdfsf = ftruncdfsf;
  o->floatsisf = floatsisf;
  o->floatsidf = floatsidf;
  o->ufloatsisf = ufloatsisf;
  o->fixsfsi = fixsfsi;
  o->fixdfsi = fixdfsi;
  o->ufixsfsi = ufixsfsi;
}
