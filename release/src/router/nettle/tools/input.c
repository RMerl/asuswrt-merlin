/* input.c

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
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"

void
sexp_input_init(struct sexp_input *input, FILE *f)
{
  input->f = f;
  input->coding = NULL;
}

static void
sexp_get_raw_char(struct sexp_input *input)
{
  int c = getc(input->f);
  
  if (c < 0)
    {
      if (ferror(input->f))
	die("Read error: %s\n", strerror(errno));
      
      input->ctype = SEXP_EOF_CHAR;
    }
  else
    {
      input->ctype = SEXP_NORMAL_CHAR;
      input->c = c;
    }
}

void
sexp_get_char(struct sexp_input *input)
{
  if (input->coding)
    for (;;)
      {
	size_t done;

	sexp_get_raw_char(input);
	if (input->ctype == SEXP_EOF_CHAR)
	  die("Unexpected end of file in coded data.\n");

	if (input->c == input->terminator)
	  {
	    input->ctype = SEXP_END_CHAR;
	    return;
	  }

	done = 1;

	/* Decodes in place. Should always work, when we decode one
	 * character at a time. */
	if (!input->coding->decode_update(&input->state,
					  &done, &input->c,
					  1, &input->c))
	  die("Invalid coded data.\n");
	
	if (done)
	  return;
      }
  else
    sexp_get_raw_char(input);
}

static uint8_t
sexp_next_char(struct sexp_input *input)
{
  sexp_get_char(input);
  if (input->ctype != SEXP_NORMAL_CHAR)
    die("Unexpected end of file.\n");

  return input->c;
}

static void
sexp_push_char(struct sexp_input *input,
	       struct nettle_buffer *string)
{
  assert(input->ctype == SEXP_NORMAL_CHAR);
    
  if (!NETTLE_BUFFER_PUTC(string, input->c))
    die("Virtual memory exhasuted.\n");
}

static void
sexp_input_start_coding(struct sexp_input *input,
			const struct nettle_armor *coding,
			uint8_t terminator)
{
  assert(!input->coding);
  
  input->coding = coding;
  input->coding->decode_init(&input->state);
  input->terminator = terminator;
}

static void
sexp_input_end_coding(struct sexp_input *input)
{
  assert(input->coding);

  if (!input->coding->decode_final(&input->state))
    die("Invalid coded data.\n");
  
  input->coding = NULL;
}


/* Return 0 at end-of-string */
static int
sexp_get_quoted_char(struct sexp_input *input)
{
  sexp_next_char(input);

  switch (input->c)
    {
    default:
      return 1;
    case '\"':
      return 0;
    case '\\':
      sexp_next_char(input);
	
      switch (input->c)
	{
	case 'b': input->c = '\b'; return 1;
	case 't': input->c = '\t'; return 1;
	case 'n': input->c = '\n'; return 1;
	case 'f': input->c = '\f'; return 1;
	case 'r': input->c = '\r'; return 1;
	case '\\': input->c = '\\'; return 1;
	case 'o':
	case 'x':
	  /* FIXME: Not implemnted */
	  abort();
	case '\n':
	  if (sexp_next_char(input) == '\r')
	    sexp_next_char(input);

	  break;
	case '\r':
	  if (sexp_next_char(input) == '\n')
	    sexp_next_char(input);

	  break;
	}
      return 1;
    }
}

static void
sexp_get_token_string(struct sexp_input *input,
		      struct nettle_buffer *string)
{
  assert(!input->coding);
  assert(input->ctype == SEXP_NORMAL_CHAR);
  
  if (!TOKEN_CHAR(input->c))
    die("Invalid token.\n");

  do
    {
      sexp_push_char(input, string);
      sexp_get_char(input);
    }
  while (input->ctype == SEXP_NORMAL_CHAR && TOKEN_CHAR(input->c));
  
  assert (string->size);
}

static void
sexp_get_string(struct sexp_input *input,
		struct nettle_buffer *string)
{
  nettle_buffer_reset(string);
  input->token = SEXP_STRING;
  
  switch (input->c)
    {
    case '\"':
      while (sexp_get_quoted_char(input))
	sexp_push_char(input, string);
      
      sexp_get_char(input);
      break;
      
    case '#':
      sexp_input_start_coding(input, &nettle_base16, '#');
      goto decode;

    case '|':
      sexp_input_start_coding(input, &nettle_base64, '|');

    decode:
      for (;;)
	{
	  sexp_get_char(input);
	  switch (input->ctype)
	    {
	    case SEXP_NORMAL_CHAR:
	      sexp_push_char(input, string);
	      break;
	    case SEXP_EOF_CHAR:
	      die("Unexpected end of file in coded string.\n");
	    case SEXP_END_CHAR:
	      sexp_input_end_coding(input);
	      sexp_get_char(input);
	      return;
	    }
	}

      break;

    default:
      sexp_get_token_string(input, string);
      break;
    }
}

