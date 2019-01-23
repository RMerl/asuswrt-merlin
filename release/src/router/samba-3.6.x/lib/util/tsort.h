/*
   Unix SMB/CIFS implementation.

   typesafe qsort

   Copyright (C) Andrew Tridgell 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TSORT_H
#define _TSORT_H
#include <assert.h>

/*
  a wrapper around qsort() that ensures the comparison function is
  type safe.
 */
#ifndef TYPESAFE_QSORT
#define TYPESAFE_QSORT(base, numel, comparison) \
do { \
	if (numel > 1) { \
		qsort(base, numel, sizeof((base)[0]), (int (*)(const void *, const void *))comparison); \
		assert(comparison(&((base)[0]), &((base)[1])) <= 0); \
	} \
} while (0)
#endif

#endif
