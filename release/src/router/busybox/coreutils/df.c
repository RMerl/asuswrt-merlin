/* vi: set sw=4 ts=4: */
/*
 * Mini df implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * based on original code by (I think) Bruce Perens <bruce@pixar.com>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* BB_AUDIT SUSv3 _NOT_ compliant -- option -t missing. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/df.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction.  Removed floating point dependency.  Added error checking
 * on output.  Output stats on 0-sized filesystems if specifically listed on
 * the command line.  Properly round *-blocks, Used, and Available quantities.
 *
 * Aug 28, 2008      Bernhard Reutner-Fischer
 *
 * Implement -P and -B; better coreutils compat; cleanup
 */

//usage:#define df_trivial_usage
//usage:	"[-Pk"
//usage:	IF_FEATURE_HUMAN_READABLE("mh")
//usage:	"T"
//usage:	IF_FEATURE_DF_FANCY("ai] [-B SIZE")
//usage:	"] [FILESYSTEM]..."
//usage:#define df_full_usage "\n\n"
//usage:       "Print filesystem usage statistics\n"
//usage:     "\n	-P	POSIX output format"
//usage:     "\n	-k	1024-byte blocks (default)"
//usage:	IF_FEATURE_HUMAN_READABLE(
//usage:     "\n	-m	1M-byte blocks"
//usage:     "\n	-h	Human readable (e.g. 1K 243M 2G)"
//usage:	)
//usage:     "\n	-T	Print filesystem type"
//usage:	IF_FEATURE_DF_FANCY(
//usage:     "\n	-a	Show all filesystems"
//usage:     "\n	-i	Inodes"
//usage:     "\n	-B SIZE	Blocksize"
//usage:	)
//usage:
//usage:#define df_example_usage
//usage:       "$ df\n"
//usage:       "Filesystem           1K-blocks      Used Available Use% Mounted on\n"
//usage:       "/dev/sda3              8690864   8553540    137324  98% /\n"
//usage:       "/dev/sda1                64216     36364     27852  57% /boot\n"
//usage:       "$ df /dev/sda3\n"
//usage:       "Filesystem           1K-blocks      Used Available Use% Mounted on\n"
//usage:       "/dev/sda3              8690864   8553540    137324  98% /\n"
//usage:       "$ POSIXLY_CORRECT=sure df /dev/sda3\n"
//usage:       "Filesystem         512B-blocks      Used Available Use% Mounted on\n"
//usage:       "/dev/sda3             17381728  17107080    274648  98% /\n"
//usage:       "$ POSIXLY_CORRECT=yep df -P /dev/sda3\n"
//usage:       "Filesystem          512-blocks      Used Available Capacity Mounted on\n"
//usage:       "/dev/sda3             17381728  17107080    274648      98% /\n"

#include <mntent.h>
#include <sys/vfs.h>
#include "libbb.h"
#include "unicode.h"

#if !ENABLE_FEATURE_HUMAN_READABLE
static unsigned long kscale(unsigned long b, unsigned long bs)
{
	return (b * (unsigned long long) bs + 1024/2) / 1024;
}
#endif

int df_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int df_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned long blocks_used;
	unsigned blocks_percent_used;
	unsigned long df_disp_hr = 1024;
	int status = EXIT_SUCCESS;
	unsigned opt;
	FILE *mount_table;
	struct mntent *mount_entry;
	struct statfs s;

	enum {
		OPT_KILO  = (1 << 0),
		OPT_POSIX = (1 << 1),
		OPT_FSTYPE  = (1 << 2),
		OPT_ALL   = (1 << 3) * ENABLE_FEATURE_DF_FANCY,
		OPT_INODE = (1 << 4) * ENABLE_FEATURE_DF_FANCY,
		OPT_BSIZE = (1 << 5) * ENABLE_FEATURE_DF_FANCY,
		OPT_HUMAN = (1 << (3 + 3*ENABLE_FEATURE_DF_FANCY)) * ENABLE_FEATURE_HUMAN_READABLE,
		OPT_MEGA  = (1 << (4 + 3*ENABLE_FEATURE_DF_FANCY)) * ENABLE_FEATURE_HUMAN_READABLE,
	};
	const char *disp_units_hdr = NULL;
	char *chp;

	init_unicode();

#if ENABLE_FEATURE_HUMAN_READABLE && ENABLE_FEATURE_DF_FANCY
	opt_complementary = "k-mB:m-Bk:B-km";
#elif ENABLE_FEATURE_HUMAN_READABLE
	opt_complementary = "k-m:m-k";
