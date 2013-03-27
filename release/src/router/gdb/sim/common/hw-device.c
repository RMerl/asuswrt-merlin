/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include "hw-main.h"
#include "hw-base.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* Address methods */

const hw_unit *
hw_unit_address (struct hw *me)
{
  return &me->unit_address_of_hw;
}


/* IOCTL: */

int
hw_ioctl (struct hw *me,
	  hw_ioctl_request request,
	  ...)
{
  int status;
  va_list ap;
  va_start(ap, request);
  status = me->to_ioctl (me, request, ap);
  va_end(ap);
  return status;
}
      
char *
hw_strdup (struct hw *me, const char *str)
{
  if (str != NULL)
    {
      char *dup = hw_zalloc (me, strlen (str) + 1);
      strcpy (dup, str);
      return dup;
    }
  else
    {
      return NULL;
    }
}
