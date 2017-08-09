/*
   Unix SMB/CIFS implementation.
   test suite for spoolss rpc operations

   Copyright (C) Guenther Deschner 2009

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

#if 0
#include "lib/replace/replace.h"
#endif

#include <windows.h>
#include <stdio.h>

#include "error.h"
#include "printlib_proto.h"

#if 0
#include "lib/talloc/talloc.h"
#include "libcli/util/ntstatus.h"
#include "lib/torture/torture.h"
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

#ifndef true
#define true TRUE
#endif

#ifndef false
#define false FALSE
#endif

#ifndef PRINTER_ENUM_NAME
#define PRINTER_ENUM_NAME 8
#endif
