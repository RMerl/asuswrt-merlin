/* global.c - The external variables needed for "original" interface and
   error messages. */

/*  This file is part of GDBM, the GNU data base manager, by Philip A. Nelson.
    Copyright (C) 1990, 1991, 1993  Free Software Foundation, Inc.

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

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department
                Western Washington University
                Bellingham, WA 98226
       
*************************************************************************/


/* include system configuration before all else. */
#include "autoconf.h"

#include "gdbmdefs.h"
#include "gdbmerrno.h"


/* The global variables used for the "original" interface. */
gdbm_file_info  *_gdbm_file = NULL;

/* Memory for return data for the "original" interface. */
datum _gdbm_memory = {NULL, 0};	/* Used by firstkey and nextkey. */
char *_gdbm_fetch_val = NULL;	/* Used by fetch. */

/* The dbm error number is placed in the variable GDBM_ERRNO. */
gdbm_error gdbm_errno = GDBM_NO_ERROR;
