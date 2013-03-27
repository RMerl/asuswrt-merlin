/* CGEN fpu support
   Copyright (C) 1999 Cygnus Solutions.  */

#include "sim-main.h"
#include "sim-fpu.h"

/* Return boolean indicating if X is an snan.  */

BI
cgen_sf_snan_p (CGEN_FPU* fpu, SF x)
{
  sim_fpu op1;

  sim_fpu_32to (&op1, x);
  return sim_fpu_is_nan (&op1);
}

BI
cgen_df_snan_p (CGEN_FPU* fpu, DF x)
{
  sim_fpu op1;

  sim_fpu_64to (&op1, x);
  return sim_fpu_is_nan (&op1);
}

/* No-op fpu error handler.  */

void
cgen_fpu_ignore_errors (CGEN_FPU* fpu, int status)
{
}
