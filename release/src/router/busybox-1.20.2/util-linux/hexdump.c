/* vi: set sw=4 ts=4: */
/*
 * hexdump implementation for busybox
 * Based on code from util-linux v 2.11l
 *
 * Copyright (c) 1989
 * The Regents of the University of California.  All rights reserved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define hexdump_trivial_usage
//usage:       "[-bcCdefnosvx" IF_FEATURE_HEXDUMP_REVERSE("R") "] [FILE]..."
//usage:#define hexdump_full_usage "\n\n"
//usage:       "Display FILEs (or stdin) in a user specified format\n"
//usage:     "\n	-b		One-byte octal display"
//usage:     "\n	-c		One-byte character display"
//usage:     "\n	-C		Canonical hex+ASCII, 16 bytes per line"
//usage:     "\n	-d		Two-byte decimal display"
//usage:     "\n	-e FORMAT_STRING"
//usage:     "\n	-f FORMAT_FILE"
//usage:     "\n	-n LENGTH	Interpret only LENGTH bytes of input"
//usage:     "\n	-o		Two-byte octal display"
//usage:     "\n	-s OFFSET	Skip OFFSET bytes"
//usage:     "\n	-v		Display all input data"
//usage:     "\n	-x		Two-byte hexadecimal display"
//usage:	IF_FEATURE_HEXDUMP_REVERSE(
//usage:     "\n	-R		Reverse of 'hexdump -Cv'")
//usage:
//usage:#define hd_trivial_usage
//usage:       "FILE..."
//usage:#define hd_full_usage "\n\n"
//usage:       "hd is an alias for hexdump -C"

#include "libbb.h"
#include "dump.h"

/* This is a NOEXEC applet. Be very careful! */

static void bb_dump_addfile(dumper_t *dumper, char *name)
{
	char *p;
	FILE *fp;
	char *buf;

	fp = xfopen_for_read(name);
	while ((buf = xmalloc_fgetline(fp)) != NULL) {
		p = skip_whitespace(buf);
		if (*p && (*p != '#')) {
			bb_dump_add(dumper, p);
		}
		free(buf);
	}
	fclose(fp);
}

static const char *const add_strings[] = {
	"\"%07.7_ax \" 16/1 \"%03o \" \"\\n\"",   /* b */
	"\"%07.7_ax \" 16/1 \"%3_c \" \"\\n\"",   /* c */
	"\"%07.7_ax \" 8/2 \"  %05u \" \"\\n\"",  /* d */
	"\"%07.7_ax \" 8/2 \" %06o \" \"\\n\"",   /* o */
	"\"%07.7_ax \" 8/2 \"   %04x \" \"\\n\"", /* x */
};

static const char add_first[] ALIGN1 = "\"%07.7_Ax\n\"";

static const char hexdump_opts[] ALIGN1 = "bcdoxCe:f:n:s:v" IF_FEATURE_HEXDUMP_REVERSE("R");

static const struct suffix_mult suffixes[] = {
	{ "b", 512 },
	{ "k", 1024 },
	{ "m", 1024*1024 },
	{ "", 0 }
};

int hexdump_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hexdump_main(int argc, char **argv)
{
	dumper_t *dumper = alloc_dumper();
	const char *p;
	int ch;
#if ENABLE_FEATURE_HEXDUMP_REVERSE
	FILE *fp;
	smallint rdump = 0;
#endif

	if (ENABLE_HD && !applet_name[2]) { /* we are "hd" */
		ch = 'C';
		goto hd_applet;
	}

	/* We cannot use getopt32: in hexdump options are cumulative.
	 * E.g. "hexdump -C -C file" should dump each line twice */
	while ((ch = getopt(argc, argv, hexdump_opts)) > 0) {
		p = strchr(hexdump_opts, ch);
		if (!p)
			bb_show_usage();
		if ((p - hexdump_opts) < 5) {
			bb_dump_add(dumper, add_first);
			bb_dump_add(dumper, add_strings[(int)(p - hexdump_opts)]);
		}
		/* Save a little bit of space below by omitting the 'else's. */
		if (ch == 'C') {
 hd_applet:
			bb_dump_add(dumper, "\"%08.8_Ax\n\"");
			bb_dump_add(dumper, "\"%08.8_ax  \" 8/1 \"%02x \" \"  \" 8/1 \"%02x \" ");
			bb_dump_add(dumper, "\"  |\" 16/1 \"%_p\" \"|\\n\"");
		}
		if (ch == 'e') {
			bb_dump_add(dumper, optarg);
		} /* else */
		if (ch == 'f') {
			bb_dump_addfile(dumper, optarg);
		} /* else */
		if (ch == 'n') {
			dumper->dump_length = xatoi_positive(optarg);
		} /* else */
		if (ch == 's') { /* compat: -s accepts hex numbers too */
			dumper->dump_skip = xstrtoul_range_sfx(optarg, /*base:*/ 0, /*lo:*/ 0, /*hi:*/ LONG_MAX, suffixes);
		} /* else */
		if (ch == 'v') {
			dumper->dump_vflag = ALL;
		}
#if ENABLE_FEATURE_HEXDUMP_REVERSE
		if (ch == 'R') {
			rdump = 1;
		}
#endif
	}

	if (!dumper->fshead) {
		bb_dump_add(dumper, add_first);
		bb_dump_add(dumper, "\"%07.7_ax \" 8/2 \"%04x \" \"\\n\"");
	}

	argv += optind;

#if !ENABLE_FEATURE_HEXDUMP_REVERSE
	return bb_dump_dump(dumper, argv);
#else
	if (!rdump) {
		return bb_dump_dump(dumper, argv);
	}

	/* -R: reverse of 'hexdump -Cv' */
	fp = stdin;
	if (!*argv) {
		argv--;
		goto jump_in;
	}

	do {
		char *buf;
		fp = xfopen_for_read(*argv);
 jump_in:
		while ((buf = xmalloc_fgetline(fp)) != NULL) {
			p = buf;
			while (1) {
				/* skip address or previous byte */
				while (isxdigit(*p)) p++;
				while (*p == ' ') p++;
				/* '|' char will break the line */
				if (!isxdigit(*p) || sscanf(p, "%x ", &ch) != 1)
					break;
				putchar(ch);
			}
			free(buf);
		}
		fclose(fp);
	} while (*++argv);

	fflush_stdout_and_exit(EXIT_SUCCESS);
#endif
}
