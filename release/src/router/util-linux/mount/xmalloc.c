#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* strdup() */
#include "xmalloc.h"
#include "nls.h"	/* _() */
#include "sundries.h"	/* EX_SYSERR */

static void
die_if_null(void *t) {
	if (t == NULL)
		die(EX_SYSERR, _("not enough memory"));
}

void *
xmalloc (size_t size) {
	void *t;

	if (size == 0)
		return NULL;

	t = malloc(size);
	die_if_null(t);

	return t;
}

void *
xrealloc (void *p, size_t size) {
	void *t;

	t = realloc(p, size);
	die_if_null(t);

	return t;
}

char *
xstrdup (const char *s) {
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup(s);
	die_if_null(t);

	return t;
}
