/* Internationalization related declarations.
   Copyright (C) 2008, 2009, 2010, 2011 Free Software Foundation, Inc.

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

#ifndef IRI_H
#define IRI_H

struct iri {
  char *uri_encoding;      /* Encoding of the uri to fetch */
  char *content_encoding;  /* Encoding of links inside the fetched file */
  char *orig_url;          /* */
  bool utf8_encode;        /* Will/Is the current url encoded in utf8 */
};

#ifdef ENABLE_IRI

char *parse_charset (char *str);
char *find_locale (void);
bool check_encoding_name (char *encoding);
const char *locale_to_utf8 (const char *str);
char *idn_encode (struct iri *i, char *host);
char *idn_decode (char *host);
bool remote_to_utf8 (struct iri *i, const char *str, const char **new);
struct iri *iri_new (void);
struct iri *iri_dup (const struct iri *);
void iri_free (struct iri *i);
void set_uri_encoding (struct iri *i, char *charset, bool force);
void set_content_encoding (struct iri *i, char *charset);

#else /* ENABLE_IRI */

extern struct iri dummy_iri;

#define parse_charset(str)          (str, NULL)
#define find_locale()               NULL
#define check_encoding_name(str)    false
#define locale_to_utf8(str)         (str)
#define idn_encode(a,b)             NULL
#define idn_decode(str)             NULL
#define remote_to_utf8(a,b,c)       false
#define iri_new()                   (&dummy_iri)
#define iri_dup(a)                  (&dummy_iri)
#define iri_free(a)
#define set_uri_encoding(a,b,c)
#define set_content_encoding(a,b)

#endif /* ENABLE_IRI */
#endif /* IRI_H */
