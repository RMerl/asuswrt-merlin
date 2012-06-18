/* gdbmerrno.h - The enumeration type describing all the dbm errors. */

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


/* gdbm sets the following error codes. */
#define	GDBM_NO_ERROR		0
#define	GDBM_MALLOC_ERROR	1
#define	GDBM_BLOCK_SIZE_ERROR	2
#define	GDBM_FILE_OPEN_ERROR	3
#define	GDBM_FILE_WRITE_ERROR	4
#define	GDBM_FILE_SEEK_ERROR	5
#define	GDBM_FILE_READ_ERROR	6
#define	GDBM_BAD_MAGIC_NUMBER	7
#define	GDBM_EMPTY_DATABASE	8
#define	GDBM_CANT_BE_READER	9
#define	GDBM_CANT_BE_WRITER	10
#define	GDBM_READER_CANT_DELETE	11
#define	GDBM_READER_CANT_STORE	12
#define	GDBM_READER_CANT_REORGANIZE	13
#define	GDBM_UNKNOWN_UPDATE	14
#define	GDBM_ITEM_NOT_FOUND	15
#define	GDBM_REORGANIZE_FAILED	16
#define	GDBM_CANNOT_REPLACE	17
#define	GDBM_ILLEGAL_DATA	18
#define	GDBM_OPT_ALREADY_SET	19
#define	GDBM_OPT_ILLEGAL	20

typedef int gdbm_error;		/* For compatibilities sake. */

extern gdbm_error gdbm_errno;
