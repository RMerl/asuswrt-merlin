/* CGEN fpu support
   Copyright (C) 1999 Cygnus Solutions.  */

#ifndef CGEN_FPU_H
#define CGEN_FPU_H

/* Floating point support is a little more complicated.
   We want to support using either host fp insns or an accurate fp library.
   We also want to support easily added variants (e.g. modified ieee).
   This is done by vectoring all calls through a table.  */

typedef USI SF;
typedef UDI DF;
typedef struct { SI parts[3]; } XF;
typedef struct { SI parts[4]; } TF;

#ifndef TARGET_EXT_FP_WORDS
#define TARGET_EXT_FP_WORDS 4
#endif

/* forward decl */
typedef struct cgen_fp_ops CGEN_FP_OPS;

/* Instance of an fpu.  */

typedef struct {
  /* Usually a pointer back to the SIM_CPU struct.  */
  void* owner;
  /* Pointer to ops struct, rather than copy of it, to avoid bloating
     SIM_CPU struct.  */
  CGEN_FP_OPS* ops;
} CGEN_FPU;

/* result of cmp */

typedef enum {
  /* ??? May wish to distinguish qnan/snan here.  */
  FP_CMP_EQ, FP_CMP_LT, FP_CMP_GT, FP_CMP_NAN
} CGEN_FP_CMP;

/* error handler */

typedef void (CGEN_FPU_ERROR_FN) (CGEN_FPU*, int);

/* fpu operation table */

struct cgen_fp_ops {

  /* error (e.g. signalling nan) handler, supplied by owner */

  CGEN_FPU_ERROR_FN *error;

  /* basic SF ops */

  SF (*addsf) (CGEN_FPU*, SF, SF);
  SF (*subsf) (CGEN_FPU*, SF, SF);
  SF (*mulsf) (CGEN_FPU*, SF, SF);
  SF (*divsf) (CGEN_FPU*, SF, SF);
  SF (*negsf) (CGEN_FPU*, SF);
  SF (*abssf) (CGEN_FPU*, SF);
  SF (*sqrtsf) (CGEN_FPU*, SF);
  SF (*invsf) (CGEN_FPU*, SF);
  SF (*cossf) (CGEN_FPU*, SF);
  SF (*sinsf) (CGEN_FPU*, SF);
  SF (*minsf) (CGEN_FPU*, SF, SF);
  SF (*maxsf) (CGEN_FPU*, SF, SF);

  /* ??? to be revisited */
  CGEN_FP_CMP (*cmpsf) (CGEN_FPU*, SF, SF);
  int (*eqsf) (CGEN_FPU*, SF, SF);
  int (*nesf) (CGEN_FPU*, SF, SF);
  int (*ltsf) (CGEN_FPU*, SF, SF);
  int (*lesf) (CGEN_FPU*, SF, SF);
  int (*gtsf) (CGEN_FPU*, SF, SF);
  int (*gesf) (CGEN_FPU*, SF, SF);

  /* basic DF ops */

  DF (*adddf) (CGEN_FPU*, DF, DF);
  DF (*subdf) (CGEN_FPU*, DF, DF);
  DF (*muldf) (CGEN_FPU*, DF, DF);
  DF (*divdf) (CGEN_FPU*, DF, DF);
  DF (*negdf) (CGEN_FPU*, DF);
  DF (*absdf) (CGEN_FPU*, DF);
  DF (*sqrtdf) (CGEN_FPU*, DF);
  DF (*invdf) (CGEN_FPU*, DF);
  DF (*cosdf) (CGEN_FPU*, DF);
  DF (*sindf) (CGEN_FPU*, DF);
  DF (*mindf) (CGEN_FPU*, DF, DF);
  DF (*maxdf) (CGEN_FPU*, DF, DF);

  /* ??? to be revisited */
  CGEN_FP_CMP (*cmpdf) (CGEN_FPU*, DF, DF);
  int (*eqdf) (CGEN_FPU*, DF, DF);
  int (*nedf) (CGEN_FPU*, DF, DF);
  int (*ltdf) (CGEN_FPU*, DF, DF);
  int (*ledf) (CGEN_FPU*, DF, DF);
  int (*gtdf) (CGEN_FPU*, DF, DF);
  int (*gedf) (CGEN_FPU*, DF, DF);

  /* SF/DF conversion ops */

  DF (*fextsfdf) (CGEN_FPU*, SF);
  SF (*ftruncdfsf) (CGEN_FPU*, DF);

  SF (*floatsisf) (CGEN_FPU*, SI);
  SF (*floatdisf) (CGEN_FPU*, DI);
  SF (*ufloatsisf) (CGEN_FPU*, USI);
  SF (*ufloatdisf) (CGEN_FPU*, UDI);

  SI (*fixsfsi) (CGEN_FPU*, SF);
  DI (*fixsfdi) (CGEN_FPU*, SF);
  USI (*ufixsfsi) (CGEN_FPU*, SF);
  UDI (*ufixsfdi) (CGEN_FPU*, SF);

