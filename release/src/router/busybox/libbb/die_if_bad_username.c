/* vi: set sw=4 ts=4: */
/*
 * Check user and group names for illegal characters
 *
 * Copyright (C) 2008 Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* To avoid problems, the username should consist only of
 * letters, digits, underscores, periods, at signs and dashes,
 * and not start with a dash (as defined by IEEE Std 1003.1-2001).
 * For compatibility with Samba machine accounts $ is also supported
 * at the end of the username.
 */

void FAST_FUNC die_if_bad_username(const char *name)
{
	/* 1st char being dash or dot isn't valid: */
	goto skip;
	/* For example, name like ".." can make adduser
	 * chown "/home/.." recursively - NOT GOOD
	 */

	do {
		if (*name == '-' || *name == '.')
			continue;
 skip:
		if (isalnum(*name)
		 || *name == '_'
		 || *name == '@'
		 || (*name == '$' && !name[1])
		) {
			continue;
		}
		bb_error_msg_and_die("illegal character '%c'", *name);
	} while (*++name);
}
