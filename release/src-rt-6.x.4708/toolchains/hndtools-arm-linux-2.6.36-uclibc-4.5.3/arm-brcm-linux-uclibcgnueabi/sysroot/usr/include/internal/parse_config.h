/* vi: set sw=4 ts=4: */
/*
 * config file parser helper
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 * Also for use in uClibc (http://uclibc.org/) licensed under LGPLv2.1 or later.
 */
#ifndef __INTERNAL_PARSE_CONFIG_H
#define __INTERNAL_PARSE_CONFIG_H

#include <stdio.h>
#ifndef FAST_FUNC
# define FAST_FUNC
#endif

/*
 * Config file parser
 */
enum {
	PARSE_COLLAPSE  = 0x00010000, /* treat consecutive delimiters as one */
	PARSE_TRIM      = 0x00020000, /* trim leading and trailing delimiters */
/* TODO: COLLAPSE and TRIM seem to always go in pair */
	PARSE_GREEDY    = 0x00040000, /* last token takes entire remainder of the line */
	PARSE_MIN_DIE   = 0x00100000, /* die if < min tokens found */
	/* keep a copy of current line */
	PARSE_KEEP_COPY = 0x00200000 * 0, /*ENABLE_FEATURE_CROND_D, */
/*	PARSE_ESCAPE    = 0x00400000,*/ /* process escape sequences in tokens */
	/* NORMAL is:
	   * remove leading and trailing delimiters and collapse
	     multiple delimiters into one
	   * warn and continue if less than mintokens delimiters found
	   * grab everything into last token
	 */
	PARSE_NORMAL    = PARSE_COLLAPSE | PARSE_TRIM | PARSE_GREEDY,
};

typedef struct parser_t {
	FILE *fp; /* input file */
	char *data; /* pointer to data */
	size_t data_len; /* offset into data of begin of line */
	char *line; /* pointer to beginning of line */
	size_t line_len; /* length of line */
	smalluint allocated;
} parser_t;

parser_t* config_open(const char *filename) FAST_FUNC attribute_hidden;
int config_read(parser_t *parser, char ***tokens, unsigned flags, const char *delims) FAST_FUNC attribute_hidden;
#define config_read(parser, tokens, max, min, str, flags) \
	config_read(parser, tokens, ((flags) | (((min) & 0xFF) << 8) | ((max) & 0xFF)), str)
void config_close(parser_t *parser) FAST_FUNC attribute_hidden;

#endif /* __INTERNAL_PARSE_CONFIG_H */
