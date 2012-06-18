/** \file test-integers.c
 * \brief Check assumptions about integer types (sizes, ranges).
 *
 * Copyright (C) 2007 Hans Ulrich Niedermann <gp@n-dimensional.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */


#include "libexif/_stdint.h"
#include <stdlib.h>
#include <stdio.h>


typedef enum {
   EN_A,
   EN_B,
   EN_C,
   EN_D,
   EN_E,
   EN_F
} enum_t;


#if defined(__GNUC__) && (__GNUC__ >= 4)
# define CHECK(condition)					     \
	if (!(condition)) {					     \
		fprintf(stderr, "%s:%d: check failed: %s\n",	     \
			__FILE__, __LINE__, #condition);	     \
		errors++;					     \
	}
#else
# define CHECK(condition)					     \
	if (!(condition)) {					     \
		abort();					     \
	}
#endif


int main()
{
  unsigned int errors = 0;

  /* libexif assumes unsigned ints are not smaller than 32bit in many places */
  CHECK(sizeof(unsigned int) >= sizeof(uint32_t));

  /* libexif assumes that enums fit into ints */
  CHECK(sizeof(enum_t) <= sizeof(int));
  
  return (errors>0)?1:0;
}
