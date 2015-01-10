/*
 * Copyright (C) 2003, 2004, 2005 Thorsten Kukuk
 * Author: Thorsten Kukuk <kukuk@suse.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain any existing copyright
 *    notice, and this entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 *
 * 2. Redistributions in binary form must reproduce all prior and current
 *    copyright notices, this list of conditions, and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *   written permission.
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>

#include "c.h"
#include "logindefs.h"
#include "nls.h"
#include "pathnames.h"
#include "xalloc.h"

struct item {
	char *name;		/* name of the option.  */
	char *value;		/* value of the option.  */
	char *path;		/* name of config file for this option.  */

	struct item *next;	/* pointer to next option.  */
};

static struct item *list = NULL;

void free_getlogindefs_data(void)
{
	struct item *ptr;

	ptr = list;
	while (ptr) {
		struct item *tmp = ptr->next;

		free(ptr->path);
		free(ptr->name);
		free(ptr->value);
		free(ptr);
		ptr = tmp;
	}

	list = NULL;
}

static void store(const char *name, const char *value, const char *path)
{
	struct item *new = xmalloc(sizeof(struct item));

	if (!name)
		abort();

	new->name = xstrdup(name);
	new->value = value && *value ? xstrdup(value) : NULL;
	new->path = xstrdup(path);
	new->next = list;
	list = new;
}

static void load_defaults(const char *filename)
{
	FILE *f;
	char buf[BUFSIZ];

	f = fopen(filename, "r");
	if (!f)
		return;

	while (fgets(buf, sizeof(buf), f)) {

		char *p, *name, *data = NULL;

		if (*buf == '#' || *buf == '\n')
			continue;	/* only comment or empty line */

		p = strchr(buf, '#');
		if (p)
			*p = '\0';
		else {
			size_t n = strlen(buf);
			if (n && *(buf + n - 1) == '\n')
				*(buf + n - 1) = '\0';
		}

		if (!*buf)
			continue;	/* empty line */

		/* ignore space at begin of the line */
		name = buf;
		while (*name && isspace((unsigned)*name))
			name++;

		/* go to the end of the name */
		data = name;
		while (*data && !(isspace((unsigned)*data) || *data == '='))
			data++;
		if (data > name && *data)
			*data++ = '\0';

		if (!*name || data == name)
			continue;

		/* go to the begin of the value */
		while (*data
		       && (isspace((unsigned)*data) || *data == '='
			   || *data == '"'))
			data++;

		/* remove space at the end of the value */
		p = data + strlen(data);
		if (p > data)
			p--;
		while (p > data && (isspace((unsigned)*p) || *p == '"'))
			*p-- = '\0';

		store(name, data, filename);
	}

	fclose(f);
}

static struct item *search(const char *name)
{
	struct item *ptr;

	if (!list)
		load_defaults(_PATH_LOGINDEFS);

	ptr = list;
	while (ptr != NULL) {
		if (strcasecmp(name, ptr->name) == 0)
			return ptr;
		ptr = ptr->next;
	}

	return NULL;
}

static const char *search_config(const char *name)
{
	struct item *ptr;

	ptr = list;
	while (ptr != NULL) {
		if (strcasecmp(name, ptr->name) == 0)
			return ptr->path;
		ptr = ptr->next;
	}

	return NULL;
}

int getlogindefs_bool(const char *name, int dflt)
{
	struct item *ptr = search(name);
	return ptr && ptr->value ? (strcasecmp(ptr->value, "yes") == 0) : dflt;
}

long getlogindefs_num(const char *name, long dflt)
{
	struct item *ptr = search(name);
	char *end = NULL;
	long retval;

	if (!ptr || !ptr->value)
		return dflt;

	errno = 0;
	retval = strtol(ptr->value, &end, 0);
	if (end && *end == '\0' && !errno)
		return retval;

	syslog(LOG_NOTICE, _("%s: %s contains invalid numerical value: %s"),
	       search_config(name), name, ptr->value);
	return dflt;
}

/*
 * Returns:
 *	@dflt		if @name not found
 *	""		(empty string) if found, but value not defined
 *	"string"	if found
 */
const char *getlogindefs_str(const char *name, const char *dflt)
{
	struct item *ptr = search(name);

	if (!ptr)
		return dflt;
	if (!ptr->value)
		return "";
	return ptr->value;
}

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
	char *name, *type;

	if (argc <= 1)
		errx(EXIT_FAILURE, "usage: %s <filename> "
		     "[<str|num|bool> <valname>]", argv[0]);

	load_defaults(argv[1]);

	if (argc != 4) {	/* list all */
		struct item *ptr;

		for (ptr = list; ptr; ptr = ptr->next)
			printf("%s: $%s: '%s'\n", ptr->path, ptr->name,
			       ptr->value);

		return EXIT_SUCCESS;
	}

	type = argv[2];
	name = argv[3];

	if (strcmp(type, "str") == 0)
		printf("$%s: '%s'\n", name, getlogindefs_str(name, "DEFAULT"));
	else if (strcmp(type, "num") == 0)
		printf("$%s: '%ld'\n", name, getlogindefs_num(name, 0));
	else if (strcmp(type, "bool") == 0)
		printf("$%s: '%s'\n", name,
		       getlogindefs_bool(name, 0) ? "Y" : "N");

	return EXIT_SUCCESS;
}
#endif
