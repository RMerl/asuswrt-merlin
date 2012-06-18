/* proto.h - The prototypes for the dbm routines. */

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


#ifndef __P
#ifdef __STDC__
#define __P(args) args
#else
#define __P(args) ()
#endif
#endif

/* From bucket.c */
void _gdbm_new_bucket    __P((gdbm_file_info *, hash_bucket *, int));
void _gdbm_get_bucket    __P((gdbm_file_info *, int));
void _gdbm_split_bucket  __P((gdbm_file_info *, int));
void _gdbm_write_bucket  __P((gdbm_file_info *, cache_elem *));

/* From falloc.c */
off_t _gdbm_alloc       __P((gdbm_file_info *, int));
void _gdbm_free         __P((gdbm_file_info *, off_t, int));
int  _gdbm_put_av_elem  __P((avail_elem, avail_elem [], int *, int));

/* From findkey.c */
char *_gdbm_read_entry  __P((gdbm_file_info *, int));
int _gdbm_findkey       __P((gdbm_file_info *, datum, char **, int *));

/* From hash.c */
int _gdbm_hash __P((datum));

/* From update.c */
void _gdbm_end_update   __P((gdbm_file_info *));
void _gdbm_fatal         __P((gdbm_file_info *, char *));

/* From gdbmopen.c */
int _gdbm_init_cache	__P((gdbm_file_info *, int));

/* The user callable routines.... */
void  gdbm_close	  __P((gdbm_file_info *));
int   gdbm_delete	  __P((gdbm_file_info *, datum));
datum gdbm_fetch	  __P((gdbm_file_info *, datum));
gdbm_file_info *gdbm_open __P((char *, int, int, int, void (*) (void)));
int   gdbm_reorganize	  __P((gdbm_file_info *));
datum gdbm_firstkey       __P((gdbm_file_info *));
datum gdbm_nextkey        __P((gdbm_file_info *, datum));
int   gdbm_store          __P((gdbm_file_info *, datum, datum, int));
int   gdbm_exists	  __P((gdbm_file_info *, datum));
void  gdbm_sync		  __P((gdbm_file_info *));
int   gdbm_setopt	  __P((gdbm_file_info *, int, int *, int));
int   gdbm_fdesc	  __P((gdbm_file_info *));