static void
sexp_get_string_length(struct sexp_input *input, enum sexp_mode mode,
		       struct nettle_buffer *string)
{
  unsigned length;

  nettle_buffer_reset(string);
  input->token = SEXP_STRING;
  
  length = input->c - '0';
  
  if (!length)
    /* There must be no more digits */
    sexp_next_char(input);

  else
    {
      assert(length < 10);
      /* Get rest of digits */
      for (;;)
	{
	  sexp_next_char(input);
	  
	  if (input->c < '0' || input->c > '9')
	    break;
	  
	  /* FIXME: Check for overflow? */
	  length = length * 10 + input->c - '0';
	}
    }

  switch(input->c)
    {
    case ':':
      /* Verbatim */
      for (; length; length--)
	{
	  sexp_next_char(input);
	  sexp_push_char(input, string);
	}
      
      break;

    case '"':
      if (mode != SEXP_ADVANCED)
	die("Encountered quoted string in canonical mode.\n");

      for (; length; length--)
	if (sexp_get_quoted_char(input))
	  sexp_push_char(input, string);
	else
	  die("Unexpected end of string.\n");
      
      if (sexp_get_quoted_char(input))
	die("Quoted string longer than expected.\n");

      break;
      
    case '#':
      sexp_input_start_coding(input, &nettle_base16, '#');
      goto decode;

    case '|':
      sexp_input_start_coding(input, &nettle_base64, '|');

    decode:
      for (; length; length--)
	{
	  sexp_next_char(input);
	  sexp_push_char(input, string);
	}
      sexp_get_char(input);
      if (input->ctype != SEXP_END_CHAR)
	die("Coded string too long.\n");

      sexp_input_end_coding(input);
      
      break;
      
    default:
      die("Invalid string.\n");
    }

  /* Skip the ending character. */
  sexp_get_char(input);  
}

static void
sexp_get_comment(struct sexp_input *input, struct nettle_buffer *string)
{
  nettle_buffer_reset(string);

  assert(input->ctype == SEXP_NORMAL_CHAR);
  assert(input->c == ';');

  do
    {
      sexp_push_char(input, string);
      sexp_get_raw_char(input);
    }
  while (input->ctype == SEXP_NORMAL_CHAR && input->c != '\n');

  input->token = SEXP_COMMENT;
}

/* When called, input->c should be the first character of the current
 * token.
 *
 * When returning, input->c should be the first character of the next
 * token. */
void
sexp_get_token(struct sexp_input *input, enum sexp_mode mode,
	       struct nettle_buffer *string)
{
  for(;;)
    switch(input->ctype)
      {
      case SEXP_EOF_CHAR:
	input->token = SEXP_EOF;
	return;

      case SEXP_END_CHAR:
	input->token = SEXP_CODING_END;
	sexp_input_end_coding(input);
	sexp_get_char(input);
	return;

      case SEXP_NORMAL_CHAR:
	switch(input->c)
	  {
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
	    sexp_get_string_length(input, mode, string);
	    return;
	  
	  case '(':
	    input->token = SEXP_LIST_START;
	    sexp_get_char(input);
	    return;
	  
	  case ')':
	    input->token = SEXP_LIST_END;
	    sexp_get_char(input);
	    return;

	  case '[':
	    input->token = SEXP_DISPLAY_START;
	    sexp_get_char(input);
	    return;

	  case ']':
	    input->token = SEXP_DISPLAY_END;
	    sexp_get_char(input);
	    return;

	  case '{':
	    if (mode == SEXP_CANONICAL)
	      die("Unexpected transport data in canonical mode.\n");
	    
	    sexp_input_start_coding(input, &nettle_base64, '}');
	    sexp_get_char(input);

	    input->token = SEXP_TRANSPORT_START;
	    
	    return;
	  
	  case ' ':  /* SPC, TAB, LF, CR */
	  case '\t':
	  case '\n':
	  case '\r':
	    if (mode == SEXP_CANONICAL)
	      die("Whitespace encountered in canonical mode.\n");

	    sexp_get_char(input);
	    break;

	  case ';': /* Comments */
	    if (mode == SEXP_CANONICAL)
	      die("Comment encountered in canonical mode.\n");

	    sexp_get_comment(input, string);
	    return;
	  
	  default:
	    /* Ought to be a string */
	    if (mode != SEXP_ADVANCED)
	      die("Encountered advanced string in canonical mode.\n");

	    sexp_get_string(input, string);
	    return;
	  }
      }
}
