/* vi: set sw=4 ts=4: */
/*
 * Mini mv implementation for busybox
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 * SELinux support by Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction and improved error checking.
 */

#include "libbb.h"
#include "libcoreutils/coreutils.h"

//usage:#define mv_trivial_usage
//usage:       "[-fin] SOURCE DEST\n"
//usage:       "or: mv [-fin] SOURCE... DIRECTORY"
//usage:#define mv_full_usage "\n\n"
//usage:       "Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY\n"
//usage:     "\n	-f	Don't prompt before overwriting"
//usage:     "\n	-i	Interactive, prompt before overwrite"
//usage:     "\n	-n	Don't overwrite an existing file"
//usage:
//usage:#define mv_example_usage
//usage:       "$ mv /tmp/foo /bin/bar\n"

#if ENABLE_FEATURE_MV_LONG_OPTIONS
static const char mv_longopts[] ALIGN1 =
	"interactive\0" No_argument "i"
	"force\0"       No_argument "f"
	"no-clobber\0"  No_argument "n"
	;
#endif

#define OPT_FILEUTILS_FORCE       1
#define OPT_FILEUTILS_INTERACTIVE 2
#define OPT_FILEUTILS_NOCLOBBER   4

int mv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mv_main(int argc, char **argv)
{
	struct stat dest_stat;
	const char *last;
	const char *dest;
	unsigned flags;
	int dest_exists;
	int status = 0;
	int copy_flag = 0;

#if ENABLE_FEATURE_MV_LONG_OPTIONS
	applet_long_options = mv_longopts;
#endif
	/* Need at least two arguments.
	 * If more than one of -f, -i, -n is specified , only the final one
	 * takes effect (it unsets previous options). */
	opt_complementary = "-2:f-in:i-fn:n-fi";
	flags = getopt32(argv, "fin");
	argc -= optind;
	argv += optind;
	last = argv[argc - 1];

	if (argc == 2) {
		dest_exists = cp_mv_stat(last, &dest_stat);
		if (dest_exists < 0) {
			return EXIT_FAILURE;
		}

		if (!(dest_exists & 2)) { /* last is not a directory */
			dest = last;
			goto DO_MOVE;
		}
	}

	do {
		dest = concat_path_file(last, bb_get_last_path_component_strip(*argv));
		dest_exists = cp_mv_stat(dest, &dest_stat);
		if (dest_exists < 0) {
			goto RET_1;
		}

 DO_MOVE:
		if (dest_exists) {
			if (flags & OPT_FILEUTILS_NOCLOBBER)
				goto RET_0;
			if (!(flags & OPT_FILEUTILS_FORCE)
			 && ((access(dest, W_OK) < 0 && isatty(0))
			    || (flags & OPT_FILEUTILS_INTERACTIVE))
			) {
				if (fprintf(stderr, "mv: overwrite '%s'? ", dest) < 0) {
					goto RET_1;  /* Ouch! fprintf failed! */
				}
				if (!bb_ask_confirmation()) {
					goto RET_0;
				}
			}
		}

		if (rename(*argv, dest) < 0) {
			struct stat source_stat;
			int source_exists;

			if (errno != EXDEV
			 || (source_exists = cp_mv_stat2(*argv, &source_stat, lstat)) < 1
			) {
				bb_perror_msg("can't rename '%s'", *argv);
			} else {
				static const char fmt[] ALIGN1 =
					"can't overwrite %sdirectory with %sdirectory";

				if (dest_exists) {
					if (dest_exists == 3) {
						if (source_exists != 3) {
							bb_error_msg(fmt, "", "non-");
							goto RET_1;
						}
					} else {
						if (source_exists == 3) {
							bb_error_msg(fmt, "non-", "");
							goto RET_1;
						}
					}
					if (unlink(dest) < 0) {
						bb_perror_msg("can't remove '%s'", dest);
						goto RET_1;
					}
				}
				/* FILEUTILS_RECUR also prevents nasties like
				 * "read from device and write contents to dst"
				 * instead of "create same device node" */
				copy_flag = FILEUTILS_RECUR | FILEUTILS_PRESERVE_STATUS;
#if ENABLE_SELINUX
				copy_flag |= FILEUTILS_PRESERVE_SECURITY_CONTEXT;
#endif
				if ((copy_file(*argv, dest, copy_flag) >= 0)
				 && (remove_file(*argv, FILEUTILS_RECUR | FILEUTILS_FORCE) >= 0)
				) {
					goto RET_0;
				}
			}
 RET_1:
			status = 1;
		}
 RET_0:
		if (dest != last) {
			free((void *) dest);
		}
	} while (*++argv != last);

	return status;
}