  DF (*floatsidf) (CGEN_FPU*, SI);
  DF (*floatdidf) (CGEN_FPU*, DI);
  DF (*ufloatsidf) (CGEN_FPU*, USI);
  DF (*ufloatdidf) (CGEN_FPU*, UDI);

  SI (*fixdfsi) (CGEN_FPU*, DF);
  DI (*fixdfdi) (CGEN_FPU*, DF);
  USI (*ufixdfsi) (CGEN_FPU*, DF);
  UDI (*ufixdfdi) (CGEN_FPU*, DF);

  /* XF mode support (kept separate 'cus not always present) */

  XF (*addxf) (CGEN_FPU*, XF, XF);
  XF (*subxf) (CGEN_FPU*, XF, XF);
  XF (*mulxf) (CGEN_FPU*, XF, XF);
  XF (*divxf) (CGEN_FPU*, XF, XF);
  XF (*negxf) (CGEN_FPU*, XF);
  XF (*absxf) (CGEN_FPU*, XF);
  XF (*sqrtxf) (CGEN_FPU*, XF);
  XF (*invxf) (CGEN_FPU*, XF);
  XF (*cosxf) (CGEN_FPU*, XF);
  XF (*sinxf) (CGEN_FPU*, XF);
  XF (*minxf) (CGEN_FPU*, XF, XF);
  XF (*maxxf) (CGEN_FPU*, XF, XF);

  CGEN_FP_CMP (*cmpxf) (CGEN_FPU*, XF, XF);
  int (*eqxf) (CGEN_FPU*, XF, XF);
  int (*nexf) (CGEN_FPU*, XF, XF);
  int (*ltxf) (CGEN_FPU*, XF, XF);
  int (*lexf) (CGEN_FPU*, XF, XF);
  int (*gtxf) (CGEN_FPU*, XF, XF);
  int (*gexf) (CGEN_FPU*, XF, XF);

  XF (*extsfxf) (CGEN_FPU*, SF);
  XF (*extdfxf) (CGEN_FPU*, DF);
  SF (*truncxfsf) (CGEN_FPU*, XF);
  DF (*truncxfdf) (CGEN_FPU*, XF);

  XF (*floatsixf) (CGEN_FPU*, SI);
  XF (*floatdixf) (CGEN_FPU*, DI);
  XF (*ufloatsixf) (CGEN_FPU*, USI);
  XF (*ufloatdixf) (CGEN_FPU*, UDI);

  SI (*fixxfsi) (CGEN_FPU*, XF);
  DI (*fixxfdi) (CGEN_FPU*, XF);
  USI (*ufixxfsi) (CGEN_FPU*, XF);
  UDI (*ufixxfdi) (CGEN_FPU*, XF);

  /* TF mode support (kept separate 'cus not always present) */

  TF (*addtf) (CGEN_FPU*, TF, TF);
  TF (*subtf) (CGEN_FPU*, TF, TF);
  TF (*multf) (CGEN_FPU*, TF, TF);
  TF (*divtf) (CGEN_FPU*, TF, TF);
  TF (*negtf) (CGEN_FPU*, TF);
  TF (*abstf) (CGEN_FPU*, TF);
  TF (*sqrttf) (CGEN_FPU*, TF);
  TF (*invtf) (CGEN_FPU*, TF);
  TF (*costf) (CGEN_FPU*, TF);
  TF (*sintf) (CGEN_FPU*, TF);
  TF (*mintf) (CGEN_FPU*, TF, TF);
  TF (*maxtf) (CGEN_FPU*, TF, TF);

  CGEN_FP_CMP (*cmptf) (CGEN_FPU*, TF, TF);
  int (*eqtf) (CGEN_FPU*, TF, TF);
  int (*netf) (CGEN_FPU*, TF, TF);
  int (*lttf) (CGEN_FPU*, TF, TF);
  int (*letf) (CGEN_FPU*, TF, TF);
  int (*gttf) (CGEN_FPU*, TF, TF);
  int (*getf) (CGEN_FPU*, TF, TF);

  TF (*extsftf) (CGEN_FPU*, SF);
  TF (*extdftf) (CGEN_FPU*, DF);
  SF (*trunctfsf) (CGEN_FPU*, TF);
  DF (*trunctfdf) (CGEN_FPU*, TF);

  TF (*floatsitf) (CGEN_FPU*, SI);
  TF (*floatditf) (CGEN_FPU*, DI);
  TF (*ufloatsitf) (CGEN_FPU*, USI);
  TF (*ufloatditf) (CGEN_FPU*, UDI);

  SI (*fixtfsi) (CGEN_FPU*, TF);
  DI (*fixtfdi) (CGEN_FPU*, TF);
  USI (*ufixtfsi) (CGEN_FPU*, TF);
  UDI (*ufixtfdi) (CGEN_FPU*, TF);

};

extern void cgen_init_accurate_fpu (SIM_CPU*, CGEN_FPU*, CGEN_FPU_ERROR_FN*);

BI cgen_sf_snan_p (CGEN_FPU*, SF);
BI cgen_df_snan_p (CGEN_FPU*, DF);

/* no-op fp error handler */
extern CGEN_FPU_ERROR_FN cgen_fpu_ignore_errors;

#endif /* CGEN_FPU_H */
