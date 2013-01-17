/* vi: set sw=4 ts=4: */
/*
 * Mini swapon/swapoff implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//usage:#define swapon_trivial_usage
//usage:       "[-a]" IF_FEATURE_SWAPON_PRI(" [-p PRI]") " [DEVICE]"
//usage:#define swapon_full_usage "\n\n"
//usage:       "Start swapping on DEVICE\n"
//usage:     "\n	-a	Start swapping on all swap devices"
//usage:	IF_FEATURE_SWAPON_PRI(
//usage:     "\n	-p PRI	Set swap device priority"
//usage:	)
//usage:
//usage:#define swapoff_trivial_usage
//usage:       "[-a] [DEVICE]"
//usage:#define swapoff_full_usage "\n\n"
//usage:       "Stop swapping on DEVICE\n"
//usage:     "\n	-a	Stop swapping on all swap devices"

#include "libbb.h"
#include <mntent.h>
#ifndef __BIONIC__
# include <sys/swap.h>
#endif

#if ENABLE_FEATURE_MOUNT_LABEL
# include "volume_id.h"
#else
# define resolve_mount_spec(fsname) ((void)0)
#endif

#ifndef MNTTYPE_SWAP
# define MNTTYPE_SWAP "swap"
#endif

#if ENABLE_FEATURE_SWAPON_PRI
struct globals {
	int flags;
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define g_flags (G.flags)
#else
#define g_flags 0
#endif
#define INIT_G() do { } while (0)

static int swap_enable_disable(char *device)
{
	int status;
	struct stat st;

	resolve_mount_spec(&device);
	xstat(device, &st);

#if ENABLE_DESKTOP
	/* test for holes */
	if (S_ISREG(st.st_mode))
		if (st.st_blocks * (off_t)512 < st.st_size)
			bb_error_msg("warning: swap file has holes");
#endif

	if (applet_name[5] == 'n')
		status = swapon(device, g_flags);
	else
		status = swapoff(device);

	if (status != 0) {
		bb_simple_perror_msg(device);
		return 1;
	}

	return 0;
}

static int do_em_all(void)
{
	struct mntent *m;
	FILE *f;
	int err;

	f = setmntent("/etc/fstab", "r");
	if (f == NULL)
		bb_perror_msg_and_die("/etc/fstab");

	err = 0;
	while ((m = getmntent(f)) != NULL) {
		if (strcmp(m->mnt_type, MNTTYPE_SWAP) == 0) {
			/* swapon -a should ignore entries with noauto,
			 * but swapoff -a should process them */
			if (applet_name[5] != 'n'
			 || hasmntopt(m, MNTOPT_NOAUTO) == NULL
			) {
				err += swap_enable_disable(m->mnt_fsname);
			}
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		endmntent(f);

	return err;
}

int swap_on_off_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int swap_on_off_main(int argc UNUSED_PARAM, char **argv)
{
	int ret;

	INIT_G();

#if !ENABLE_FEATURE_SWAPON_PRI
	ret = getopt32(argv, "a");
#else
	if (applet_name[5] == 'n')
		opt_complementary = "p+";
	ret = getopt32(argv, (applet_name[5] == 'n') ? "ap:" : "a", &g_flags);

	if (ret & 2) { // -p
		g_flags = SWAP_FLAG_PREFER |
			((g_flags & SWAP_FLAG_PRIO_MASK) << SWAP_FLAG_PRIO_SHIFT);
		ret &= 1;
	}
#endif

	if (ret /* & 1: not needed */) // -a
		return do_em_all();

	argv += optind;
	if (!*argv)
		bb_show_usage();

	/* ret = 0; redundant */
	do {
		ret += swap_enable_disable(*argv);
	} while (*++argv);

	return ret;
}
