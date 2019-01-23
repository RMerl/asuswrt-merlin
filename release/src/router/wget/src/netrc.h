/* Declarations for netrc.c
   Copyright (C) 1996, 1996, 1997, 2007, 2008, 2009, 2010, 2011, 2015
   Free Software Foundation, Inc.

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

#ifndef NETRC_H
#define NETRC_H

typedef struct _acc_t
{
  char *host;           /* NULL if this is the default machine
                           entry.  */
  char *acc;
  char *passwd;         /* NULL if there is no password.  */
  struct _acc_t *next;
} acc_t;

void search_netrc (const char *, const char **, const char **, int);
void free_netrc (acc_t *l);
void netrc_cleanup(void);

#endif /* NETRC_H */
