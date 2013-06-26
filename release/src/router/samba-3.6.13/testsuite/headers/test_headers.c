/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell		2011

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

/*
  this tests that all of our public headers will build
 */

#define _GNU_SOURCE 1

#include <Python.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* pre-include some of the public headers to avoid ordering issues */
#include "core/ntstatus.h"

/* include all our public headers */
#include "test_headers.h"

int main(void)
{
	printf("All OK\n");
	return 0;
}
