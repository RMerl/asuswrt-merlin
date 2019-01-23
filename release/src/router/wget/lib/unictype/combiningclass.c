/* Combining classes of Unicode characters.
   Copyright (C) 2002, 2006, 2009-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2002.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "unictype.h"

/* Define u_combclass table.  */
#include "combiningclass.h"

int
uc_combining_class (ucs4_t uc)
{
  unsigned int index1 = uc >> combclass_header_0;
  if (index1 < combclass_header_1)
    {
      int lookup1 = u_combclass.level1[index1];
      if (lookup1 >= 0)
        {
          unsigned int index2 = (uc >> combclass_header_2) & combclass_header_3;
          int lookup2 = u_combclass.level2[lookup1 + index2];
          if (lookup2 >= 0)
            {
              unsigned int index3 = (uc & combclass_header_4);
              unsigned int lookup3 = u_combclass.level3[lookup2 + index3];

              return lookup3;
            }
        }
    }
  return 0;
}
