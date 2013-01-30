/* vi: set sw=4 ts=4: */
/*
 * strings implementation for busybox
 *
 * Copyright 2003 Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#define WHOLE_FILE		1
#define PRINT_NAME		2
#define PRINT_OFFSET	4
#define SIZE			8

int strings_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int strings_main(int argc UNUSED_PARAM, char **argv)
{
	int n, c, status = EXIT_SUCCESS;
	unsigned count;
	off_t offset;
	FILE *file;
	char *string;
	const char *fmt = "%s: ";
	const char *n_arg = "4";

	getopt32(argv, "afon:", &n_arg);
	/* -a is our default behaviour */
	/*argc -= optind;*/
	argv += optind;

	n = xatou_range(n_arg, 1, INT_MAX);
	string = xzalloc(n + 1);
	n--;

	if (!*argv) {
		fmt = "{%s}: ";
		*--argv = (char *)bb_msg_standard_input;
	}

	do {
		file = fopen_or_warn_stdin(*argv);
		if (!file) {
			status = EXIT_FAILURE;
			continue;
		}
		offset = 0;
		count = 0;
		do {
			c = fgetc(file);
			if (isprint_asciionly(c) || c == '\t') {
				if (count > n) {
					bb_putchar(c);
				} else {
					string[count] = c;
					if (count == n) {
						if (option_mask32 & PRINT_NAME) {
							printf(fmt, *argv);
						}
						if (option_mask32 & PRINT_OFFSET) {
							printf("%7"OFF_FMT"o ", offset - n);
						}
						fputs(string, stdout);
					}
					count++;
				}
			} else {
				if (count > n) {
					bb_putchar('\n');
				}
				count = 0;
			}
			offset++;
		} while (c != EOF);
		fclose_if_not_stdin(file);
	} while (*++argv);

	if (ENABLE_FEATURE_CLEAN_UP)
		free(string);

	fflush_stdout_and_exit(status);
}
