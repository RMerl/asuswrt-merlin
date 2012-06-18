/***********************************************************************
*
* options.c
*
* Code for parsing options out of configuration file.
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: options.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>

l2tp_settings Settings;

static option_handler *option_handlers = NULL;

/* Function for currently-active option context */
static int (*option_context_fn)(EventSelector *es,
				char const *name, char const *value);
static int do_load_handler(EventSelector *es,
			   l2tp_opt_descriptor *desc, char const *value);
static int set_option(EventSelector *es,
		      l2tp_opt_descriptor *desc, char const *value);

/* Global options */
static l2tp_opt_descriptor global_opts[] = {
    /*  name               type                 addr */
    { "load-handler",      OPT_TYPE_CALLFUNC,   (void *) do_load_handler },
    { "listen-port",       OPT_TYPE_PORT,       &Settings.listen_port },
    { "listen-addr",       OPT_TYPE_IPADDR,     &Settings.listen_addr },
    { NULL,                OPT_TYPE_BOOL,       NULL }
};

/**********************************************************************
* %FUNCTION: do_load_handler
* %ARGUMENTS:
*  es -- event selector
*  desc -- option descriptor
*  value -- name of handler to load
* %RETURNS:
*  0 on success, -1 on failure
* %DESCRIPTION:
*  Loads a DLL as a handler
***********************************************************************/
static int
do_load_handler(EventSelector *es,
		l2tp_opt_descriptor *desc,
		char const *value)
{
    return l2tp_load_handler(es, value);
}

/**********************************************************************
* %FUNCTION: set_option
* %ARGUMENTS:
*  es -- event selector
*  desc -- option descriptor
*  value -- value string parsed from config file
* %RETURNS:
*  -1 on error, 0 if all is OK
* %DESCRIPTION:
*  Sets an option value.
***********************************************************************/
static int
set_option(EventSelector *es,
	   l2tp_opt_descriptor *desc,
	   char const *value)
{
    long x;
    char *end;
    struct hostent *he;
    int (*fn)(EventSelector *, l2tp_opt_descriptor *, char const *);

    switch(desc->type) {
    case OPT_TYPE_BOOL:
	if (!strcasecmp(value, "true") ||
	    !strcasecmp(value, "yes") ||
	    !strcasecmp(value, "on") ||
	    !strcasecmp(value, "1")) {
	    * (int *) (desc->addr) = 1;
	    return 0;
	}
	if (!strcasecmp(value, "false") ||
	    !strcasecmp(value, "no") ||
	    !strcasecmp(value, "off") ||
	    !strcasecmp(value, "0")) {
	    * (int *) (desc->addr) = 0;
	    return 0;
	}
	l2tp_set_errmsg("Expecting boolean value, found '%s'", value);
	return -1;

    case OPT_TYPE_INT:
    case OPT_TYPE_PORT:
	x = strtol(value, &end, 0);
	if (*end) {
	    l2tp_set_errmsg("Expecting integer value, found '%s'", value);
	    return -1;
	}
	if (desc->type == OPT_TYPE_PORT) {
	    if (x < 1 || x > 65535) {
		l2tp_set_errmsg("Port values must range from 1 to 65535");
		return -1;
	    }
	}

	* (int *) desc->addr = (int) x;
	return 0;

    case OPT_TYPE_IPADDR:
	he = gethostbyname(value);
	if (!he) {
	    l2tp_set_errmsg("Could not resolve %s as IP address: %s",
		       value, strerror(errno));
	    return -1;
	}

	memcpy(desc->addr, he->h_addr, sizeof(he->h_addr));
	return 0;

    case OPT_TYPE_STRING:
	if (* (char **) desc->addr) {
	    free(* (char **) desc->addr);
	}
	* (char **) desc->addr = strdup(value);
	if (! * (char *) desc->addr) {
	    l2tp_set_errmsg("Out of memory");
	    return -1;
	}
	return 0;

    case OPT_TYPE_CALLFUNC:
	fn = (int (*)(EventSelector *, l2tp_opt_descriptor *, char const *)) desc->addr;
	return fn(es, desc, value);
    }
    l2tp_set_errmsg("Unknown value type %d", desc->type);
    return -1;
}

/**********************************************************************
* %FUNCTION: chomp_word
* %ARGUMENTS:
*  line -- the input line
*  word -- buffer for storing word
* %RETURNS:
*  Updated value of line
* %DESCRIPTION:
*  Chomps a word from line
***********************************************************************/
char const *
l2tp_chomp_word(char const *line, char *word)
{
    *word = 0;

    /* Chew up whitespace */
    while(*line && isspace(*line)) line++;

    if (*line != '"') {
	/* Not quoted string */
	while (*line && !isspace(*line)) {
	    *word++ = *line++;
	}
	*word = 0;
	return line;
    }

    /* Quoted string */
    line++;
    while(*line) {
	if (*line != '\\') {
	    if (*line == '"') {
		line++;
		*word = 0;
		return line;
	    }
	    *word++ = *line++;
	    continue;
	}
	line++;
	if (*line) *word++ = *line++;
    }
    *word = 0;
    return line;
}

