/*
 * Copyright (C) 2003 by Martin Pool
 * Copyright (C) 2003 by Andrew Bartlett
 *
 * Test harness for push_ucs2
 */

#include "includes.h"

static int check_push_ucs2(const char *orig) 
{
	smb_ucs2_t *dest = NULL;
	char *orig2 = NULL;
	int ret;
	size_t converted_size;

	push_ucs2_talloc(NULL, &dest, orig, &converted_size);
	pull_ucs2_talloc(NULL, &orig2, dest, &converted_size);
	ret = strcmp(orig, orig2);
	if (ret) {
		fprintf(stderr, "orig: %s\n", orig);
		fprintf(stderr, "orig (UNIX -> UCS2 -> UNIX): %s\n", orig2);
	}

	TALLOC_FREE(dest);
	TALLOC_FREE(orig2);

	return ret;
}

int main(int argc, char *argv[])
{
	int i, ret = 0;
	int count = 1;

	/* Needed to initialize character set */
	lp_load("/dev/null", True, False, False, True);

	if (argc < 2) {
		fprintf(stderr, "usage: %s STRING1 [COUNT]\n"
			"Checks that a string translated UNIX->UCS2->UNIX is unchanged\n"
			"Should be always 0\n",
			argv[0]);
		return 2;
	}
	if (argc >= 3)
		count = atoi(argv[2]);

	for (i = 0; ((i < count) && (!ret)); i++)
		ret = check_push_ucs2(argv[1]);

	printf("%d\n", ret);
	
	return 0;
}
