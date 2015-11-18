/* parse.c

   Copyright (C) 2002, 2003, 2008 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

#include "parse.h"

#include "input.h"

void
sexp_compound_token_init(struct sexp_compound_token *token)
{
  token->type = 0;
  nettle_buffer_init(&token->display);
  nettle_buffer_init(&token->string);
}

void
sexp_compound_token_clear(struct sexp_compound_token *token)
{
  nettle_buffer_clear(&token->display);
  nettle_buffer_clear(&token->string);
}

void
sexp_parse_init(struct sexp_parser *parser,
		struct sexp_input *input,
		enum sexp_mode mode)
{
  parser->input = input;
  parser->mode = mode;

  /* Start counting with 1 for the top level, to make comparisons
   * between transport and level simpler.
   *
   * FIXME: Is that trick ugly? */
  parser->level = 1;
  parser->transport = 0;
}

/* Get next token, and check that it is of the expected kind. */
static void
sexp_check_token(struct sexp_parser *parser,
		 enum sexp_token token,
		 struct nettle_buffer *string)
{
  sexp_get_token(parser->input,
		 parser->transport ? SEXP_CANONICAL : parser->mode,
		 string);

  if (parser->input->token != token)
    die("Syntax error.\n");
}

/* Performs further processing of the input, in particular display
 * types and transport decoding.
 *
 * This is complicated a little by the requirement that a
 * transport-encoded block, {xxxxx}, must include exactly one
 * expression. We check at the end of strings and list whether or not
 * we should expect a SEXP_CODING_END as the next token. */
void
sexp_parse(struct sexp_parser *parser,
	   struct sexp_compound_token *token)
{
  for (;;)
    {
      sexp_get_token(parser->input,
		     parser->transport ? SEXP_CANONICAL : parser->mode,
		     &token->string);

      switch(parser->input->token)
	{
	case SEXP_LIST_END:
	  if (parser->level == parser->transport)
	    die("Unmatched end of list in transport encoded data.\n");
	  parser->level--;

	  if (!parser->level)
	    die("Unmatched end of list.\n");

	  token->type = SEXP_LIST_END;

	check_transport_end:
	  if (parser->level == parser->transport)
	    {
	      sexp_check_token(parser, SEXP_CODING_END, &token->string);
	      assert(parser->transport);
	      assert(parser->level == parser->transport);

	      parser->level--;
	      parser->transport = 0;
	    }
	  return;
    
	case SEXP_EOF:
	  if (parser->level > 1)
	    die("Unexpected end of file.\n");

	  token->type = SEXP_EOF;
	  return;

	case SEXP_LIST_START:
	  parser->level++;
	  token->type = SEXP_LIST_START;
	  return;

	case SEXP_DISPLAY_START:
	  sexp_check_token(parser, SEXP_STRING, &token->display);
	  sexp_check_token(parser, SEXP_DISPLAY_END, &token->display);
	  sexp_check_token(parser, SEXP_STRING, &token->string);

	  token->type = SEXP_DISPLAY;
	  goto check_transport_end;

	case SEXP_STRING:
	  token->type = SEXP_STRING;
	  goto check_transport_end;

	case SEXP_COMMENT:
	  token->type = SEXP_COMMENT;
	  return;

	case SEXP_TRANSPORT_START:
	  if (parser->mode == SEXP_CANONICAL)
	    die("Base64 not allowed in canonical mode.\n");
	  parser->level++;
	  parser->transport = parser->level;

	  continue;

	case SEXP_CODING_END:
	  die("Unexpected end of transport encoding.\n");
	  
	default:
	  /* Internal error. */
	  abort();
	}
    }
}
