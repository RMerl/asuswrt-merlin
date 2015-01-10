/*
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <string.h>

/*
 * match_fstype:
 * @type: filesystem type
 * @pattern: filesystem name or comma delimited list of names
 *
 * The @pattern list of filesystem can be prefixed with a global
 * "no" prefix to invert matching of the whole list. The "no" could
 * also be used for individual items in the @pattern list. So,
 * "nofoo,bar" has the same meaning as "nofoo,nobar".
 */
int match_fstype(const char *type, const char *pattern)
{
	int no = 0;		/* negated types list */
	int len;
	const char *p;

	if (!pattern && !type)
		return 1;
	if (!pattern)
		return 0;

	if (!strncmp(pattern, "no", 2)) {
		no = 1;
		pattern += 2;
	}

	/* Does type occur in types, separated by commas? */
	len = strlen(type);
	p = pattern;
	while(1) {
		if (!strncmp(p, "no", 2) && !strncmp(p+2, type, len) &&
		    (p[len+2] == 0 || p[len+2] == ','))
			return 0;
		if (strncmp(p, type, len) == 0 && (p[len] == 0 || p[len] == ','))
			return !no;
		p = strchr(p,',');
		if (!p)
			break;
		p++;
	}
	return no;
}
