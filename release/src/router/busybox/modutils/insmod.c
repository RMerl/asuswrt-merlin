/* vi: set sw=4 ts=4: */
/*
 * Mini insmod implementation for busybox
 *
 * Copyright (C) 2008 Timo Teras <timo.teras@iki.fi>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//applet:IF_INSMOD(APPLET(insmod, BB_DIR_SBIN, BB_SUID_DROP))

#include "libbb.h"
#include "common_bufsiz.h"
#include "modutils.h"

/* 2.6 style insmod has no options and required filename
 * (not module name - .ko can't be omitted) */

//usage:#if !ENABLE_MODPROBE_SMALL
//usage:#define insmod_trivial_usage
//usage:	IF_FEATURE_2_4_MODULES("[OPTIONS] MODULE ")
//usage:	IF_NOT_FEATURE_2_4_MODULES("FILE ")
//usage:	"[SYMBOL=VALUE]..."
//usage:#define insmod_full_usage "\n\n"
//usage:       "Load kernel module"
//usage:	IF_FEATURE_2_4_MODULES( "\n"
//usage:     "\n	-f	Force module to load into the wrong kernel version"
//usage:     "\n	-k	Make module autoclean-able"
//usage:     "\n	-v	Verbose"
//usage:     "\n	-q	Quiet"
//usage:     "\n	-L	Lock: prevent simultaneous loads"
//usage:	IF_FEATURE_INSMOD_LOAD_MAP(
//usage:     "\n	-m	Output load map to stdout"
//usage:	)
//usage:     "\n	-x	Don't export externs"
//usage:	)
//usage:#endif

struct globals {
	char *filename;
} FIX_ALIASING;
#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { \
	setup_common_bufsiz(); \
} while (0)

static int FAST_FUNC check_module_name_match(const char *filename,
		struct stat *statbuf UNUSED_PARAM,
		void *userdata, int depth UNUSED_PARAM)
{
	char *fullname = (char *) userdata;
	char *tmp;

	if (fullname[0] == '\0')
		return FALSE;

	tmp = bb_get_last_path_component_nostrip(filename);
	if (strcmp(tmp, fullname) == 0) {
		/* Stop searching if we find a match */
		G.filename = xstrdup(filename);
		return FALSE;
	}
	return TRUE;
}

int insmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int insmod_main(int argc UNUSED_PARAM, char **argv)
{
	char *filename, *dot;
	int rc;

	INIT_G();

	/* Compat note:
	 * 2.6 style insmod has no options and required filename
	 * (not module name - .ko can't be omitted).
	 * 2.4 style insmod can take module name without .o
	 * and performs module search in default directories
	 * or in $MODPATH.
	 */

	IF_FEATURE_2_4_MODULES(
		getopt32(argv, INSMOD_OPTS INSMOD_ARGS);
		argv += optind - 1;
	);

	filename = *++argv;
	if (!filename)
		bb_show_usage();

	/* Check if we have a complete module name */
	dot = strrchr(filename, '.');
	if (ENABLE_FEATURE_2_4_MODULES
	 && get_linux_version_code() < KERNEL_VERSION(2,6,0)) {
		if (!dot || strcmp(dot, ".o") != 0)
			filename = xasprintf("%s.o", filename);
	} else {
		if (!dot || strcmp(dot, ".ko") != 0)
			filename = xasprintf("%s.ko", filename);
	}

	/* Check if we have a complete path */
	if (access(filename, R_OK) != 0) {
		/* Hmm.  Could not open it. Search /lib/modules/ */
		char *module_dir = xmalloc_readlink(CONFIG_DEFAULT_MODULES_DIR);
		if (!module_dir)
			module_dir = xstrdup(CONFIG_DEFAULT_MODULES_DIR);
		rc = recursive_action(module_dir, ACTION_RECURSE,
				check_module_name_match, NULL, filename, 0);
		if (!rc && G.filename && access(G.filename, R_OK) == 0)
			filename = G.filename;
		free(module_dir);
	}

	rc = bb_init_module(filename, parse_cmdline_module_options(argv, /*quote_spaces:*/ 0));
	if (rc)
		bb_error_msg("can't insert '%s': %s", filename, moderror(rc));

	if (ENABLE_FEATURE_CLEAN_UP)
		free(G.filename);

	return rc;
}
