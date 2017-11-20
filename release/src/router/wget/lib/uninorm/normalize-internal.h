/* Normalization of Unicode strings.
   Copyright (C) 2009-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2009.

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

#include "unitypes.h"

/* Complete definition of normalization form descriptor.  */
struct unicode_normalization_form
{
  /* Bit mask containing meta-information.
     This must be the first field.  */
  unsigned int description;
  #define NF_IS_COMPAT_DECOMPOSING  (1 << 0)
  #define NF_IS_COMPOSING           (1 << 1)
  /* Function that decomposes a Unicode character.  */
  int (*decomposer) (ucs4_t uc, ucs4_t *decomposition);
  /* Function that combines two Unicode characters, a starter and another
     character.  */
  ucs4_t (*composer) (ucs4_t uc1, ucs4_t uc2);
  /* Decomposing variant.  */
  const struct unicode_normalization_form *decomposing_variant;
};
