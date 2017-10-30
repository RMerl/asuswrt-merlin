/* Declarations for html-parse.c.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2008, 2009, 2010, 2011, 2015 Free Software Foundation, Inc.

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

#ifndef HTML_PARSE_H
#define HTML_PARSE_H

struct attr_pair {
  char *name;           /* attribute name */
  char *value;          /* attribute value */

  /* Needed for URL conversion; the places where the value begins and
     ends, including the quotes and everything. */
  const char *value_raw_beginning;
  int value_raw_size;

  /* Used internally by map_html_tags. */
  int name_pool_index, value_pool_index;
};

struct taginfo {
  char *name;                   /* tag name */
  int end_tag_p;                /* whether this is an end-tag */
  int nattrs;                   /* number of attributes */
  struct attr_pair *attrs;      /* attributes */

  const char *start_position;   /* start position of tag */
  const char *end_position;     /* end position of tag */

  const char *contents_begin;   /* delimiters of tag contents */
  const char *contents_end;     /* only valid if end_tag_p */
};

struct hash_table;              /* forward declaration */

/* Flags for map_html_tags: */
#define MHT_STRICT_COMMENTS  1  /* use strict comment interpretation */
#define MHT_TRIM_VALUES      2  /* trim attribute values, e.g. interpret
                                   <a href=" foo "> as "foo" */

void map_html_tags (const char *, int,
                    void (*) (struct taginfo *, void *), void *, int,
                    const struct hash_table *, const struct hash_table *);

#endif /* HTML_PARSE_H */
