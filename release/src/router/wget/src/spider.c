/* Keep track of visited URLs in spider mode.
   Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Free Software
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

#include "wget.h"

#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "spider.h"
#include "url.h"
#include "utils.h"
#include "hash.h"
#include "res.h"


static struct hash_table *nonexisting_urls_set;

/* Cleanup the data structures associated with this file.  */

void
spider_cleanup (void)
{
  if (nonexisting_urls_set)
    string_set_free (nonexisting_urls_set);
}

/* Remembers broken links.  */
void
nonexisting_url (const char *url)
{
  /* Ignore robots.txt URLs */
  if (is_robots_txt_url (url))
    return;
  if (!nonexisting_urls_set)
    nonexisting_urls_set = make_string_hash_table (0);
  string_set_add (nonexisting_urls_set, url);
}

void
print_broken_links (void)
{
  hash_table_iterator iter;
  int num_elems;

  if (!nonexisting_urls_set)
    {
      logprintf (LOG_NOTQUIET, _("Found no broken links.\n\n"));
      return;
    }

  num_elems = hash_table_count (nonexisting_urls_set);
  assert (num_elems > 0);

  logprintf (LOG_NOTQUIET, ngettext("Found %d broken link.\n\n",
                                    "Found %d broken links.\n\n", num_elems),
             num_elems);

  for (hash_table_iterate (nonexisting_urls_set, &iter);
       hash_table_iter_next (&iter); )
    {
      /* Struct url_list *list; */
      const char *url = (const char *) iter.key;

      logprintf (LOG_NOTQUIET, _("%s\n"), url);
    }
  logputs (LOG_NOTQUIET, "\n");
}

/*
 * vim: et ts=2 sw=2
 */