/**********************************************************************
* %FUNCTION: split_line_into_words
* %ARGUMENTS:
*  line -- the input line
*  name, value -- buffers which are large enough to contain all chars in line
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Splits line into two words.  A word is:
*    - Non-whitespace chars:  foobarbazblech_3
*    - Quoted text:           "Here is text \"with embedded quotes\""
***********************************************************************/
static void
split_line_into_words(char const *line, char *name, char *value)
{
    line = l2tp_chomp_word(line, name);
    line = l2tp_chomp_word(line, value);
}

/**********************************************************************
* %FUNCTION: parser_switch_context
* %ARGUMENTS:
*  name, value -- words read from line.  Either "global ..ignored.."
*                 or "section context"
* %RETURNS:
*  0 if context-switch proceeded OK, -1 if not.
* %DESCRIPTION:
*  Switches configuration contexts
***********************************************************************/
static int
parser_switch_context(EventSelector *es,
		      char const *name,
		      char const *value)
{
    int r;
    option_handler *handler;

    /* Switch out of old context */
    if (option_context_fn) {
	r = option_context_fn(es, "*end*", "*end*");
	option_context_fn = NULL;
	if (r < 0) return r;
    }

    if (!strcasecmp(name, "global")) {
	return 0;
    }

    /* Must be "section foo" */
    handler = option_handlers;
    while(handler) {
	if (!strcasecmp(value, handler->section)) {
	    option_context_fn = handler->process_option;
	    option_context_fn(es, "*begin*", "*begin*");
	    return 0;
	}
	handler = handler->next;
    }
    l2tp_set_errmsg("No handler for section %s", value);
    return -1;
}

/**********************************************************************
* %FUNCTION: option_set
* %ARGUMENTS:
*  es -- event selector
*  name -- name of option
*  value -- value of option
*  descriptors -- array of option descriptors for this context
* %RETURNS:
*  0 on success, -1 on failure
* %DESCRIPTION:
*  Sets an option
***********************************************************************/
int
l2tp_option_set(EventSelector *es,
		char const *name,
		char const *value,
		l2tp_opt_descriptor descriptors[])
{
    int i;

    for (i=0; descriptors[i].name; i++) {
	if (!strcasecmp(descriptors[i].name, name)) {
	    return set_option(es, &descriptors[i], value);
	}
    }
    l2tp_set_errmsg("Option %s is not known in this context",
	       name);
    return -1;
}

/**********************************************************************
* %FUNCTION: option_register_section
* %ARGUMENTS:
*  handler -- an option-handler
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Adds handler to linked-list of sections.
***********************************************************************/
void
l2tp_option_register_section(option_handler *h)
{
    h->next = option_handlers;
    option_handlers = h;
}

/**********************************************************************
* %FUNCTION: handle_option
* %ARGUMENTS:
*  es -- event selector
*  name -- name of option
*  value -- option's value
* %RETURNS:
*  0 on success, -1 on failure
* %DESCRIPTION:
*  Handles an option
***********************************************************************/
static int
handle_option(EventSelector *es,
	      char const *name,
	      char const *value)
{
    if (option_context_fn) {
	return option_context_fn(es, name, value);
    }

    return l2tp_option_set(es, name, value, global_opts);
}

/**********************************************************************
* %FUNCTION: parse_config_file
* %ARGUMENTS:
*  es -- event selector
*  fname -- filename to parse
* %RETURNS:
*  -1 on error, 0 if all is OK
* %DESCRIPTION:
*  Parses configuration file.
***********************************************************************/
int
l2tp_parse_config_file(EventSelector *es,
		       char const *fname)
{
    char buf[512];
    char name[512];
    char value[512];
    int r = 0;
    size_t l;
    char *line;
    FILE *fp;

    /* Defaults */
    Settings.listen_port = 1701;
    Settings.listen_addr.s_addr = htonl(INADDR_ANY);

    fp = fopen(fname, "r");
    if (!fp) {
	l2tp_set_errmsg("Could not open '%s' for reading: %s",
		   fname, strerror(errno));
	return -1;
    }

    /* Start in global context */
    option_context_fn = NULL;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	l = strlen(buf);
	if (l && (buf[l] == '\n')) {
	    buf[l--] = 0;
	}

	/* Skip leading whitespace */
	line = buf;
	while(*line && isspace(*line)) line++;

	/* Ignore blank lines and comments */
	if (!*line || *line == '#') {
	    continue;
	}

	/* Split line into two words */
	split_line_into_words(line, name, value);

	/* Check for context switch */
	if (!strcasecmp(name, "global") ||
	    !strcasecmp(name, "section")) {
	    r = parser_switch_context(es, name, value);
	    if (r < 0) break;
	    continue;
	}

	r = handle_option(es, name, value);
	if (r < 0) break;
    }
    fclose(fp);
    if (r >= 0) {
	if (option_context_fn) {
	    r = option_context_fn(es, "*end*", "*end*");
	    option_context_fn = NULL;
	}
    }

    return r;
}
