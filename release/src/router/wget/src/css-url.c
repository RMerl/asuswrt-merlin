/* Collect URLs from CSS source.
   Copyright (C) 1998, 2000, 2001, 2002, 2003, 2009, 2010, 2011, 2014 Free
   Software Foundation, Inc.

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

/*
  Note that this is not an actual CSS parser, but just a lexical
  scanner with a tiny bit more smarts bolted on top.  A full parser
  is somewhat overkill for this job.  The only things we're interested
  in are @import rules and url() tokens, so it's easy enough to
  grab those without truly understanding the input.  The only downside
  to this is that we might be coerced into downloading files that
  a browser would ignore.  That might merit some more investigation.
 */

#include <wget.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "wget.h"
#include "utils.h"
#include "convert.h"
#include "html-url.h"
#include "css-tokens.h"
#include "css-url.h"
#include "xstrndup.h"

/* from lex.yy.c */
extern char *yytext;
extern int yyleng;
typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_bytes (const char *bytes,int len  );
extern int yylex (void);

/*
  Given a detected URI token, get only the URI specified within.
  Also adjust the starting position and length of the string.

  A URI can be specified with or without quotes, and the quotes
  can be single or double quotes.  In addition there can be
  whitespace after the opening parenthesis and before the closing
  parenthesis.
*/
static char *
get_uri_string (const char *at, int *pos, int *length)
{
  if (0 != strncasecmp (at + *pos, "url(", 4))
    return NULL;

  *pos += 4;
  *length -= 5; /* url() */
  /* skip leading space */
  while (isspace (at[*pos]))
    {
      (*pos)++;
      if (--(*length) == 0)
        return NULL;
    }

  /* skip trailing space */
  while (isspace (at[*pos + *length - 1]))
    {
      (*length)--;
    }
  /* trim off quotes */
  if (at[*pos] == '\'' || at[*pos] == '"')
    {
      (*pos)++;
      *length -= 2;
    }

  return xstrndup (at + *pos, *length);
}

void
get_urls_css (struct map_context *ctx, int offset, int buf_length)
{
  int token;
  /*char tmp[2048];*/
  int buffer_pos = 0;
  int pos, length;
  char *uri;

  /* tell flex to scan from this buffer */
  yy_scan_bytes (ctx->text + offset, buf_length);

  while((token = yylex()) != CSSEOF)
    {
      /*DEBUGP (("%s ", token_names[token]));*/
      /* @import "foo.css"
         or @import url(foo.css)
      */
      if(token == IMPORT_SYM)
        {
          do {
            buffer_pos += yyleng;
          } while((token = yylex()) == S);

          /*DEBUGP (("%s ", token_names[token]));*/

          if (token == STRING || token == URI)
            {
              /*DEBUGP (("Got URI "));*/
              pos = buffer_pos + offset;
              length = yyleng;

              if (token == URI)
                {
                  uri = get_uri_string (ctx->text, &pos, &length);
                }
              else
                {
                  /* cut out quote characters */
                  pos++;
                  length -= 2;
                  uri = xmalloc (length + 1);
                  memcpy (uri, yytext + 1, length);
                  uri[length] = '\0';
                }

              if (uri)
                {
                  struct urlpos *up = append_url (uri, pos, length, ctx);
                  DEBUGP (("Found @import: [%s] at %d [%s]\n", yytext, buffer_pos, uri));

                  if (up)
                    {
                      up->link_inline_p = 1;
                      up->link_css_p = 1;
                      up->link_expect_css = 1;
                    }

                  xfree(uri);
                }
            }
        }
      /* background-image: url(foo.png)
         note that we don't care what
         property this is actually on.
      */
      else if(token == URI)
        {
          pos = buffer_pos + offset;
          length = yyleng;
          uri = get_uri_string (ctx->text, &pos, &length);

          if (uri)
            {
              struct urlpos *up = append_url (uri, pos, length, ctx);
              DEBUGP (("Found URI: [%s] at %d [%s]\n", yytext, buffer_pos, uri));
              if (up)
                {
                  up->link_inline_p = 1;
                  up->link_css_p = 1;
                }

              xfree (uri);
            }
        }
      buffer_pos += yyleng;
    }
  DEBUGP (("\n"));
}

struct urlpos *
get_urls_css_file (const char *file, const char *url)
{
  struct file_memory *fm;
  struct map_context ctx;

  /* Load the file. */
  fm = wget_read_file (file);
  if (!fm)
    {
      logprintf (LOG_NOTQUIET, "%s: %s\n", file, strerror (errno));
      return NULL;
    }
  DEBUGP (("Loaded %s (size %s).\n", file, number_to_static_string (fm->length)));

  ctx.text = fm->content;
  ctx.head = NULL;
  ctx.base = NULL;
  ctx.parent_base = url ? url : opt.base_href;
  ctx.document_file = file;
  ctx.nofollow = 0;

  get_urls_css (&ctx, 0, fm->length);
  wget_read_file_free (fm);
  return ctx.head;
}
