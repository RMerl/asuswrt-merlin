/* vi: set sw=4 ts=4: */
/*
 * SuS3 compliant sort implementation for busybox
 *
 * Copyright (C) 2004 by Rob Landley <rob@landley.net>
 *
 * MAINTAINER: Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * See SuS3 sort standard at:
 * http://www.opengroup.org/onlinepubs/007904975/utilities/sort.html
 */

//usage:#define sort_trivial_usage
//usage:       "[-nru"
//usage:	IF_FEATURE_SORT_BIG("gMcszbdfimSTokt] [-o FILE] [-k start[.offset][opts][,end[.offset][opts]] [-t CHAR")
//usage:       "] [FILE]..."
//usage:#define sort_full_usage "\n\n"
//usage:       "Sort lines of text\n"
//usage:	IF_FEATURE_SORT_BIG(
//usage:     "\n	-b	Ignore leading blanks"
//usage:     "\n	-c	Check whether input is sorted"
//usage:     "\n	-d	Dictionary order (blank or alphanumeric only)"
//usage:     "\n	-f	Ignore case"
//usage:     "\n	-g	General numerical sort"
//usage:     "\n	-i	Ignore unprintable characters"
//usage:     "\n	-k	Sort key"
//usage:     "\n	-M	Sort month"
//usage:	)
//usage:     "\n	-n	Sort numbers"
//usage:	IF_FEATURE_SORT_BIG(
//usage:     "\n	-o	Output to file"
//usage:     "\n	-k	Sort by key"
//usage:     "\n	-t CHAR	Key separator"
//usage:	)
//usage:     "\n	-r	Reverse sort order"
//usage:	IF_FEATURE_SORT_BIG(
//usage:     "\n	-s	Stable (don't sort ties alphabetically)"
//usage:	)
//usage:     "\n	-u	Suppress duplicate lines"
//usage:	IF_FEATURE_SORT_BIG(
//usage:     "\n	-z	Lines are terminated by NUL, not newline"
//usage:     "\n	-mST	Ignored for GNU compatibility")
//usage:
//usage:#define sort_example_usage
//usage:       "$ echo -e \"e\\nf\\nb\\nd\\nc\\na\" | sort\n"
//usage:       "a\n"
//usage:       "b\n"
//usage:       "c\n"
//usage:       "d\n"
//usage:       "e\n"
//usage:       "f\n"
//usage:	IF_FEATURE_SORT_BIG(
//usage:		"$ echo -e \"c 3\\nb 2\\nd 2\" | $SORT -k 2,2n -k 1,1r\n"
//usage:		"d 2\n"
//usage:		"b 2\n"
//usage:		"c 3\n"
//usage:	)
//usage:       ""

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */


/*
	sort [-m][-o output][-bdfinru][-t char][-k keydef]... [file...]
	sort -c [-bdfinru][-t char][-k keydef][file]
*/

/* These are sort types */
static const char OPT_STR[] ALIGN1 = "ngMucszbrdfimS:T:o:k:t:";
enum {
	FLAG_n  = 1,            /* Numeric sort */
	FLAG_g  = 2,            /* Sort using strtod() */
	FLAG_M  = 4,            /* Sort date */
/* ucsz apply to root level only, not keys.  b at root level implies bb */
	FLAG_u  = 8,            /* Unique */
	FLAG_c  = 0x10,         /* Check: no output, exit(!ordered) */
	FLAG_s  = 0x20,         /* Stable sort, no ascii fallback at end */
	FLAG_z  = 0x40,         /* Input and output is NUL terminated, not \n */
/* These can be applied to search keys, the previous four can't */
	FLAG_b  = 0x80,         /* Ignore leading blanks */
	FLAG_r  = 0x100,        /* Reverse */
	FLAG_d  = 0x200,        /* Ignore !(isalnum()|isspace()) */
	FLAG_f  = 0x400,        /* Force uppercase */
	FLAG_i  = 0x800,        /* Ignore !isprint() */
	FLAG_m  = 0x1000,       /* ignored: merge already sorted files; do not sort */
	FLAG_S  = 0x2000,       /* ignored: -S, --buffer-size=SIZE */
	FLAG_T  = 0x4000,       /* ignored: -T, --temporary-directory=DIR */
	FLAG_o  = 0x8000,
	FLAG_k  = 0x10000,
	FLAG_t  = 0x20000,
	FLAG_bb = 0x80000000,   /* Ignore trailing blanks  */
};

