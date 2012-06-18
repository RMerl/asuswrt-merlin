/* gdbm.h  -  The include file for dbm users.  */

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

/* Protection for multiple includes. */
#ifndef _GDBM_H_
#define _GDBM_H_

/* Parameters to gdbm_open for READERS, WRITERS, and WRITERS who
   can create the database. */
#define  GDBM_READER  0		/* A reader. */
#define  GDBM_WRITER  1		/* A writer. */
#define  GDBM_WRCREAT 2		/* A writer.  Create the db if needed. */
#define  GDBM_NEWDB   3		/* A writer.  Always create a new db. */
#define  GDBM_FAST    0x10	/* Write fast! => No fsyncs.  OBSOLETE. */
#define  GDBM_SYNC    0x20	/* Sync operations to the disk. */
#define  GDBM_NOLOCK  0x40	/* Don't do file locking operations. */

/* Parameters to gdbm_store for simple insertion or replacement in the
   case that the key is already in the database. */
#define  GDBM_INSERT  0		/* Never replace old data with new. */
#define  GDBM_REPLACE 1		/* Always replace old data with new. */

/* Parameters to gdbm_setopt, specifing the type of operation to perform. */
#define  GDBM_CACHESIZE 1       /* Set the cache size. */
#define  GDBM_FASTMODE  2       /* Toggle fast mode.  OBSOLETE. */
#define  GDBM_SYNCMODE	3	/* Turn on or off sync operations. */
#define  GDBM_CENTFREE  4	/* Keep all free blocks in the header. */
#define  GDBM_COALESCEBLKS 5	/* Attempt to coalesce free blocks. */

/* The data and key structure.  This structure is defined for compatibility. */
typedef struct {
	char *dptr;
	int   dsize;
      } datum;


/* The file information header. This is good enough for most applications. */
typedef struct {int dummy[10];} *GDBM_FILE;

/* Determine if the C(++) compiler requires complete function prototype  */
#ifndef __P
#if defined(__STDC__) || defined(__cplusplus) || defined(c_plusplus)
#define __P(x) x
#else
#define __P(x) ()
#endif
#endif

/* External variable, the gdbm build release string. */
extern char *gdbm_version;	


/* GDBM C++ support */
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* These are the routines! */

extern GDBM_FILE gdbm_open __P((char *, int, int, int, void (*)()));
extern void gdbm_close __P((GDBM_FILE));
extern int gdbm_store __P((GDBM_FILE, datum, datum, int));
extern datum gdbm_fetch __P((GDBM_FILE, datum));
extern int gdbm_delete __P((GDBM_FILE, datum));
extern datum gdbm_firstkey __P((GDBM_FILE));
extern datum gdbm_nextkey __P((GDBM_FILE, datum));
extern int gdbm_reorganize __P((GDBM_FILE));
extern void gdbm_sync __P((GDBM_FILE));
extern int gdbm_exists __P((GDBM_FILE, datum));
extern int gdbm_setopt __P((GDBM_FILE, int, int *, int));
extern int gdbm_fdesc __P((GDBM_FILE));

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

