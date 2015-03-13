/* Declarations for res.c.
   Copyright (C) 2001, 2007, 2008, 2009, 2010, 2011 Free Software
   Foundation, Inc.

This file is part of Wget.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
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

#ifndef RES_H
#define RES_H

struct robot_specs;

struct robot_specs *res_parse (const char *, int);
struct robot_specs *res_parse_from_file (const char *);

bool res_match_path (const struct robot_specs *, const char *);

void res_register_specs (const char *, int, struct robot_specs *);
struct robot_specs *res_get_specs (const char *, int);

bool res_retrieve_file (const char *, char **, struct iri *);

bool is_robots_txt_url (const char *);

void res_cleanup (void);

#endif /* RES_H */