#if ENABLE_FEATURE_SORT_BIG
static char key_separator;

static struct sort_key {
	struct sort_key *next_key;  /* linked list */
	unsigned range[4];          /* start word, start char, end word, end char */
	unsigned flags;
} *key_list;

static char *get_key(char *str, struct sort_key *key, int flags)
{
	int start = 0, end = 0, len, j;
	unsigned i;

	/* Special case whole string, so we don't have to make a copy */
	if (key->range[0] == 1 && !key->range[1] && !key->range[2] && !key->range[3]
	 && !(flags & (FLAG_b | FLAG_d | FLAG_f | FLAG_i | FLAG_bb))
	) {
		return str;
	}

	/* Find start of key on first pass, end on second pass */
	len = strlen(str);
	for (j = 0; j < 2; j++) {
		if (!key->range[2*j])
			end = len;
		/* Loop through fields */
		else {
			end = 0;
			for (i = 1; i < key->range[2*j] + j; i++) {
				if (key_separator) {
					/* Skip body of key and separator */
					while (str[end]) {
						if (str[end++] == key_separator)
							break;
					}
				} else {
					/* Skip leading blanks */
					while (isspace(str[end]))
						end++;
					/* Skip body of key */
					while (str[end]) {
						if (isspace(str[end]))
							break;
						end++;
					}
				}
			}
		}
		if (!j) start = end;
	}
	/* Strip leading whitespace if necessary */
//XXX: skip_whitespace()
	if (flags & FLAG_b)
		while (isspace(str[start])) start++;
	/* Strip trailing whitespace if necessary */
	if (flags & FLAG_bb)
		while (end > start && isspace(str[end-1])) end--;
	/* Handle offsets on start and end */
	if (key->range[3]) {
		end += key->range[3] - 1;
		if (end > len) end = len;
	}
	if (key->range[1]) {
		start += key->range[1] - 1;
		if (start > len) start = len;
	}
	/* Make the copy */
	if (end < start) end = start;
	str = xstrndup(str+start, end-start);
	/* Handle -d */
	if (flags & FLAG_d) {
		for (start = end = 0; str[end]; end++)
			if (isspace(str[end]) || isalnum(str[end]))
				str[start++] = str[end];
		str[start] = '\0';
	}
	/* Handle -i */
	if (flags & FLAG_i) {
		for (start = end = 0; str[end]; end++)
			if (isprint_asciionly(str[end]))
				str[start++] = str[end];
		str[start] = '\0';
	}
	/* Handle -f */
	if (flags & FLAG_f)
		for (i = 0; str[i]; i++)
			str[i] = toupper(str[i]);

	return str;
}

static struct sort_key *add_key(void)
{
	struct sort_key **pkey = &key_list;
	while (*pkey)
		pkey = &((*pkey)->next_key);
	return *pkey = xzalloc(sizeof(struct sort_key));
}

#define GET_LINE(fp) \
	((option_mask32 & FLAG_z) \
	? bb_get_chunk_from_file(fp, NULL) \
	: xmalloc_fgetline(fp))
#else
#define GET_LINE(fp) xmalloc_fgetline(fp)
#endif

