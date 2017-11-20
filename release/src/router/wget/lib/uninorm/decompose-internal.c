/* Decomposition of Unicode strings.
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
#include "decompose-internal.h"

#define ELEMENT struct ucs4_with_ccc
#define COMPARE(a,b) ((a)->ccc - (b)->ccc)
#define STATIC
#define merge_sort_fromto gl_uninorm_decompose_merge_sort_fromto
#define merge_sort_inplace gl_uninorm_decompose_merge_sort_inplace
#include "array-mergesort.h"