#endif
	opt = getopt32(argv, "kPT"
			IF_FEATURE_DF_FANCY("aiB:")
			IF_FEATURE_HUMAN_READABLE("hm")
			IF_FEATURE_DF_FANCY(, &chp));
	if (opt & OPT_MEGA)
		df_disp_hr = 1024*1024;

	if (opt & OPT_BSIZE)
		df_disp_hr = xatoul_range(chp, 1, ULONG_MAX); /* disallow 0 */

	/* From the manpage of df from coreutils-6.10:
	 * Disk space is shown in 1K blocks by default, unless the environment
	 * variable POSIXLY_CORRECT is set, in which case 512-byte blocks are used.
	 */
	if (getenv("POSIXLY_CORRECT")) /* TODO - a new libbb function? */
		df_disp_hr = 512;

	if (opt & OPT_HUMAN) {
		df_disp_hr = 0;
		disp_units_hdr = "     Size";
	}
	if (opt & OPT_INODE)
		disp_units_hdr = "   Inodes";

	if (disp_units_hdr == NULL) {
#if ENABLE_FEATURE_HUMAN_READABLE
		disp_units_hdr = xasprintf("%s-blocks",
			/* print df_disp_hr, show no fractionals,
			 * use suffixes if OPT_POSIX is set in opt */
			make_human_readable_str(df_disp_hr, 0, !!(opt & OPT_POSIX))
		);
#else
		disp_units_hdr = xasprintf("%lu-blocks", df_disp_hr);
#endif
	}

	printf("Filesystem           %s%-15sUsed Available %s Mounted on\n",
			(opt & OPT_FSTYPE) ? "Type       " : "",
			disp_units_hdr,
			(opt & OPT_POSIX) ? "Capacity" : "Use%");

	mount_table = NULL;
	argv += optind;
	if (!argv[0]) {
		mount_table = setmntent(bb_path_mtab_file, "r");
		if (!mount_table)
			bb_perror_msg_and_die(bb_path_mtab_file);
	}

	while (1) {
		const char *device;
		const char *mount_point;
		const char *fs_type;

		if (mount_table) {
			mount_entry = getmntent(mount_table);
			if (!mount_entry) {
				endmntent(mount_table);
				break;
			}
		} else {
			mount_point = *argv++;
			if (!mount_point)
				break;
			mount_entry = find_mount_point(mount_point, 1);
			if (!mount_entry) {
				bb_error_msg("%s: can't find mount point", mount_point);
 set_error:
				status = EXIT_FAILURE;
				continue;
			}
		}

		device = mount_entry->mnt_fsname;
		mount_point = mount_entry->mnt_dir;
		fs_type = mount_entry->mnt_type;

		if (statfs(mount_point, &s) != 0) {
			bb_simple_perror_msg(mount_point);
			goto set_error;
		}

		if ((s.f_blocks > 0) || !mount_table || (opt & OPT_ALL)) {
			if (opt & OPT_INODE) {
				s.f_blocks = s.f_files;
				s.f_bavail = s.f_bfree = s.f_ffree;
				s.f_bsize = 1;

				if (df_disp_hr)
					df_disp_hr = 1;
			}
			blocks_used = s.f_blocks - s.f_bfree;
			blocks_percent_used = 0;
			if (blocks_used + s.f_bavail) {
				blocks_percent_used = (blocks_used * 100ULL
						+ (blocks_used + s.f_bavail)/2
						) / (blocks_used + s.f_bavail);
			}

			/* GNU coreutils 6.10 skips certain mounts, try to be compatible.  */
			if (ENABLE_FEATURE_SKIP_ROOTFS && strcmp(device, "rootfs") == 0)
				continue;

#ifdef WHY_WE_DO_IT_FOR_DEV_ROOT_ONLY
			if (strcmp(device, "/dev/root") == 0) {
				/* Adjusts device to be the real root device,
				 * or leaves device alone if it can't find it */
				device = find_block_device("/");
				if (!device) {
					goto set_error;
				}
			}
#endif

#if ENABLE_UNICODE_SUPPORT
			{
				uni_stat_t uni_stat;
				char *uni_dev = unicode_conv_to_printable(&uni_stat, device);
				if (uni_stat.unicode_width > 20 && !(opt & OPT_POSIX)) {
					printf("%s\n%20s", uni_dev, "");
				} else {
					printf("%s%*s", uni_dev, 20 - (int)uni_stat.unicode_width, "");
				}
				free(uni_dev);
				if (opt & OPT_FSTYPE) {
					char *uni_type = unicode_conv_to_printable(&uni_stat, fs_type);
					if (uni_stat.unicode_width > 10 && !(opt & OPT_POSIX))
						printf(" %s\n%31s", uni_type, "");
					else
						printf(" %s%*s", uni_type, 10 - (int)uni_stat.unicode_width, "");
					free(uni_type);
				}
			}
#else
			if (printf("\n%-20s" + 1, device) > 20 && !(opt & OPT_POSIX))
				printf("\n%-20s", "");
			if (opt & OPT_FSTYPE) {
				if (printf(" %-10s", fs_type) > 11 && !(opt & OPT_POSIX))
					printf("\n%-30s", "");
			}
#endif

#if ENABLE_FEATURE_HUMAN_READABLE
			printf(" %9s ",
				/* f_blocks x f_bsize / df_disp_hr, show one fractional,
				 * use suffixes if df_disp_hr == 0 */
				make_human_readable_str(s.f_blocks, s.f_bsize, df_disp_hr));

			printf(" %9s " + 1,
				/* EXPR x f_bsize / df_disp_hr, show one fractional,
				 * use suffixes if df_disp_hr == 0 */
				make_human_readable_str((s.f_blocks - s.f_bfree),
						s.f_bsize, df_disp_hr));

			printf("%9s %3u%% %s\n",
				/* f_bavail x f_bsize / df_disp_hr, show one fractional,
				 * use suffixes if df_disp_hr == 0 */
				make_human_readable_str(s.f_bavail, s.f_bsize, df_disp_hr),
				blocks_percent_used, mount_point);
#else
			printf(" %9lu %9lu %9lu %3u%% %s\n",
				kscale(s.f_blocks, s.f_bsize),
				kscale(s.f_blocks - s.f_bfree, s.f_bsize),
				kscale(s.f_bavail, s.f_bsize),
				blocks_percent_used, mount_point);
#endif
		}
	}

	return status;
}
