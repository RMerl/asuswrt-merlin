/*
 * token.c -- tokenize strings, a la strtok(3)
 *
 * Copyright (C) 2007 Oracle.  All rights reserved.
 * Copyright (C) 2007 Chuck Lever <chuck.lever@oracle.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 *
 */

/*
 * We've constructed a simple string tokenizer that is better than
 * strtok(3) in several ways:
 *
 * 1.  It doesn't interfere with ongoing tokenizations using strtok(3).
 * 2.  It's re-entrant so we can nest tokenizations, if needed.
 * 3.  It can handle double-quoted delimiters (needed for 'context="sd,fslj"').
 * 4.  It doesn't alter the string we're tokenizing, so it can work
 *     on write-protected strings as well as writable strings.
 */


#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "token.h"


struct tokenizer_state {
	char *pos;
	char delimiter;
	int error;
};

static void find_next_nondelimiter(struct tokenizer_state *tstate)
{
	while (*tstate->pos != '\0' && *tstate->pos == tstate->delimiter)
		tstate->pos++;
}

static size_t find_next_delimiter(struct tokenizer_state *tstate)
{
	size_t len = 0;
	int quote_seen = 0;

	while (*tstate->pos != '\0') {
		if (*tstate->pos == '"')
			quote_seen ^= 1;

		if (!quote_seen && *tstate->pos == tstate->delimiter)
			break;

		len++;
		tstate->pos++;
	}

	/* did the string terminate before the close quote? */
	if (quote_seen) {
		tstate->error = EINVAL;
		return 0;
	}

	return len;
}

/**
 * next_token - find the next token in a string and return it
 * @tstate: pointer to tokenizer context object
 *
 * Returns the next token found in the current string.
 * Returns NULL if there are no more tokens in the string,
 * or if an error occurs.
 *
 * Side effect: tstate is updated
 */
char *next_token(struct tokenizer_state *tstate)
{
	char *token;
	size_t len;

	if (!tstate || !tstate->pos || tstate->error)
		return NULL;

	find_next_nondelimiter(tstate);
	if (*tstate->pos == '\0')
		goto fail;
	token = tstate->pos;

	len = find_next_delimiter(tstate);
	if (len) {
		token = strndup(token, len);
		if (token)
			return token;
		tstate->error = ENOMEM;
	}

fail:
	tstate->pos = NULL;
	return NULL;			/* no tokens found in this string */
}

/**
 * init_tokenizer - return an initialized tokenizer context object
 * @string: pointer to C string
 * @delimiter: single character that delimits tokens in @string
 *
 * Returns an initialized tokenizer context object
 */
struct tokenizer_state *init_tokenizer(char *string, char delimiter)
{
	struct tokenizer_state *tstate;

	tstate = malloc(sizeof(*tstate));
	if (tstate) {
		tstate->pos = string;
		tstate->delimiter = delimiter;
		tstate->error = 0;
	}
	return tstate;
}

/**
 * tokenizer_error - digs error value out of tokenizer context
 * @tstate: pointer to tokenizer context object
 *
 */
int tokenizer_error(struct tokenizer_state *tstate)
{
	return tstate ? tstate->error : 0;
}

/**
 * end_tokenizer - free a tokenizer context object
 * @tstate: pointer to tokenizer context object
 *
 */
void end_tokenizer(struct tokenizer_state *tstate)
{
	free(tstate);
}
