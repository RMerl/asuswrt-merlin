/*
 * Command line options parser.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id: miniopt.h 241182 2011-02-17 21:50:03Z $
 */


#ifndef MINI_OPT_H
#define MINI_OPT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Include Files ---------------------------------------------------- */
/* ---- Constants and Types ---------------------------------------------- */

#define MINIOPT_MAXKEY	128	/* Max options */
typedef struct miniopt {

	/* These are persistent after miniopt_init() */
	const char* name;		/* name for prompt in error strings */
	const char* flags;		/* option chars that take no args */
	bool longflags;		/* long options may be flags */
	bool opt_end;		/* at end of options (passed a "--") */

	/* These are per-call to miniopt() */

	int consumed;		/* number of argv entries cosumed in
				 * the most recent call to miniopt()
				 */
	bool positional;
	bool good_int;		/* 'val' member is the result of a sucessful
				 * strtol conversion of the option value
				 */
	char opt;
	char key[MINIOPT_MAXKEY];
	char* valstr;		/* positional param, or value for the option,
				 * or null if the option had
				 * no accompanying value
				 */
	uint uval;		/* strtol translation of valstr */
	int  val;		/* strtol translation of valstr */
} miniopt_t;

void miniopt_init(miniopt_t *t, const char* name, const char* flags, bool longflags);
int miniopt(miniopt_t *t, char **argv);


/* ---- Variable Externs ------------------------------------------------- */
/* ---- Function Prototypes ---------------------------------------------- */


#ifdef __cplusplus
	}
#endif

#endif  /* MINI_OPT_H  */
