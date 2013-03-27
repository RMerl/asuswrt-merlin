#define IN_MACSCP3_H

#undef WHERE
#define WHERE before macscp3_1
#define BEFORE_MACSCP3_1
#undef UNTIL_MACSCP3_1
void
macscp3_1 ()
{
  puts ("macscp3_1");
}

#include "macscp4.h"

#undef WHERE
#define WHERE before macscp3_2
#define BEFORE_MACSCP3_2
#undef UNTIL_MACSCP3_2
void
macscp3_2 ()
{
  puts ("macscp3_2");
}

#undef IN_MACSCP3_H
