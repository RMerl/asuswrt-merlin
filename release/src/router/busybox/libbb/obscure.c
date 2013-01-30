/* vi: set sw=4 ts=4: */
/*
 * Mini weak password checker implementation for busybox
 *
 * Copyright (C) 2006 Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/*	A good password:
	1)	should contain at least six characters (man passwd);
	2)	empty passwords are not permitted;
	3)	should contain a mix of four different types of characters
		upper case letters,
		lower case letters,
		numbers,
		special characters such as !@#$%^&*,;".
	This password types should not  be permitted:
	a)	pure numbers: birthdates, social security number, license plate, phone numbers;
	b)	words and all letters only passwords (uppercase, lowercase or mixed)
		as palindromes, consecutive or repetitive letters
		or adjacent letters on your keyboard;
	c)	username, real name, company name or (e-mail?) address
		in any form (as-is, reversed, capitalized, doubled, etc.).
		(we can check only against username, gecos and hostname)
	d)	common and obvious letter-number replacements
		(e.g. replace the letter O with number 0)
		such as "M1cr0$0ft" or "P@ssw0rd" (CAVEAT: we cannot check for them
		without the use of a dictionary).

	For each missing type of characters an increase of password length is
	requested.

	If user is root we warn only.

	CAVEAT: some older versions of crypt() truncates passwords to 8 chars,
	so that aaaaaaaa1Q$ is equal to aaaaaaaa making it possible to fool
	some of our checks. We don't test for this special case as newer versions
	of crypt do not truncate passwords.
*/

#include "libbb.h"

static int string_checker_helper(const char *p1, const char *p2) __attribute__ ((__pure__));

static int string_checker_helper(const char *p1, const char *p2)
{
	/* as-is or capitalized */
	if (strcasecmp(p1, p2) == 0
	/* as sub-string */
	|| strcasestr(p2, p1) != NULL
	/* invert in case haystack is shorter than needle */
	|| strcasestr(p1, p2) != NULL)
		return 1;
	return 0;
}

static int string_checker(const char *p1, const char *p2)
{
	int size;
	/* check string */
	int ret = string_checker_helper(p1, p2);
	/* Make our own copy */
	char *p = xstrdup(p1);
	/* reverse string */
	size = strlen(p);

	while (size--) {
		*p = p1[size];
		p++;
	}
	/* restore pointer */
	p -= strlen(p1);
	/* check reversed string */
	ret |= string_checker_helper(p, p2);
	/* clean up */
	memset(p, 0, strlen(p1));
	free(p);
	return ret;
}

#define LOWERCASE          1
#define UPPERCASE          2
#define NUMBERS            4
#define SPECIAL            8

static const char *obscure_msg(const char *old_p, const char *new_p, const struct passwd *pw)
{
	int i;
	int c;
	int length;
	int mixed = 0;
	/* Add 2 for each type of characters to the minlen of password */
	int size = CONFIG_PASSWORD_MINLEN + 8;
	const char *p;
	char *hostname;

	/* size */
	if (!new_p || (length = strlen(new_p)) < CONFIG_PASSWORD_MINLEN)
		return "too short";

	/* no username as-is, as sub-string, reversed, capitalized, doubled */
	if (string_checker(new_p, pw->pw_name)) {
		return "similar to username";
	}
	/* no gecos as-is, as sub-string, reversed, capitalized, doubled */
	if (*pw->pw_gecos && string_checker(new_p, pw->pw_gecos)) {
		return "similar to gecos";
	}
	/* hostname as-is, as sub-string, reversed, capitalized, doubled */
	hostname = safe_gethostname();
	i = string_checker(new_p, hostname);
	free(hostname);
	if (i)
		return "similar to hostname";

	/* Should / Must contain a mix of: */
	for (i = 0; i < length; i++) {
		if (islower(new_p[i])) {        /* a-z */
			mixed |= LOWERCASE;
		} else if (isupper(new_p[i])) { /* A-Z */
			mixed |= UPPERCASE;
		} else if (isdigit(new_p[i])) { /* 0-9 */
			mixed |= NUMBERS;
		} else  {                       /* special characters */
			mixed |= SPECIAL;
		}
		/* More than 50% similar characters ? */
		c = 0;
		p = new_p;
		while (1) {
			p = strchr(p, new_p[i]);
			if (p == NULL) {
				break;
			}
			c++;
			if (!++p) {
				break; /* move past the matched char if possible */
			}
		}

		if (c >= (length / 2)) {
			return "too many similar characters";
		}
	}
	for (i=0; i<4; i++)
		if (mixed & (1<<i)) size -= 2;
	if (length < size)
		return "too weak";

	if (old_p && old_p[0] != '\0') {
		/* check vs. old password */
		if (string_checker(new_p, old_p)) {
			return "similar to old password";
		}
	}
	return NULL;
}

int FAST_FUNC obscure(const char *old, const char *newval, const struct passwd *pw)
{
	const char *msg;

	msg = obscure_msg(old, newval, pw);
	if (msg) {
		printf("Bad password: %s\n", msg);
		return 1;
	}
	return 0;
}
