/* Declarations for metalink.c.
   Copyright (C) 2015 Free Software Foundation, Inc.

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
#if ! defined METALINK_H && defined HAVE_METALINK
#define	METALINK_H

#include <metalink/metalink_types.h>
#include "wget.h"

#ifdef HAVE_SSL
# define RES_TYPE_SUPPORTED(x)\
    ((!x) || !strcmp (x, "http") || !strcmp (x, "https") || !strcmp (x, "ftp") || !strcmp (x, "ftps"))
#else
# define RES_TYPE_SUPPORTED(x)\
    ((!x) || !strcmp (x, "ftp") || !strcmp (x, "http"))
#endif

#define DEFAULT_PRI 999999
#define VALID_PRI_RANGE(x) ((x) > 0 && (x) < 1000000)

uerr_t retrieve_from_metalink (const metalink_t *metalink);

int metalink_res_cmp (const void *res1, const void *res2);
int metalink_meta_cmp (const void* meta1, const void* meta2);

int metalink_check_safe_path (const char *path);

char *last_component (char const *name);
void replace_metalink_basename (char **name, char *ref);
char *get_metalink_basename (char *name);
void append_suffix_number (char **str, const char *sep, wgint num);
void clean_metalink_string (char **str);
void dequote_metalink_string (char **str);
void badhash_suffix (char *name);
void badhash_or_remove (char *name);
uerr_t fetch_metalink_file (const char *url_str,
                            bool resume, bool metalink_http,
                            const char *filename, char **destname);

bool find_key_value (const char *start,
                     const char *end,
                     const char *key,
                     char **value);
bool has_key (const char *start, const char *end, const char *key);
const char *find_key_values (const char *start,
                             const char *end,
                             char **key,
                             char **value);

#endif	/* METALINK_H */
