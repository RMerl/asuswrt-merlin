/*
 * Copyright (C) 2007 Karel Zak <kzak@redhat.com>
 *
 * This file is part of util-linux.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

typedef struct {
	const char	*name;
	int		(*fnc)(void);
} mntHlpfnc;

int
hlp_wordsize(void)
{
	printf("%lu\n", CHAR_BIT * sizeof(void*));
	return 0;
}

int
hlp_endianness(void)
{
#if !defined(WORDS_BIGENDIAN)
	printf("LE\n");
#else
	printf("BE\n");
#endif
	return 0;
}


int
hlp_pagesize(void)
{
	printf("%d\n", getpagesize());
	return 0;
}

int
hlp_int_max(void)
{
	printf("%d\n", INT_MAX);
	return 0;
}

int
hlp_uint_max(void)
{
	printf("%u\n", UINT_MAX);
	return 0;
}

int
hlp_long_max(void)
{
	printf("%ld\n", LONG_MAX);
	return 0;
}

int
hlp_ulong_max(void)
{
	printf("%lu\n", ULONG_MAX);
	return 0;
}

int
hlp_ulong_max32(void)
{
#if __WORDSIZE == 64
	printf("%lu\n", ULONG_MAX >> 32);
#else
	printf("%lu\n", ULONG_MAX);
#endif
	return 0;
}

mntHlpfnc hlps[] =
{
	{ "WORDSIZE",	hlp_wordsize	},
	{ "pagesize",	hlp_pagesize	},
	{ "INT_MAX",	hlp_int_max	},
	{ "UINT_MAX",   hlp_uint_max	},
	{ "LONG_MAX",   hlp_long_max	},
	{ "ULONG_MAX",  hlp_ulong_max	},
	{ "ULONG_MAX32",hlp_ulong_max32	},
	{ "byte-order", hlp_endianness  },
	{ NULL, NULL }
};

int
main(int argc, char **argv)
{
	int re = 0;
	mntHlpfnc *fn;

	if (argc == 1) {
		for (fn = hlps; fn->name; fn++) {
			printf("%15s: ", fn->name);
			re += fn->fnc();
		}
	} else {
		int i;

		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
			printf("%s <option>\n", argv[0]);
			fputs("options:\n", stdout);
			for (fn = hlps; fn->name; fn++)
				printf("\t%s\n", fn->name);
			exit(EXIT_SUCCESS);
		}

		for (i=1; i < argc; i++) {
			for (fn = hlps; fn->name; fn++) {
				if (strcmp(fn->name, argv[i]) == 0)
					re += fn->fnc();
			}
		}
	}

	exit(re ? EXIT_FAILURE : EXIT_SUCCESS);
}

