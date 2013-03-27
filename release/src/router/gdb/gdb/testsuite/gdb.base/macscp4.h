/* Put together a macro we can use as part of function names.  */
#undef MACSCP4_INCLUSION
#ifdef IN_MACSCP2_H
#define MACSCP4_INCLUSION from_macscp2
#endif
#ifdef IN_MACSCP3_H
#define MACSCP4_INCLUSION from_macscp3
#endif

#undef WHERE
#ifdef IN_MACSCP2_H
#define WHERE before macscp4_1_..., from macscp2.h
#define BEFORE_MACSCP4_1_FROM_MACSCP2
#undef UNTIL_MACSCP4_1_FROM_MACSCP2
#endif
#ifdef IN_MACSCP3_H
#define WHERE before macscp4_1_..., from macscp3.h
#define BEFORE_MACSCP4_1_FROM_MACSCP3
#undef UNTIL_MACSCP4_1_FROM_MACSCP3
#endif
void
SPLICE (macscp4_1_, MACSCP4_INCLUSION) ()
{
  puts ("macscp4_1_" STRINGIFY(MACSCP4_INCLUSION));
}

#undef WHERE
#ifdef IN_MACSCP2_H
#define WHERE before macscp4_2_..., from macscp2.h
#define BEFORE_MACSCP4_2_FROM_MACSCP2
#undef UNTIL_MACSCP4_2_FROM_MACSCP2
#endif
#ifdef IN_MACSCP3_H
#define WHERE before macscp4_2_..., from macscp3.h
#define BEFORE_MACSCP4_2_FROM_MACSCP3
#undef UNTIL_MACSCP4_2_FROM_MACSCP3
#endif
void
SPLICE (macscp4_2_, MACSCP4_INCLUSION) ()
{
  puts ("macscp4_2_" STRINGIFY(MACSCP4_INCLUSION));
}

#define DEFINED_IN_MACSCP4 this was defined in macscp4.h.
