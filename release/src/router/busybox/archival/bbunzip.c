/* vi: set sw=4 ts=4: */
/*
 * Common code for gunzip-like applets
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "bb_archive.h"

enum {
	OPT_STDOUT     = 1 << 0,
	OPT_FORCE      = 1 << 1,
	/* only some decompressors: */
	OPT_VERBOSE    = 1 << 2,
	OPT_DECOMPRESS = 1 << 3,
	OPT_TEST       = 1 << 4,
};

static
int open_to_or_warn(int to_fd, const char *filename, int flags, int mode)
{
	int fd = open3_or_warn(filename, flags, mode);
	if (fd < 0) {
		return 1;
	}
	xmove_fd(fd, to_fd);
	return 0;
}

char* FAST_FUNC append_ext(char *filename, const char *expected_ext)
{
	return xasprintf("%s.%s", filename, expected_ext);
}

int FAST_FUNC bbunpack(char **argv,
	IF_DESKTOP(long long) int FAST_FUNC (*unpacker)(transformer_aux_data_t *aux),
	char* FAST_FUNC (*make_new_name)(char *filename, const char *expected_ext),
	const char *expected_ext
)
{
	struct stat stat_buf;
	IF_DESKTOP(long long) int status;
	char *filename, *new_name;
	smallint exitcode = 0;
	transformer_aux_data_t aux;

	do {
		/* NB: new_name is *maybe* malloc'ed! */
		new_name = NULL;
		filename = *argv; /* can be NULL - 'streaming' bunzip2 */

		if (filename && LONE_DASH(filename))
			filename = NULL;

		/* Open src */
		if (filename) {
			if (stat(filename, &stat_buf) != 0) {
				bb_simple_perror_msg(filename);
 err:
				exitcode = 1;
				goto free_name;
			}
			if (open_to_or_warn(STDIN_FILENO, filename, O_RDONLY, 0))
				goto err;
		}

		/* Special cases: test, stdout */
		if (option_mask32 & (OPT_STDOUT|OPT_TEST)) {
			if (option_mask32 & OPT_TEST)
				if (open_to_or_warn(STDOUT_FILENO, bb_dev_null, O_WRONLY, 0))
					goto err;
			filename = NULL;
		}

		/* Open dst if we are going to unpack to file */
		if (filename) {
			new_name = make_new_name(filename, expected_ext);
			if (!new_name) {
				bb_error_msg("%s: unknown suffix - ignored", filename);
				goto err;
			}

			/* -f: overwrite existing output files */
			if (option_mask32 & OPT_FORCE) {
				unlink(new_name);
			}

			/* O_EXCL: "real" bunzip2 doesn't overwrite files */
			/* GNU gunzip does not bail out, but goes to next file */
			if (open_to_or_warn(STDOUT_FILENO, new_name, O_WRONLY | O_CREAT | O_EXCL,
					stat_buf.st_mode))
				goto err;
		}

		/* Check that the input is sane */
		if (isatty(STDIN_FILENO) && (option_mask32 & OPT_FORCE) == 0) {
			bb_error_msg_and_die("compressed data not read from terminal, "
					"use -f to force it");
		}

		init_transformer_aux_data(&aux);
		aux.check_signature = 1;
		status = unpacker(&aux);
		if (status < 0)
			exitcode = 1;

		if (!(option_mask32 & OPT_STDOUT))
			xclose(STDOUT_FILENO); /* with error check! */

		if (filename) {
			char *del = new_name;
			if (status >= 0) {
				/* TODO: restore other things? */
				if (aux.mtime != 0) {
					struct timeval times[2];

					times[1].tv_sec = times[0].tv_sec = aux.mtime;
					times[1].tv_usec = times[0].tv_usec = 0;
					/* Note: we closed it first.
					 * On some systems calling utimes
					 * then closing resets the mtime
					 * back to current time. */
					utimes(new_name, times); /* ignoring errors */
				}

				/* Delete _compressed_ file */
				del = filename;
				/* restore extension (unless tgz -> tar case) */
				if (new_name == filename)
					filename[strlen(filename)] = '.';
			}
			xunlink(del);

#if 0 /* Currently buggy - wrong name: "a.gz: 261% - replaced with a.gz" */
			/* Extreme bloat for gunzip compat */
			if (ENABLE_DESKTOP && (option_mask32 & OPT_VERBOSE) && status >= 0) {
				fprintf(stderr, "%s: %u%% - replaced with %s\n",
					filename, (unsigned)(stat_buf.st_size*100 / (status+1)), new_name);
			}
#endif

 free_name:
			if (new_name != filename)
				free(new_name);
		}
	} while (*argv && *++argv);

	if (option_mask32 & OPT_STDOUT)
		xclose(STDOUT_FILENO); /* with error check! */

	return exitcode;
}

