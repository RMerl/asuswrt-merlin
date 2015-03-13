/* Declarations for convert.c
   Copyright (C) 2003, 2004, 2005, 2006, 2009, 2010, 2011 Free Software
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

#ifndef CONVERT_H
#define CONVERT_H

struct hash_table;              /* forward decl */
extern struct hash_table *dl_url_file_map;
extern struct hash_table *downloaded_html_set;
extern struct hash_table *downloaded_css_set;

enum convert_options {
  CO_NOCONVERT = 0,             /* don't convert this URL */
  CO_CONVERT_TO_RELATIVE,       /* convert to relative, e.g. to
                                   "../../otherdir/foo.gif" */
  CO_CONVERT_TO_COMPLETE,       /* convert to absolute, e.g. to
                                   "http://orighost/somedir/bar.jpg". */
  CO_NULLIFY_BASE               /* change to empty string. */
};

struct url;

/* A structure that defines the whereabouts of a URL, i.e. its
   position in an HTML document, etc.  */

struct urlpos {
  struct url *url;              /* the URL of the link, after it has
                                   been merged with the base */
  char *local_name;             /* local file to which it was saved
                                   (used by convert_links) */

  /* reserved for special links such as <base href="..."> which are
     used when converting links, but ignored when downloading.  */
  unsigned int ignore_when_downloading  :1;

  /* Information about the original link: */

  unsigned int link_relative_p  :1; /* the link was relative */
  unsigned int link_complete_p  :1; /* the link was complete (had host name) */
  unsigned int link_base_p  :1;     /* the url came from <base href=...> */
  unsigned int link_inline_p    :1; /* needed to render the page */
  unsigned int link_css_p   :1;     /* the url came from CSS */
  unsigned int link_expect_html :1; /* expected to contain HTML */
  unsigned int link_expect_css  :1; /* expected to contain CSS */

  unsigned int link_refresh_p   :1; /* link was received from
                                       <meta http-equiv=refresh content=...> */
  int refresh_timeout;              /* for reconstructing the refresh. */

  /* Conversion requirements: */
  enum convert_options convert;     /* is conversion required? */

  /* URL's position in the buffer. */
  int pos, size;

  struct urlpos *next;              /* next list element */
};

/* downloaded_file() takes a parameter of this type and returns this type. */
typedef enum
{
  /* Return enumerators: */
  FILE_NOT_ALREADY_DOWNLOADED = 0,

  /* Return / parameter enumerators: */
  FILE_DOWNLOADED_NORMALLY,
  FILE_DOWNLOADED_AND_HTML_EXTENSION_ADDED,

  /* Parameter enumerators: */
  CHECK_FOR_FILE
} downloaded_file_t;

downloaded_file_t downloaded_file (downloaded_file_t, const char *);

void register_download (const char *, const char *);
void register_redirection (const char *, const char *);
void register_html (const char *);
void register_css (const char *);
void register_delete_file (const char *);
void convert_all_links (void);
void convert_cleanup (void);

char *html_quote_string (const char *);

#endif /* CONVERT_H */
