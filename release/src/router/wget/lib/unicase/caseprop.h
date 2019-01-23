/* Case related properties of Unicode characters.
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

#include <stdbool.h>
#include "unitypes.h"

/* Determine whether a character is "cased" according to the Unicode Standard,
   <http://www.unicode.org/versions/Unicode5.0.0/ch03.pdf>, section 3.13,
   definition D120.  */
extern bool
       uc_is_cased (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;

/* Determine whether a character is "case-ignorable"
   according to the Unicode Standard,
   <http://www.unicode.org/versions/Unicode5.0.0/ch03.pdf>, section 3.13,
   definition D121.  */
extern bool
       uc_is_case_ignorable (ucs4_t uc)
       _UC_ATTRIBUTE_CONST;
