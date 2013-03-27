#include <stdio.h>

#define SPLICE(a, b) INNER_SPLICE(a, b)
#define INNER_SPLICE(a, b) a ## b
#define STRINGIFY(a) INNER_STRINGIFY(a)
#define INNER_STRINGIFY(a) #a

/* A macro named UNTIL_<func> is #defined until just before the
   definition of the function <func>.

   A macro named BEFORE_<func> is not #defined until just before the
   definition of <func>.

   The macro WHERE is redefined before each function <func> to the
   token list ``before <func>''.

   The macscp IN_MACSCP2_H and IN_MACSCP3_H are defined while
   processing those header files; macscp4.h uses them to choose
   appropriate function names, output strings, and macro definitions.  */

#define UNTIL_MACSCP1_1
#define UNTIL_MACSCP2_1
#define UNTIL_MACSCP4_1_FROM_MACSCP2
#define UNTIL_MACSCP4_2_FROM_MACSCP2
#define UNTIL_MACSCP2_2
#define UNTIL_MACSCP1_2
#define UNTIL_MACSCP3_1
#define UNTIL_MACSCP4_1_FROM_MACSCP3
#define UNTIL_MACSCP4_2_FROM_MACSCP3
#define UNTIL_MACSCP3_2
#define UNTIL_MACSCP1_3

#define WHERE before macscp1_1
#define BEFORE_MACSCP1_1
#undef UNTIL_MACSCP1_1
void
macscp1_1 ()
{
  puts ("macscp1_1");
}

#include "macscp2.h"

#undef WHERE
#define WHERE before macscp1_2
#define BEFORE_MACSCP1_2
#undef UNTIL_MACSCP1_2
void
macscp1_2 ()
{
  puts ("macscp1_2");
}

#include "macscp3.h"

#undef WHERE
#define WHERE before macscp1_3
#define BEFORE_MACSCP1_3
#undef UNTIL_MACSCP1_3
void
macscp1_3 ()
{
  puts ("macscp1_3");
}

int
main (int argc, char **argv)
{
  macscp1_1 ();
  macscp2_1 ();
  macscp4_1_from_macscp2 ();
  macscp4_2_from_macscp2 ();
  macscp2_2 ();
  macscp1_2 ();
  macscp3_1 ();
  macscp4_1_from_macscp3 ();
  macscp4_2_from_macscp3 ();
  macscp3_2 ();
  macscp1_3 ();
}