/* Iterate through keys list and perform comparisons */
static int compare_keys(const void *xarg, const void *yarg)
{
	int flags = option_mask32, retval = 0;
	char *x, *y;

#if ENABLE_FEATURE_SORT_BIG
	struct sort_key *key;

	for (key = key_list; !retval && key; key = key->next_key) {
		flags = key->flags ? key->flags : option_mask32;
		/* Chop out and modify key chunks, handling -dfib */
		x = get_key(*(char **)xarg, key, flags);
		y = get_key(*(char **)yarg, key, flags);
#else
	/* This curly bracket serves no purpose but to match the nesting
	   level of the for () loop we're not using */
	{
		x = *(char **)xarg;
		y = *(char **)yarg;
#endif
		/* Perform actual comparison */
		switch (flags & 7) {
		default:
			bb_error_msg_and_die("unknown sort type");
			break;
		/* Ascii sort */
		case 0:
#if ENABLE_LOCALE_SUPPORT
			retval = strcoll(x, y);
#else
			retval = strcmp(x, y);
#endif
			break;
#if ENABLE_FEATURE_SORT_BIG
		case FLAG_g: {
			char *xx, *yy;
			double dx = strtod(x, &xx);
			double dy = strtod(y, &yy);
			/* not numbers < NaN < -infinity < numbers < +infinity) */
			if (x == xx)
				retval = (y == yy ? 0 : -1);
			else if (y == yy)
				retval = 1;
			/* Check for isnan */
			else if (dx != dx)
				retval = (dy != dy) ? 0 : -1;
			else if (dy != dy)
				retval = 1;
			/* Check for infinity.  Could underflow, but it avoids libm. */
			else if (1.0 / dx == 0.0) {
				if (dx < 0)
					retval = (1.0 / dy == 0.0 && dy < 0) ? 0 : -1;
				else
					retval = (1.0 / dy == 0.0 && dy > 0) ? 0 : 1;
			} else if (1.0 / dy == 0.0)
				retval = (dy < 0) ? 1 : -1;
			else
				retval = (dx > dy) ? 1 : ((dx < dy) ? -1 : 0);
			break;
		}
		case FLAG_M: {
			struct tm thyme;
			int dx;
			char *xx, *yy;

			xx = strptime(x, "%b", &thyme);
			dx = thyme.tm_mon;
			yy = strptime(y, "%b", &thyme);
			if (!xx)
				retval = (!yy) ? 0 : -1;
			else if (!yy)
				retval = 1;
			else
				retval = (dx == thyme.tm_mon) ? 0 : dx - thyme.tm_mon;
			break;
		}
		/* Full floating point version of -n */
		case FLAG_n: {
			double dx = atof(x);
			double dy = atof(y);
			retval = (dx > dy) ? 1 : ((dx < dy) ? -1 : 0);
			break;
		}
		} /* switch */
		/* Free key copies. */
		if (x != *(char **)xarg) free(x);
		if (y != *(char **)yarg) free(y);
		/* if (retval) break; - done by for () anyway */
#else
		/* Integer version of -n for tiny systems */
		case FLAG_n:
			retval = atoi(x) - atoi(y);
			break;
		} /* switch */
#endif
	} /* for */

	/* Perform fallback sort if necessary */
	if (!retval && !(option_mask32 & FLAG_s))
		retval = strcmp(*(char **)xarg, *(char **)yarg);

	if (flags & FLAG_r) return -retval;
	return retval;
}

#if ENABLE_FEATURE_SORT_BIG
static unsigned str2u(char **str)
{
	unsigned long lu;
	if (!isdigit((*str)[0]))
		bb_error_msg_and_die("bad field specification");
	lu = strtoul(*str, str, 10);
	if ((sizeof(long) > sizeof(int) && lu > INT_MAX) || !lu)
		bb_error_msg_and_die("bad field specification");
	return lu;
}
#endif

