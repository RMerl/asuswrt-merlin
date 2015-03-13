/* Declarations for html-url.c.
   Copyright (C) 1995, 1996, 1997, 2009, 2010, 2011 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

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

#ifndef HTML_URL_H
#define HTML_URL_H

struct map_context {
  char *text;                   /* HTML text. */
  char *base;                   /* Base URI of the document, possibly
                                   changed through <base href=...>. */
  const char *parent_base;      /* Base of the current document. */
  const char *document_file;    /* File name of this document. */
  bool nofollow;                /* whether NOFOLLOW was specified in a
                                   <meta name=robots> tag. */

  struct urlpos *head;          /* List of URLs that is being built. */
};

struct urlpos *get_urls_file (const char *);
struct urlpos *get_urls_html (const char *, const char *, bool *, struct iri *);
struct urlpos *append_url (const char *, int, int, struct map_context *);
void free_urlpos (struct urlpos *);
void cleanup_html_url (void);

#endif /* HTML_URL_H */