#if ENABLE_UNCOMPRESS || ENABLE_BUNZIP2 || ENABLE_UNLZMA || ENABLE_UNXZ
static
char* FAST_FUNC make_new_name_generic(char *filename, const char *expected_ext)
{
	char *extension = strrchr(filename, '.');
	if (!extension || strcmp(extension + 1, expected_ext) != 0) {
		/* Mimic GNU gunzip - "real" bunzip2 tries to */
		/* unpack file anyway, to file.out */
		return NULL;
	}
	*extension = '\0';
	return filename;
}
#endif


/*
 * Uncompress applet for busybox (c) 2002 Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define uncompress_trivial_usage
//usage:       "[-cf] [FILE]..."
//usage:#define uncompress_full_usage "\n\n"
//usage:       "Decompress .Z file[s]\n"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Overwrite"

#if ENABLE_UNCOMPRESS
static
IF_DESKTOP(long long) int FAST_FUNC unpack_uncompress(transformer_aux_data_t *aux)
{
	return unpack_Z_stream(aux, STDIN_FILENO, STDOUT_FILENO);
}
int uncompress_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uncompress_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "cf");
	argv += optind;

	return bbunpack(argv, unpack_uncompress, make_new_name_generic, "Z");
}
#endif


/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersen@codepoet.org> to support files as
 * well as stdin/stdout, and to generally behave itself wrt command line
 * handling.
 *
 * General cleanup to better adhere to the style guide and make use of standard
 * busybox functions by Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * gzip (GNU zip) -- compress files with zip algorithm and 'compress' interface
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * The unzip code was written and put in the public domain by Mark Adler.
 * Portions of the lzw code are derived from the public domain 'compress'
 * written by Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
 *
 * See the license_msg below and the file COPYING for the software license.
 * See the file algorithm.doc for the compression algorithms and file formats.
 */

//usage:#define gunzip_trivial_usage
//usage:       "[-cft] [FILE]..."
//usage:#define gunzip_full_usage "\n\n"
//usage:       "Decompress FILEs (or stdin)\n"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Force"
//usage:     "\n	-t	Test file integrity"
//usage:
//usage:#define gunzip_example_usage
//usage:       "$ ls -la /tmp/BusyBox*\n"
//usage:       "-rw-rw-r--    1 andersen andersen   557009 Apr 11 10:55 /tmp/BusyBox-0.43.tar.gz\n"
//usage:       "$ gunzip /tmp/BusyBox-0.43.tar.gz\n"
//usage:       "$ ls -la /tmp/BusyBox*\n"
//usage:       "-rw-rw-r--    1 andersen andersen  1761280 Apr 14 17:47 /tmp/BusyBox-0.43.tar\n"
//usage:
//usage:#define zcat_trivial_usage
//usage:       "FILE"
//usage:#define zcat_full_usage "\n\n"
//usage:       "Decompress to stdout"

#if ENABLE_GUNZIP
static
char* FAST_FUNC make_new_name_gunzip(char *filename, const char *expected_ext UNUSED_PARAM)
{
	char *extension = strrchr(filename, '.');

	if (!extension)
		return NULL;

	extension++;
	if (strcmp(extension, "tgz" + 1) == 0
#if ENABLE_FEATURE_SEAMLESS_Z
	 || (extension[0] == 'Z' && extension[1] == '\0')
#endif
	) {
		extension[-1] = '\0';
	} else if (strcmp(extension, "tgz") == 0) {
		filename = xstrdup(filename);
		extension = strrchr(filename, '.');
		extension[2] = 'a';
		extension[3] = 'r';
	} else {
		return NULL;
	}
	return filename;
}
static
IF_DESKTOP(long long) int FAST_FUNC unpack_gunzip(transformer_aux_data_t *aux)
{
	return unpack_gz_stream(aux, STDIN_FILENO, STDOUT_FILENO);
}
/*
 * Linux kernel build uses gzip -d -n. We accept and ignore it.
 * Man page says:
 * -n --no-name
 * gzip: do not save the original file name and time stamp.
 * (The original name is always saved if the name had to be truncated.)
 * gunzip: do not restore the original file name/time even if present
 * (remove only the gzip suffix from the compressed file name).
 * This option is the default when decompressing.
 * -N --name
 * gzip: always save the original file name and time stamp (this is the default)
 * gunzip: restore the original file name and time stamp if present.
 */
int gunzip_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int gunzip_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "cfvdtn");
	argv += optind;
	/* if called as zcat */
	if (applet_name[1] == 'c')
		option_mask32 |= OPT_STDOUT;

	return bbunpack(argv, unpack_gunzip, make_new_name_gunzip, /*unused:*/ NULL);
}
#endif


