#define IN_MACSCP2_H

#undef WHERE
#define WHERE before macscp2_1
#define BEFORE_MACSCP2_1
#undef UNTIL_MACSCP2_1
void
macscp2_1 ()
{
  puts ("macscp2_1");
}

#include "macscp4.h"

#undef WHERE
#define WHERE before macscp2_2
#define BEFORE_MACSCP2_2
#undef UNTIL_MACSCP2_2
void
macscp2_2 ()
{
  puts ("macscp2_2");
}

#undef IN_MACSCP2_H
