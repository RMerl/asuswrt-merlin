/* langs1.f -- translated by f2c (version of 5 May 1990  1:12:08).  */

/* f2c output hacked as follows for GDB testsuite:
   1.  Change commented out "#" lines to #line directives.
       I don't know why this behavior isn't the default for f2c -g.
   2.  Remove include of f2c.h and put in just a typedef for "integer".
   Additional notes:
   3.  f2c was called as "f2c -g langs1.f".
   4.  We don't need to use the fortran libraries.  */

typedef int integer;

/* Table of constant values */

static integer c__10000 = 10000;

/* I am not sure whether there is a way to have a fortran program without */
/* a MAIN, but it does not really harm us to have one. */
#ifdef PROTOTYPES
/* Main program */ void MAIN__()
#else
/* Main program */ MAIN__()
#endif
{
} /* MAIN__ */

#line 4 "langs1.f"
/* Subroutine */ int fsub_()
{
#ifdef PROTOTYPES
    extern integer cppsub_(int*);
#else
    extern integer cppsub_();
#endif

#line 5 "langs1.f"
#line 6 "langs1.f"
    return cppsub_(&c__10000);
#line 7 "langs1.f"
} /* fsub_ */