int sort_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sort_main(int argc UNUSED_PARAM, char **argv)
{
	char *line, **lines;
	char *str_ignored, *str_o, *str_t;
	llist_t *lst_k = NULL;
	int i, flag;
	int linecount;
	unsigned opts;

	xfunc_error_retval = 2;

	/* Parse command line options */
	/* -o and -t can be given at most once */
	opt_complementary = "o--o:t--t:" /* -t, -o: at most one of each */
			"k::"; /* -k takes list */
	opts = getopt32(argv, OPT_STR, &str_ignored, &str_ignored, &str_o, &lst_k, &str_t);
	/* global b strips leading and trailing spaces */
	if (opts & FLAG_b)
		option_mask32 |= FLAG_bb;
#if ENABLE_FEATURE_SORT_BIG
	if (opts & FLAG_t) {
		if (!str_t[0] || str_t[1])
			bb_error_msg_and_die("bad -t parameter");
		key_separator = str_t[0];
	}
	/* note: below this point we use option_mask32, not opts,
	 * since that reduces register pressure and makes code smaller */

	/* parse sort key */
	while (lst_k) {
		enum {
			FLAG_allowed_for_k =
				FLAG_n | /* Numeric sort */
				FLAG_g | /* Sort using strtod() */
				FLAG_M | /* Sort date */
				FLAG_b | /* Ignore leading blanks */
				FLAG_r | /* Reverse */
				FLAG_d | /* Ignore !(isalnum()|isspace()) */
				FLAG_f | /* Force uppercase */
				FLAG_i | /* Ignore !isprint() */
			0
		};
		struct sort_key *key = add_key();
		char *str_k = llist_pop(&lst_k);

		i = 0; /* i==0 before comma, 1 after (-k3,6) */
		while (*str_k) {
			/* Start of range */
			/* Cannot use bb_strtou - suffix can be a letter */
			key->range[2*i] = str2u(&str_k);
			if (*str_k == '.') {
				str_k++;
				key->range[2*i+1] = str2u(&str_k);
			}
			while (*str_k) {
				const char *temp2;

				if (*str_k == ',' && !i++) {
					str_k++;
					break;
				} /* no else needed: fall through to syntax error
					because comma isn't in OPT_STR */
				temp2 = strchr(OPT_STR, *str_k);
				if (!temp2)
					bb_error_msg_and_die("unknown key option");
				flag = 1 << (temp2 - OPT_STR);
				if (flag & ~FLAG_allowed_for_k)
					bb_error_msg_and_die("unknown sort type");
				/* b after ',' means strip _trailing_ space */
				if (i && flag == FLAG_b)
					flag = FLAG_bb;
				key->flags |= flag;
				str_k++;
			}
		}
	}
#endif

	/* Open input files and read data */
	argv += optind;
	if (!*argv)
		*--argv = (char*)"-";
	linecount = 0;
	lines = NULL;
	do {
		/* coreutils 6.9 compat: abort on first open error,
		 * do not continue to next file: */
		FILE *fp = xfopen_stdin(*argv);
		for (;;) {
			line = GET_LINE(fp);
			if (!line)
				break;
			lines = xrealloc_vector(lines, 6, linecount);
			lines[linecount++] = line;
		}
		fclose_if_not_stdin(fp);
	} while (*++argv);

#if ENABLE_FEATURE_SORT_BIG
	/* if no key, perform alphabetic sort */
	if (!key_list)
		add_key()->range[0] = 1;
	/* handle -c */
	if (option_mask32 & FLAG_c) {
		int j = (option_mask32 & FLAG_u) ? -1 : 0;
		for (i = 1; i < linecount; i++) {
			if (compare_keys(&lines[i-1], &lines[i]) > j) {
				fprintf(stderr, "Check line %u\n", i);
				return EXIT_FAILURE;
			}
		}
		return EXIT_SUCCESS;
	}
#endif
	/* Perform the actual sort */
	qsort(lines, linecount, sizeof(lines[0]), compare_keys);
	/* handle -u */
	if (option_mask32 & FLAG_u) {
		flag = 0;
		/* coreutils 6.3 drop lines for which only key is the same */
		/* -- disabling last-resort compare... */
		option_mask32 |= FLAG_s;
		for (i = 1; i < linecount; i++) {
			if (compare_keys(&lines[flag], &lines[i]) == 0)
				free(lines[i]);
			else
				lines[++flag] = lines[i];
		}
		if (linecount)
			linecount = flag+1;
	}

	/* Print it */
#if ENABLE_FEATURE_SORT_BIG
	/* Open output file _after_ we read all input ones */
	if (option_mask32 & FLAG_o)
		xmove_fd(xopen(str_o, O_WRONLY|O_CREAT|O_TRUNC), STDOUT_FILENO);
#endif
	flag = (option_mask32 & FLAG_z) ? '\0' : '\n';
	for (i = 0; i < linecount; i++)
		printf("%s%c", lines[i], flag);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
