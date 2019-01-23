/* Convert string to wide string.
   Copyright (C) 2008-2017 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2008.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <wchar.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "strnlen1.h"


extern mbstate_t _gl_mbsrtowcs_state;

#include "mbsrtowcs-impl.h"
