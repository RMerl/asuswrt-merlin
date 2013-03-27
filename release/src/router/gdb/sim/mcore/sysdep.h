/* System includes and definitions used by the Motorola MCore simulator.
   Copyright (C) 1999, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

This file is part of GDB, the GNU debugger.

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

#ifndef __SYSDEP_H
#define __SYSDEP_H

#ifndef	hosts_std_host_H
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <sys/file.h>
#include "ansidecl.h"

#ifndef	O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif
#ifndef	SEEK_SET
#define SEEK_SET 0
#endif
#ifndef	SEEK_CUR
#define SEEK_CUR 1
#endif
#ifdef STDC_HEADERS
#include <stdlib.h>
/*#include <string.h>*/
#else
extern char * mktemp ();
#ifndef memset
extern PTR    memset ();
#endif

#ifndef	DONTDECLARE_MALLOC
extern PTR   malloc ();
extern PTR   realloc ();
#endif

#ifndef	__GNUC__
extern PTR   memcpy ();
#else
/* char * memcpy (); */
#endif

#ifdef __STDC__
extern void free ();
#else
extern int free();
#endif

#ifndef strchr
extern char * strchr();
#endif
extern char * getenv();
extern PTR    memchr();
extern char * strrchr();

extern char * strrchr();
extern char * ctime();
extern long   atol();
extern char * getenv();
#endif /* STDC_HEADERS */

#ifndef	BYTES_IN_PRINTF_INT
#define BYTES_IN_PRINTF_INT 4
#endif

#include "fopen-same.h"
#define hosts_std_host_H
#endif

#ifdef	STDC_HEADERS
#include <stddef.h>
#endif /* STDC_HEADERS */

#endif /* __SYSDEP_H */
