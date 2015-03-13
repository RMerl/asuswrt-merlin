/* Support for cookies.
   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
   2010, 2011 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

GNU Wget is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

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

#ifndef COOKIES_H
#define COOKIES_H

struct cookie_jar;

struct cookie_jar *cookie_jar_new (void);
void cookie_jar_delete (struct cookie_jar *);

void cookie_handle_set_cookie (struct cookie_jar *, const char *, int,
                               const char *, const char *);
char *cookie_header (struct cookie_jar *, const char *, int,
                     const char *, bool);

void cookie_jar_load (struct cookie_jar *, const char *);
void cookie_jar_save (struct cookie_jar *, const char *);

#endif /* COOKIES_H */
