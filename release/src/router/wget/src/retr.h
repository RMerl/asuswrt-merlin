/* Declarations for retr.c.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef RETR_H
#define RETR_H

#include "url.h"

extern int numurls;

/* These global vars should be made static to retr.c and exported via
   functions! */
extern SUM_SIZE_INT total_downloaded_bytes;
extern double total_download_time;
extern FILE *output_stream;
extern bool output_stream_regular;

/* Flags for fd_read_body. */
enum {
  rb_read_exactly  = 1,
  rb_skip_startpos = 2,

  /* Used by HTTP/HTTPS*/
  rb_chunked_transfer_encoding = 4,

  rb_compressed_gzip = 8
};

int fd_read_body (const char *, int, FILE *, wgint, wgint, wgint *, wgint *, double *, int, FILE *);

typedef const char *(*hunk_terminator_t) (const char *, const char *, int);

char *fd_read_hunk (int, hunk_terminator_t, long, long);
char *fd_read_line (int);

uerr_t retrieve_url (struct url *, const char *, char **, char **,
                     const char *, int *, bool, struct iri *, bool);
uerr_t retrieve_from_file (const char *, bool, int *);

const char *retr_rate (wgint, double);
double calc_rate (wgint, double, int *);
void printwhat (int, int);

void sleep_between_retrievals (int);

void rotate_backups (const char *);

bool url_uses_proxy (struct url *);

void set_local_file (const char **, const char *);

bool input_file_url (const char *);

#endif /* RETR_H */
