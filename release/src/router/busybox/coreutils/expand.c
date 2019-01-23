/* expand - convert tabs to spaces
 * unexpand - convert spaces to tabs
 *
 * Copyright (C) 89, 91, 1995-2006 Free Software Foundation, Inc.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * David MacKenzie <djm@gnu.ai.mit.edu>
 *
 * Options for expand:
 * -t num  --tabs=NUM      Convert tabs to num spaces (default 8 spaces).
 * -i      --initial       Only convert initial tabs on each line to spaces.
 *
 * Options for unexpand:
 * -a      --all           Convert all blanks, instead of just initial blanks.
 * -f      --first-only    Convert only leading sequences of blanks (default).
 * -t num  --tabs=NUM      Have tabs num characters apart instead of 8.
 *
 *  Busybox version (C) 2007 by Tito Ragusa <farmatito@tiscali.it>
 *
 *  Caveat: this versions of expand and unexpand don't accept tab lists.
 */

//usage:#define expand_trivial_usage
//usage:       "[-i] [-t N] [FILE]..."
//usage:#define expand_full_usage "\n\n"
//usage:       "Convert tabs to spaces, writing to stdout\n"
//usage:	IF_FEATURE_EXPAND_LONG_OPTIONS(
//usage:     "\n	-i,--initial	Don't convert tabs after non blanks"
//usage:     "\n	-t,--tabs=N	Tabstops every N chars"
//usage:	)
//usage:	IF_NOT_FEATURE_EXPAND_LONG_OPTIONS(
//usage:     "\n	-i	Don't convert tabs after non blanks"
//usage:     "\n	-t	Tabstops every N chars"
//usage:	)

//usage:#define unexpand_trivial_usage
//usage:       "[-fa][-t N] [FILE]..."
//usage:#define unexpand_full_usage "\n\n"
//usage:       "Convert spaces to tabs, writing to stdout\n"
//usage:	IF_FEATURE_UNEXPAND_LONG_OPTIONS(
//usage:     "\n	-a,--all	Convert all blanks"
//usage:     "\n	-f,--first-only	Convert only leading blanks"
//usage:     "\n	-t,--tabs=N	Tabstops every N chars"
//usage:	)
//usage:	IF_NOT_FEATURE_UNEXPAND_LONG_OPTIONS(
//usage:     "\n	-a	Convert all blanks"
//usage:     "\n	-f	Convert only leading blanks"
//usage:     "\n	-t N	Tabstops every N chars"
//usage:	)

#include "libbb.h"
#include "unicode.h"

enum {
	OPT_INITIAL     = 1 << 0,
	OPT_TABS        = 1 << 1,
	OPT_ALL         = 1 << 2,
};

#if ENABLE_EXPAND
static void expand(FILE *file, unsigned tab_size, unsigned opt)
{
	char *line;

	while ((line = xmalloc_fgets(file)) != NULL) {
		unsigned char c;
		char *ptr;
		char *ptr_strbeg;

		ptr = ptr_strbeg = line;
		while ((c = *ptr) != '\0') {
			if ((opt & OPT_INITIAL) && !isblank(c)) {
				/* not space or tab */
				break;
			}
			if (c == '\t') {
				unsigned len;
				*ptr = '\0';
# if ENABLE_UNICODE_SUPPORT
				len = unicode_strwidth(ptr_strbeg);
# else
				len = ptr - ptr_strbeg;
# endif
				len = tab_size - (len % tab_size);
				/*while (ptr[1] == '\t') { ptr++; len += tab_size; } - can handle many tabs at once */
				printf("%s%*s", ptr_strbeg, len, "");
				ptr_strbeg = ptr + 1;
			}
			ptr++;
		}
		fputs(ptr_strbeg, stdout);
		free(line);
	}
}
#endif

#if ENABLE_UNEXPAND
static void unexpand(FILE *file, unsigned tab_size, unsigned opt)
{
	char *line;

	while ((line = xmalloc_fgets(file)) != NULL) {
		char *ptr = line;
		unsigned column = 0;

		while (*ptr) {
			unsigned n;
			unsigned len = 0;

			while (*ptr == ' ') {
				ptr++;
				len++;
			}
			column += len;
			if (*ptr == '\t') {
				column += tab_size - (column % tab_size);
				ptr++;
				continue;
			}

			n = column / tab_size;
			if (n) {
				len = column = column % tab_size;
				while (n--)
					putchar('\t');
			}

			if ((opt & OPT_INITIAL) && ptr != line) {
				printf("%*s%s", len, "", ptr);
				break;
			}
			n = strcspn(ptr, "\t ");
			printf("%*s%.*s", len, "", n, ptr);
# if ENABLE_UNICODE_SUPPORT
			{
				char c = ptr[n];
				ptr[n] = '\0';
				len = unicode_strwidth(ptr);
				ptr[n] = c;
			}
# else
			len = n;
# endif
			ptr += n;
			column = (column + len) % tab_size;
		}
		free(line);
	}
}
#endif

int expand_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int expand_main(int argc UNUSED_PARAM, char **argv)
{
	/* Default 8 spaces for 1 tab */
	const char *opt_t = "8";
	FILE *file;
	unsigned tab_size;
	unsigned opt;
	int exit_status = EXIT_SUCCESS;

#if ENABLE_FEATURE_EXPAND_LONG_OPTIONS
	static const char expand_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"initial\0"          No_argument       "i"
		"tabs\0"             Required_argument "t"
	;
#endif
#if ENABLE_FEATURE_UNEXPAND_LONG_OPTIONS
	static const char unexpand_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"first-only\0"       No_argument       "i"
		"tabs\0"             Required_argument "t"
		"all\0"              No_argument       "a"
	;
#endif
	init_unicode();

	if (ENABLE_EXPAND && (!ENABLE_UNEXPAND || applet_name[0] == 'e')) {
		IF_FEATURE_EXPAND_LONG_OPTIONS(applet_long_options = expand_longopts);
		opt = getopt32(argv, "it:", &opt_t);
	} else {
		IF_FEATURE_UNEXPAND_LONG_OPTIONS(applet_long_options = unexpand_longopts);
		/* -t NUM sets also -a */
		opt_complementary = "ta";
		opt = getopt32(argv, "ft:a", &opt_t);
		/* -f --first-only is the default */
		if (!(opt & OPT_ALL)) opt |= OPT_INITIAL;
	}
	tab_size = xatou_range(opt_t, 1, UINT_MAX);

	argv += optind;

	if (!*argv) {
		*--argv = (char*)bb_msg_standard_input;
	}
	do {
		file = fopen_or_warn_stdin(*argv);
		if (!file) {
			exit_status = EXIT_FAILURE;
			continue;
		}

		if (ENABLE_EXPAND && (!ENABLE_UNEXPAND || applet_name[0] == 'e'))
			IF_EXPAND(expand(file, tab_size, opt));
		else
			IF_UNEXPAND(unexpand(file, tab_size, opt));

		/* Check and close the file */
		if (fclose_if_not_stdin(file)) {
			bb_simple_perror_msg(*argv);
			exit_status = EXIT_FAILURE;
		}
		/* If stdin also clear EOF */
		if (file == stdin)
			clearerr(file);
	} while (*++argv);

	/* Now close stdin also */
	/* (if we didn't read from it, it's a no-op) */
	if (fclose(stdin))
		bb_perror_msg_and_die(bb_msg_standard_input);

	fflush_stdout_and_exit(exit_status);
}
