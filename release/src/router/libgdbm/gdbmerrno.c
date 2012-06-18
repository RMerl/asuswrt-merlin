/* gdbmerrno.c - convert gdbm errors into english. */

/*  This file is part of GDBM, the GNU data base manager, by Philip A. Nelson.
    Copyright (C) 1993  Free Software Foundation, Inc.

    GDBM is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    GDBM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with GDBM; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the original author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department
                Western Washington University
                Bellingham, WA 98226
       
    The author of this file is:
       e-mail:  downsj@downsj.com

*************************************************************************/


/* include system configuration before all else. */
#include "autoconf.h"

#include "gdbmerrno.h"

/* this is not static so that applications may access the array if they
   like. it must be in the same order as the error codes! */

const char * const gdbm_errlist[] = {
  "No error", "Malloc error", "Block size error", "File open error",
  "File write error", "File seek error", "File read error",
  "Bad magic number", "Empty database", "Can't be reader", "Can't be writer",
  "Reader can't delete", "Reader can't store", "Reader can't reorganize",
  "Unknown update", "Item not found", "Reorganize failed", "Cannot replace",
  "Illegal data", "Option already set", "Illegal option"
};

#define gdbm_errorcount	((sizeof(gdbm_errlist) / sizeof(gdbm_errlist[0])) - 1)

const char *
gdbm_strerror(error)
    gdbm_error error;
{
  if(((int)error < 0) || ((int)error > gdbm_errorcount))
    {
      return("Unknown error");
    }
  else
    {
      return(gdbm_errlist[(int)error]);
    }
}
