/* Declarations for HTTP.
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free
   Software Foundation, Inc.

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

#ifndef HTTP_H
#define HTTP_H

#include "hsts.h"

struct url;

uerr_t http_loop (const struct url *, struct url *, char **, char **, const char *,
                  int *, struct url *, struct iri *);
void save_cookies (void);
void http_cleanup (void);
time_t http_atotm (const char *);

typedef struct {
  /* A token consists of characters in the [b, e) range. */
  const char *b, *e;
} param_token;
bool extract_param (const char **, param_token *, param_token *, char, bool *);


#endif /* HTTP_H */
