/* Normalization of UTF-8 strings.
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

#include <config.h>

/* Specification.  */
#include "uninorm.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "unistr.h"
#include "unictype.h"
#include "normalize-internal.h"
#include "decompose-internal.h"

#define FUNC u8_normalize
#define UNIT uint8_t
#define U_MBTOUC_UNSAFE u8_mbtouc_unsafe
#define U_UCTOMB u8_uctomb
#define U_CPY u8_cpy
#include "u-normalize-internal.h"