/*
 * Modified for busybox by Glenn McGrath
 * Added support output to stdout by Thomas Lundquist <thomasez@zelow.no>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//usage:#define bunzip2_trivial_usage
//usage:       "[-cf] [FILE]..."
//usage:#define bunzip2_full_usage "\n\n"
//usage:       "Decompress FILEs (or stdin)\n"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Force"
//usage:#define bzcat_trivial_usage
//usage:       "FILE"
//usage:#define bzcat_full_usage "\n\n"
//usage:       "Decompress to stdout"
//applet:IF_BUNZIP2(APPLET(bunzip2, BB_DIR_USR_BIN, BB_SUID_DROP))
//applet:IF_BUNZIP2(APPLET_ODDNAME(bzcat, bunzip2, BB_DIR_USR_BIN, BB_SUID_DROP, bzcat))
#if ENABLE_BUNZIP2
static
IF_DESKTOP(long long) int FAST_FUNC unpack_bunzip2(transformer_aux_data_t *aux)
{
	return unpack_bz2_stream(aux, STDIN_FILENO, STDOUT_FILENO);
}
int bunzip2_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bunzip2_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "cfvdt");
	argv += optind;
	if (applet_name[2] == 'c') /* bzcat */
		option_mask32 |= OPT_STDOUT;

	return bbunpack(argv, unpack_bunzip2, make_new_name_generic, "bz2");
}
#endif


/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Based on bunzip.c from busybox
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//usage:#define unlzma_trivial_usage
//usage:       "[-cf] [FILE]..."
//usage:#define unlzma_full_usage "\n\n"
//usage:       "Decompress FILE (or stdin)\n"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Force"
//usage:
//usage:#define lzma_trivial_usage
//usage:       "-d [-cf] [FILE]..."
//usage:#define lzma_full_usage "\n\n"
//usage:       "Decompress FILE (or stdin)\n"
//usage:     "\n	-d	Decompress"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Force"
//usage:
//usage:#define lzcat_trivial_usage
//usage:       "FILE"
//usage:#define lzcat_full_usage "\n\n"
//usage:       "Decompress to stdout"
//usage:
//usage:#define unxz_trivial_usage
//usage:       "[-cf] [FILE]..."
//usage:#define unxz_full_usage "\n\n"
//usage:       "Decompress FILE (or stdin)\n"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Force"
//usage:
//usage:#define xz_trivial_usage
//usage:       "-d [-cf] [FILE]..."
//usage:#define xz_full_usage "\n\n"
//usage:       "Decompress FILE (or stdin)\n"
//usage:     "\n	-d	Decompress"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Force"
//usage:
//usage:#define xzcat_trivial_usage
//usage:       "FILE"
//usage:#define xzcat_full_usage "\n\n"
//usage:       "Decompress to stdout"

#if ENABLE_UNLZMA
static
IF_DESKTOP(long long) int FAST_FUNC unpack_unlzma(transformer_aux_data_t *aux)
{
	return unpack_lzma_stream(aux, STDIN_FILENO, STDOUT_FILENO);
}
int unlzma_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int unlzma_main(int argc UNUSED_PARAM, char **argv)
{
	IF_LZMA(int opts =) getopt32(argv, "cfvdt");
# if ENABLE_LZMA
	/* lzma without -d or -t? */
	if (applet_name[2] == 'm' && !(opts & (OPT_DECOMPRESS|OPT_TEST)))
		bb_show_usage();
# endif
	/* lzcat? */
	if (applet_name[2] == 'c')
		option_mask32 |= OPT_STDOUT;

	argv += optind;
	return bbunpack(argv, unpack_unlzma, make_new_name_generic, "lzma");
}
#endif


#if ENABLE_UNXZ
static
IF_DESKTOP(long long) int FAST_FUNC unpack_unxz(transformer_aux_data_t *aux)
{
	return unpack_xz_stream(aux, STDIN_FILENO, STDOUT_FILENO);
}
int unxz_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int unxz_main(int argc UNUSED_PARAM, char **argv)
{
	IF_XZ(int opts =) getopt32(argv, "cfvdt");
# if ENABLE_XZ
	/* xz without -d or -t? */
	if (applet_name[2] == '\0' && !(opts & (OPT_DECOMPRESS|OPT_TEST)))
		bb_show_usage();
# endif
	/* xzcat? */
	if (applet_name[2] == 'c')
		option_mask32 |= OPT_STDOUT;

	argv += optind;
	return bbunpack(argv, unpack_unxz, make_new_name_generic, "xz");
}
#endif
