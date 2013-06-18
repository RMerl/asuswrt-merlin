/*
 * unix.c - The unix-specific code for e2fsck
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _XOPEN_SOURCE 600 /* for inclusion of sa_handler in Solaris */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#include <unistd.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "e2p/e2p.h"
#include "et/com_err.h"
#include "e2p/e2p.h"
#include "e2fsck.h"
#include "problem.h"
#include "../version.h"

/* Command line options */
static int cflag;		/* check disk */
static int show_version_only;
static int verbose;

static int replace_bad_blocks;
static int keep_bad_blocks;
static char *bad_blocks_file;

e2fsck_t e2fsck_global_ctx;	/* Try your very best not to use this! */

#ifdef CONFIG_JBD_DEBUG		/* Enabled by configure --enable-jfs-debug */
int journal_enable_debug = -1;
#endif

static void usage(e2fsck_t ctx)
{
	fprintf(stderr,
		_("Usage: %s [-panyrcdfvtDFV] [-b superblock] [-B blocksize]\n"
		"\t\t[-I inode_buffer_blocks] [-P process_inode_size]\n"
		"\t\t[-l|-L bad_blocks_file] [-C fd] [-j external_journal]\n"
		"\t\t[-E extended-options] device\n"),
		ctx->program_name);

	fprintf(stderr, _("\nEmergency help:\n"
		" -p                   Automatic repair (no questions)\n"
		" -n                   Make no changes to the filesystem\n"
		" -y                   Assume \"yes\" to all questions\n"
		" -c                   Check for bad blocks and add them to the badblock list\n"
		" -f                   Force checking even if filesystem is marked clean\n"));
	fprintf(stderr, _(""
		" -v                   Be verbose\n"
		" -b superblock        Use alternative superblock\n"
		" -B blocksize         Force blocksize when looking for superblock\n"
		" -j external_journal  Set location of the external journal\n"
		" -l bad_blocks_file   Add to badblocks list\n"
		" -L bad_blocks_file   Set badblocks list\n"
		));

	exit(FSCK_USAGE);
}

static void show_stats(e2fsck_t	ctx)
{
	ext2_filsys fs = ctx->fs;
	ext2_ino_t inodes, inodes_used;
	blk64_t blocks, blocks_used;
	unsigned int dir_links;
	unsigned int num_files, num_links;
	__u32 *mask, m;
	int frag_percent_file, frag_percent_dir, frag_percent_total;
	int i, j, printed = 0;

	dir_links = 2 * ctx->fs_directory_count - 1;
	num_files = ctx->fs_total_count - dir_links;
	num_links = ctx->fs_links_count - dir_links;
	inodes = fs->super->s_inodes_count;
	inodes_used = (fs->super->s_inodes_count -
		       fs->super->s_free_inodes_count);
	blocks = ext2fs_blocks_count(fs->super);
	blocks_used = (ext2fs_blocks_count(fs->super) -
		       ext2fs_free_blocks_count(fs->super));

	frag_percent_file = (10000 * ctx->fs_fragmented) / inodes_used;
	frag_percent_file = (frag_percent_file + 5) / 10;

	frag_percent_dir = (10000 * ctx->fs_fragmented_dir) / inodes_used;
	frag_percent_dir = (frag_percent_dir + 5) / 10;

	frag_percent_total = ((10000 * (ctx->fs_fragmented +
					ctx->fs_fragmented_dir))
			      / inodes_used);
	frag_percent_total = (frag_percent_total + 5) / 10;

	if (!verbose) {
		log_out(ctx, _("%s: %u/%u files (%0d.%d%% non-contiguous), "
			       "%llu/%llu blocks\n"),
			ctx->device_name, inodes_used, inodes,
			frag_percent_total / 10, frag_percent_total % 10,
			blocks_used, blocks);
		return;
	}
	profile_get_boolean(ctx->profile, "options", "report_features", 0, 0,
			    &i);
	if (verbose && i) {
		log_out(ctx, "\nFilesystem features:");
		mask = &ctx->fs->super->s_feature_compat;
		for (i = 0; i < 3; i++, mask++) {
			for (j = 0, m = 1; j < 32; j++, m <<= 1) {
				if (*mask & m) {
					log_out(ctx, " %s",
						e2p_feature2string(i, m));
					printed++;
				}
			}
		}
		if (printed == 0)
			log_out(ctx, " (none)");
		log_out(ctx, "\n");
	}

	log_out(ctx, P_("\n%12u inode used (%2.2f%%, out of %u)\n",
			"\n%12u inodes used (%2.2f%%, out of %u)\n",
			inodes_used), inodes_used,
		100.0 * inodes_used / inodes, inodes);
	log_out(ctx, P_("%12u non-contiguous file (%0d.%d%%)\n",
			"%12u non-contiguous files (%0d.%d%%)\n",
			ctx->fs_fragmented),
		ctx->fs_fragmented, frag_percent_file / 10,
		frag_percent_file % 10);
	log_out(ctx, P_("%12u non-contiguous directory (%0d.%d%%)\n",
			"%12u non-contiguous directories (%0d.%d%%)\n",
			ctx->fs_fragmented_dir),
		ctx->fs_fragmented_dir, frag_percent_dir / 10,
		frag_percent_dir % 10);
	log_out(ctx, _("             # of inodes with ind/dind/tind blocks: "
		       "%u/%u/%u\n"),
		ctx->fs_ind_count, ctx->fs_dind_count, ctx->fs_tind_count);

	for (j=MAX_EXTENT_DEPTH_COUNT-1; j >=0; j--)
		if (ctx->extent_depth_count[j])
			break;
	if (++j) {
		log_out(ctx, _("             Extent depth histogram: "));
		for (i=0; i < j; i++) {
			if (i)
				fputc('/', stdout);
			log_out(ctx, "%u", ctx->extent_depth_count[i]);
		}
		log_out(ctx, "\n");
	}

	log_out(ctx, P_("%12llu block used (%2.2f%%, out of %llu)\n",
			"%12llu blocks used (%2.2f%%, out of %llu)\n",
		   blocks_used),
		blocks_used, 100.0 * blocks_used / blocks, blocks);
	log_out(ctx, P_("%12u bad block\n", "%12u bad blocks\n",
			ctx->fs_badblocks_count), ctx->fs_badblocks_count);
	log_out(ctx, P_("%12u large file\n", "%12u large files\n",
			ctx->large_files), ctx->large_files);
	log_out(ctx, P_("\n%12u regular file\n", "\n%12u regular files\n",
			ctx->fs_regular_count), ctx->fs_regular_count);
	log_out(ctx, P_("%12u directory\n", "%12u directories\n",
			ctx->fs_directory_count), ctx->fs_directory_count);
	log_out(ctx, P_("%12u character device file\n",
			"%12u character device files\n", ctx->fs_chardev_count),
		ctx->fs_chardev_count);
	log_out(ctx, P_("%12u block device file\n", "%12u block device files\n",
			ctx->fs_blockdev_count), ctx->fs_blockdev_count);
	log_out(ctx, P_("%12u fifo\n", "%12u fifos\n", ctx->fs_fifo_count),
		ctx->fs_fifo_count);
	log_out(ctx, P_("%12u link\n", "%12u links\n", num_links),
		ctx->fs_links_count - dir_links);
	log_out(ctx, P_("%12u symbolic link", "%12u symbolic links",
			ctx->fs_symlinks_count), ctx->fs_symlinks_count);
	log_out(ctx, P_(" (%u fast symbolic link)\n",
			" (%u fast symbolic links)\n",
			ctx->fs_fast_symlinks_count),
		ctx->fs_fast_symlinks_count);
	log_out(ctx, P_("%12u socket\n", "%12u sockets\n",
			ctx->fs_sockets_count),
		ctx->fs_sockets_count);
	log_out(ctx, "------------\n");
	log_out(ctx, P_("%12u file\n", "%12u files\n", num_files),
		num_files);
}

static void check_mount(e2fsck_t ctx)
{
	errcode_t	retval;
	int		cont;

	retval = ext2fs_check_if_mounted(ctx->filesystem_name,
					 &ctx->mount_flags);
	if (retval) {
		com_err("ext2fs_check_if_mount", retval,
			_("while determining whether %s is mounted."),
			ctx->filesystem_name);
		return;
	}

	/*
	 * If the filesystem isn't mounted, or it's the root
	 * filesystem and it's mounted read-only, and we're not doing
	 * a read/write check, then everything's fine.
	 */
	if ((!(ctx->mount_flags & (EXT2_MF_MOUNTED | EXT2_MF_BUSY))) ||
	    ((ctx->mount_flags & EXT2_MF_ISROOT) &&
	     (ctx->mount_flags & EXT2_MF_READONLY) &&
	     !(ctx->options & E2F_OPT_WRITECHECK)))
		return;

	if (((ctx->options & E2F_OPT_READONLY) ||
	     ((ctx->options & E2F_OPT_FORCE) &&
	      (ctx->mount_flags & EXT2_MF_READONLY))) &&
	    !(ctx->options & E2F_OPT_WRITECHECK)) {
		log_out(ctx, _("Warning!  %s is %s.\n"),
			ctx->filesystem_name,
			ctx->mount_flags & EXT2_MF_MOUNTED ?
				"mounted" : "in use");
		return;
	}

	log_out(ctx, _("%s is %s.\n"), ctx->filesystem_name,
		ctx->mount_flags & EXT2_MF_MOUNTED ? "mounted" : "in use");
	if (!ctx->interactive || ctx->mount_flags & EXT2_MF_BUSY)
		fatal_error(ctx, _("Cannot continue, aborting.\n\n"));
	puts("\007\007\007\007");
	log_out(ctx, _("\n\nWARNING!!!  "
		       "The filesystem is mounted.   "
		       "If you continue you ***WILL***\n"
		       "cause ***SEVERE*** filesystem damage.\n\n"));
	puts("\007\007\007");
	cont = ask_yn(ctx, _("Do you really want to continue"), 0);
	if (!cont) {
		printf (_("check aborted.\n"));
		exit (0);
	}
	return;
}

static int is_on_batt(void)
{
	FILE	*f;
	DIR	*d;
	char	tmp[80], tmp2[80], fname[80];
	unsigned int	acflag;
	struct dirent*	de;

	f = fopen("/sys/class/power_supply/AC/online", "r");
	if (f) {
		if (fscanf(f, "%u\n", &acflag) == 1) {
			fclose(f);
			return (!acflag);
		}
		fclose(f);
	}
	f = fopen("/proc/apm", "r");
	if (f) {
		if (fscanf(f, "%s %s %s %x", tmp, tmp, tmp, &acflag) != 4)
			acflag = 1;
		fclose(f);
		return (acflag != 1);
	}
	d = opendir("/proc/acpi/ac_adapter");
	if (d) {
		while ((de=readdir(d)) != NULL) {
			if (!strncmp(".", de->d_name, 1))
				continue;
			snprintf(fname, 80, "/proc/acpi/ac_adapter/%s/state",
				 de->d_name);
			f = fopen(fname, "r");
			if (!f)
				continue;
			if (fscanf(f, "%s %s", tmp2, tmp) != 2)
				tmp[0] = 0;
			fclose(f);
			if (strncmp(tmp, "off-line", 8) == 0) {
				closedir(d);
				return 1;
			}
		}
		closedir(d);
	}
	return 0;
}

/*
 * This routine checks to see if a filesystem can be skipped; if so,
 * it will exit with E2FSCK_OK.  Under some conditions it will print a
 * message explaining why a check is being forced.
 */
static void check_if_skip(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	struct problem_context pctx;
	const char *reason = NULL;
	unsigned int reason_arg = 0;
	long next_check;
	int batt = is_on_batt();
	int defer_check_on_battery;
	int broken_system_clock;
	time_t lastcheck;

	profile_get_boolean(ctx->profile, "options", "broken_system_clock",
			    0, 0, &broken_system_clock);
	if (ctx->flags & E2F_FLAG_TIME_INSANE)
		broken_system_clock = 1;
	profile_get_boolean(ctx->profile, "options",
			    "defer_check_on_battery", 0, 1,
			    &defer_check_on_battery);
	if (!defer_check_on_battery)
		batt = 0;

	if ((ctx->options & E2F_OPT_FORCE) || bad_blocks_file || cflag)
		return;

	if (ctx->options & E2F_OPT_JOURNAL_ONLY)
		goto skip;

	lastcheck = fs->super->s_lastcheck;
	if (lastcheck > ctx->now)
		lastcheck -= ctx->time_fudge;
	if ((fs->super->s_state & EXT2_ERROR_FS) ||
	    !ext2fs_test_valid(fs))
		reason = _(" contains a file system with errors");
	else if ((fs->super->s_state & EXT2_VALID_FS) == 0)
		reason = _(" was not cleanly unmounted");
	else if (check_backup_super_block(ctx))
		reason = _(" primary superblock features different from backup");
	else if ((fs->super->s_max_mnt_count > 0) &&
		 (fs->super->s_mnt_count >=
		  (unsigned) fs->super->s_max_mnt_count)) {
		reason = _(" has been mounted %u times without being checked");
		reason_arg = fs->super->s_mnt_count;
		if (batt && (fs->super->s_mnt_count <
			     (unsigned) fs->super->s_max_mnt_count*2))
			reason = 0;
	} else if (!broken_system_clock && fs->super->s_checkinterval &&
		   (ctx->now < lastcheck)) {
		reason = _(" has filesystem last checked time in the future");
		if (batt)
			reason = 0;
	} else if (!broken_system_clock && fs->super->s_checkinterval &&
		   ((ctx->now - lastcheck) >=
		    ((time_t) fs->super->s_checkinterval))) {
		reason = _(" has gone %u days without being checked");
		reason_arg = (ctx->now - fs->super->s_lastcheck)/(3600*24);
		if (batt && ((ctx->now - fs->super->s_lastcheck) <
			     fs->super->s_checkinterval*2))
			reason = 0;
	}
	if (reason) {
		log_out(ctx, "%s", ctx->device_name);
		log_out(ctx, reason, reason_arg);
		log_out(ctx, _(", check forced.\n"));
		return;
	}

	/*
	 * Update the global counts from the block group counts.  This
	 * is needed since modern kernels don't update the global
	 * counts so as to avoid locking the entire file system.  So
	 * if the filesystem is not unmounted cleanly, the global
	 * counts may not be accurate.  Update them here if we can,
	 * for the benefit of users who might examine the file system
	 * using dumpe2fs.  (This is for cosmetic reasons only.)
	 */
	clear_problem_context(&pctx);
	pctx.ino = fs->super->s_free_inodes_count;
	pctx.ino2 = ctx->free_inodes;
	if ((pctx.ino != pctx.ino2) &&
	    !(ctx->options & E2F_OPT_READONLY) &&
	    fix_problem(ctx, PR_0_FREE_INODE_COUNT, &pctx)) {
		fs->super->s_free_inodes_count = ctx->free_inodes;
		ext2fs_mark_super_dirty(fs);
	}
	clear_problem_context(&pctx);
	pctx.blk = ext2fs_free_blocks_count(fs->super);
	pctx.blk2 = ctx->free_blocks;
	if ((pctx.blk != pctx.blk2) &&
	    !(ctx->options & E2F_OPT_READONLY) &&
	    fix_problem(ctx, PR_0_FREE_BLOCK_COUNT, &pctx)) {
		ext2fs_free_blocks_count_set(fs->super, ctx->free_blocks);
		ext2fs_mark_super_dirty(fs);
	}

	/* Print the summary message when we're skipping a full check */
	log_out(ctx, _("%s: clean, %u/%u files, %llu/%llu blocks"),
		ctx->device_name,
		fs->super->s_inodes_count - fs->super->s_free_inodes_count,
		fs->super->s_inodes_count,
		ext2fs_blocks_count(fs->super) -
		ext2fs_free_blocks_count(fs->super),
		ext2fs_blocks_count(fs->super));
	next_check = 100000;
	if (fs->super->s_max_mnt_count > 0) {
		next_check = fs->super->s_max_mnt_count - fs->super->s_mnt_count;
		if (next_check <= 0)
			next_check = 1;
	}
	if (!broken_system_clock && fs->super->s_checkinterval &&
	    ((ctx->now - fs->super->s_lastcheck) >= fs->super->s_checkinterval))
		next_check = 1;
	if (next_check <= 5) {
		if (next_check == 1) {
			if (batt)
				log_out(ctx, _(" (check deferred; "
					       "on battery)"));
			else
				log_out(ctx, _(" (check after next mount)"));
		} else
			log_out(ctx, _(" (check in %ld mounts)"), next_check);
	}
	log_out(ctx, "\n");
skip:
	ext2fs_close(fs);
	ctx->fs = NULL;
	e2fsck_free_context(ctx);
	exit(FSCK_OK);
}

/*
 * For completion notice
 */
struct percent_tbl {
	int	max_pass;
	int	table[32];
};
struct percent_tbl e2fsck_tbl = {
	5, { 0, 70, 90, 92,  95, 100 }
};
static char bar[128], spaces[128];

static float calc_percent(struct percent_tbl *tbl, int pass, int curr,
			  int max)
{
	float	percent;

	if (pass <= 0)
		return 0.0;
	if (pass > tbl->max_pass || max == 0)
		return 100.0;
	percent = ((float) curr) / ((float) max);
	return ((percent * (tbl->table[pass] - tbl->table[pass-1]))
		+ tbl->table[pass-1]);
}

extern void e2fsck_clear_progbar(e2fsck_t ctx)
{
	if (!(ctx->flags & E2F_FLAG_PROG_BAR))
		return;

	printf("%s%s\r%s", ctx->start_meta, spaces + (sizeof(spaces) - 80),
	       ctx->stop_meta);
	fflush(stdout);
	ctx->flags &= ~E2F_FLAG_PROG_BAR;
}

int e2fsck_simple_progress(e2fsck_t ctx, const char *label, float percent,
			   unsigned int dpynum)
{
	static const char spinner[] = "\\|/-";
	int	i;
	unsigned int	tick;
	struct timeval	tv;
	int dpywidth;
	int fixed_percent;

	if (ctx->flags & E2F_FLAG_PROG_SUPPRESS)
		return 0;

	/*
	 * Calculate the new progress position.  If the
	 * percentage hasn't changed, then we skip out right
	 * away.
	 */
	fixed_percent = (int) ((10 * percent) + 0.5);
	if (ctx->progress_last_percent == fixed_percent)
		return 0;
	ctx->progress_last_percent = fixed_percent;

	/*
	 * If we've already updated the spinner once within
	 * the last 1/8th of a second, no point doing it
	 * again.
	 */
	gettimeofday(&tv, NULL);
	tick = (tv.tv_sec << 3) + (tv.tv_usec / (1000000 / 8));
	if ((tick == ctx->progress_last_time) &&
	    (fixed_percent != 0) && (fixed_percent != 1000))
		return 0;
	ctx->progress_last_time = tick;

	/*
	 * Advance the spinner, and note that the progress bar
	 * will be on the screen
	 */
	ctx->progress_pos = (ctx->progress_pos+1) & 3;
	ctx->flags |= E2F_FLAG_PROG_BAR;

	dpywidth = 66 - strlen(label);
	dpywidth = 8 * (dpywidth / 8);
	if (dpynum)
		dpywidth -= 8;

	i = ((percent * dpywidth) + 50) / 100;
	printf("%s%s: |%s%s", ctx->start_meta, label,
	       bar + (sizeof(bar) - (i+1)),
	       spaces + (sizeof(spaces) - (dpywidth - i + 1)));
	if (fixed_percent == 1000)
		fputc('|', stdout);
	else
		fputc(spinner[ctx->progress_pos & 3], stdout);
	printf(" %4.1f%%  ", percent);
	if (dpynum)
		printf("%u\r", dpynum);
	else
		fputs(" \r", stdout);
	fputs(ctx->stop_meta, stdout);

	if (fixed_percent == 1000)
		e2fsck_clear_progbar(ctx);
	fflush(stdout);

	return 0;
}

static int e2fsck_update_progress(e2fsck_t ctx, int pass,
				  unsigned long cur, unsigned long max)
{
	char buf[1024];
	float percent;

	if (pass == 0)
		return 0;

	if (ctx->progress_fd) {
		snprintf(buf, sizeof(buf), "%d %lu %lu %s\n",
			 pass, cur, max, ctx->device_name);
		write_all(ctx->progress_fd, buf, strlen(buf));
	} else {
		percent = calc_percent(&e2fsck_tbl, pass, cur, max);
		e2fsck_simple_progress(ctx, ctx->device_name,
				       percent, 0);
	}
	return 0;
}

#define PATH_SET "PATH=/sbin"

/*
 * Make sure 0,1,2 file descriptors are open, so that we don't open
 * the filesystem using the same file descriptor as stdout or stderr.
 */
static void reserve_stdio_fds(void)
{
	int	fd = 0;

	while (fd <= 2) {
		fd = open("/dev/null", O_RDWR);
		if (fd < 0) {
			fprintf(stderr, _("ERROR: Couldn't open "
				"/dev/null (%s)\n"),
				strerror(errno));
			break;
		}
	}
}

#ifdef HAVE_SIGNAL_H
static void signal_progress_on(int sig EXT2FS_ATTR((unused)))
{
	e2fsck_t ctx = e2fsck_global_ctx;

	if (!ctx)
		return;

	ctx->progress = e2fsck_update_progress;
}

static void signal_progress_off(int sig EXT2FS_ATTR((unused)))
{
	e2fsck_t ctx = e2fsck_global_ctx;

	if (!ctx)
		return;

	e2fsck_clear_progbar(ctx);
	ctx->progress = 0;
}

static void signal_cancel(int sig EXT2FS_ATTR((unused)))
{
	e2fsck_t ctx = e2fsck_global_ctx;

	if (!ctx)
		exit(FSCK_CANCELED);

	ctx->flags |= E2F_FLAG_CANCEL;
}
#endif

static void parse_extended_opts(e2fsck_t ctx, const char *opts)
{
	char	*buf, *token, *next, *p, *arg;
	int	ea_ver;
	int	extended_usage = 0;

	buf = string_copy(ctx, opts, 0);
	for (token = buf; token && *token; token = next) {
		p = strchr(token, ',');
		next = 0;
		if (p) {
			*p = 0;
			next = p+1;
		}
		arg = strchr(token, '=');
		if (arg) {
			*arg = 0;
			arg++;
		}
		if (strcmp(token, "ea_ver") == 0) {
			if (!arg) {
				extended_usage++;
				continue;
			}
			ea_ver = strtoul(arg, &p, 0);
			if (*p ||
			    ((ea_ver != 1) && (ea_ver != 2))) {
				fprintf(stderr,
					_("Invalid EA version.\n"));
				extended_usage++;
				continue;
			}
			ctx->ext_attr_ver = ea_ver;
		} else if (strcmp(token, "fragcheck") == 0) {
			ctx->options |= E2F_OPT_FRAGCHECK;
			continue;
		} else if (strcmp(token, "journal_only") == 0) {
			if (arg) {
				extended_usage++;
				continue;
			}
			ctx->options |= E2F_OPT_JOURNAL_ONLY;
		} else if (strcmp(token, "discard") == 0) {
			ctx->options |= E2F_OPT_DISCARD;
			continue;
		} else if (strcmp(token, "nodiscard") == 0) {
			ctx->options &= ~E2F_OPT_DISCARD;
			continue;
		} else if (strcmp(token, "log_filename") == 0) {
			if (!arg)
				extended_usage++;
			else
				ctx->log_fn = string_copy(ctx, arg, 0);
			continue;
		} else {
			fprintf(stderr, _("Unknown extended option: %s\n"),
				token);
			extended_usage++;
		}
	}
	free(buf);

	if (extended_usage) {
		fputs(("\nExtended options are separated by commas, "
		       "and may take an argument which\n"
		       "is set off by an equals ('=') sign.  "
		       "Valid extended options are:\n"), stderr);
		fputs(("\tea_ver=<ea_version (1 or 2)>\n"), stderr);
		fputs(("\tfragcheck\n"), stderr);
		fputs(("\tjournal_only\n"), stderr);
		fputs(("\tdiscard\n"), stderr);
		fputs(("\tnodiscard\n"), stderr);
		fputc('\n', stderr);
		exit(1);
	}
}

static void syntax_err_report(const char *filename, long err, int line_num)
{
	fprintf(stderr,
		_("Syntax error in e2fsck config file (%s, line #%d)\n\t%s\n"),
		filename, line_num, error_message(err));
	exit(FSCK_ERROR);
}

static const char *config_fn[] = { ROOT_SYSCONFDIR "/e2fsck.conf", 0 };

static errcode_t PRS(int argc, char *argv[], e2fsck_t *ret_ctx)
{
	int		flush = 0;
	int		c, fd;
#ifdef MTRACE
	extern void	*mallwatch;
#endif
	e2fsck_t	ctx;
	errcode_t	retval;
#ifdef HAVE_SIGNAL_H
	struct sigaction	sa;
#endif
	char		*extended_opts = 0;
	char		*cp;
	int 		res;		/* result of sscanf */
#ifdef CONFIG_JBD_DEBUG
	char 		*jbd_debug;
#endif

	retval = e2fsck_allocate_context(&ctx);
	if (retval)
		return retval;

	*ret_ctx = ctx;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	setvbuf(stderr, NULL, _IONBF, BUFSIZ);
	if (isatty(0) && isatty(1)) {
		ctx->interactive = 1;
	} else {
		ctx->start_meta[0] = '\001';
		ctx->stop_meta[0] = '\002';
	}
	memset(bar, '=', sizeof(bar)-1);
	memset(spaces, ' ', sizeof(spaces)-1);
	add_error_table(&et_ext2_error_table);
	add_error_table(&et_prof_error_table);
	blkid_get_cache(&ctx->blkid, NULL);

	if (argc && *argv)
		ctx->program_name = *argv;
	else
		ctx->program_name = "e2fsck";

	while ((c = getopt (argc, argv, "panyrcC:B:dE:fvtFVM:b:I:j:P:l:L:N:SsDk")) != EOF)
		switch (c) {
		case 'C':
			ctx->progress = e2fsck_update_progress;
			res = sscanf(optarg, "%d", &ctx->progress_fd);
			if (res != 1)
				goto sscanf_err;

			if (ctx->progress_fd < 0) {
				ctx->progress = 0;
				ctx->progress_fd = ctx->progress_fd * -1;
			}
			if (!ctx->progress_fd)
				break;
			/* Validate the file descriptor to avoid disasters */
			fd = dup(ctx->progress_fd);
			if (fd < 0) {
				fprintf(stderr,
				_("Error validating file descriptor %d: %s\n"),
					ctx->progress_fd,
					error_message(errno));
				fatal_error(ctx,
			_("Invalid completion information file descriptor"));
			} else
				close(fd);
			break;
		case 'D':
			ctx->options |= E2F_OPT_COMPRESS_DIRS;
			break;
		case 'E':
			extended_opts = optarg;
			break;
		case 'p':
		case 'a':
			if (ctx->options & (E2F_OPT_YES|E2F_OPT_NO)) {
			conflict_opt:
				fatal_error(ctx,
	_("Only one of the options -p/-a, -n or -y may be specified."));
			}
			ctx->options |= E2F_OPT_PREEN;
			break;
		case 'n':
			if (ctx->options & (E2F_OPT_YES|E2F_OPT_PREEN))
				goto conflict_opt;
			ctx->options |= E2F_OPT_NO;
			break;
		case 'y':
			if (ctx->options & (E2F_OPT_PREEN|E2F_OPT_NO))
				goto conflict_opt;
			ctx->options |= E2F_OPT_YES;
			break;
		case 't':
#ifdef RESOURCE_TRACK
			if (ctx->options & E2F_OPT_TIME)
				ctx->options |= E2F_OPT_TIME2;
			else
				ctx->options |= E2F_OPT_TIME;
#else
			fprintf(stderr, _("The -t option is not "
				"supported on this version of e2fsck.\n"));
#endif
			break;
		case 'c':
			if (cflag++)
				ctx->options |= E2F_OPT_WRITECHECK;
			ctx->options |= E2F_OPT_CHECKBLOCKS;
			break;
		case 'r':
			/* What we do by default, anyway! */
			break;
		case 'b':
			res = sscanf(optarg, "%llu", &ctx->use_superblock);
			if (res != 1)
				goto sscanf_err;
			ctx->flags |= E2F_FLAG_SB_SPECIFIED;
			break;
		case 'B':
			ctx->blocksize = atoi(optarg);
			break;
		case 'I':
			res = sscanf(optarg, "%d", &ctx->inode_buffer_blocks);
			if (res != 1)
				goto sscanf_err;
			break;
		case 'j':
			ctx->journal_name = blkid_get_devname(ctx->blkid,
							      optarg, NULL);
			if (!ctx->journal_name) {
				com_err(ctx->program_name, 0,
					_("Unable to resolve '%s'"),
					optarg);
				fatal_error(ctx, 0);
			}
			break;
		case 'P':
			res = sscanf(optarg, "%d", &ctx->process_inode_size);
			if (res != 1)
				goto sscanf_err;
			break;
		case 'L':
			replace_bad_blocks++;
		case 'l':
			bad_blocks_file = string_copy(ctx, optarg, 0);
			break;
		case 'd':
			ctx->options |= E2F_OPT_DEBUG;
			break;
		case 'f':
			ctx->options |= E2F_OPT_FORCE;
			break;
		case 'F':
			flush = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			show_version_only = 1;
			break;
#ifdef MTRACE
		case 'M':
			mallwatch = (void *) strtol(optarg, NULL, 0);
			break;
#endif
		case 'N':
			ctx->device_name = string_copy(ctx, optarg, 0);
			break;
		case 'k':
			keep_bad_blocks++;
			break;
		default:
			usage(ctx);
		}
	if (show_version_only)
		return 0;
	if (optind != argc - 1)
		usage(ctx);
	if ((ctx->options & E2F_OPT_NO) &&
	    (ctx->options & E2F_OPT_COMPRESS_DIRS)) {
		com_err(ctx->program_name, 0,
			_("The -n and -D options are incompatible."));
		fatal_error(ctx, 0);
	}
	if ((ctx->options & E2F_OPT_NO) && cflag) {
		com_err(ctx->program_name, 0,
			_("The -n and -c options are incompatible."));
		fatal_error(ctx, 0);
	}
	if ((ctx->options & E2F_OPT_NO) && bad_blocks_file) {
		com_err(ctx->program_name, 0,
			_("The -n and -l/-L options are incompatible."));
		fatal_error(ctx, 0);
	}
	if (ctx->options & E2F_OPT_NO)
		ctx->options |= E2F_OPT_READONLY;

	ctx->io_options = strchr(argv[optind], '?');
	if (ctx->io_options)
		*ctx->io_options++ = 0;
	ctx->filesystem_name = blkid_get_devname(ctx->blkid, argv[optind], 0);
	if (!ctx->filesystem_name) {
		com_err(ctx->program_name, 0, _("Unable to resolve '%s'"),
			argv[optind]);
		fatal_error(ctx, 0);
	}
	if (extended_opts)
		parse_extended_opts(ctx, extended_opts);

	if ((cp = getenv("E2FSCK_CONFIG")) != NULL)
		config_fn[0] = cp;
	profile_set_syntax_err_cb(syntax_err_report);
	profile_init(config_fn, &ctx->profile);

	profile_get_boolean(ctx->profile, "options", "report_time", 0, 0,
			    &c);
	if (c)
		ctx->options |= E2F_OPT_TIME | E2F_OPT_TIME2;
	profile_get_boolean(ctx->profile, "options", "report_verbose", 0, 0,
			    &c);
	if (c)
		verbose = 1;

	/* Turn off discard in read-only mode */
	if ((ctx->options & E2F_OPT_NO) &&
	    (ctx->options & E2F_OPT_DISCARD))
		ctx->options &= ~E2F_OPT_DISCARD;

	if (flush) {
		fd = open(ctx->filesystem_name, O_RDONLY, 0);
		if (fd < 0) {
			com_err("open", errno,
				_("while opening %s for flushing"),
				ctx->filesystem_name);
			fatal_error(ctx, 0);
		}
		if ((retval = ext2fs_sync_device(fd, 1))) {
			com_err("ext2fs_sync_device", retval,
				_("while trying to flush %s"),
				ctx->filesystem_name);
			fatal_error(ctx, 0);
		}
		close(fd);
	}
	if (cflag && bad_blocks_file) {
		fprintf(stderr, _("The -c and the -l/-L options may "
				  "not be both used at the same time.\n"));
		exit(FSCK_USAGE);
	}
#ifdef HAVE_SIGNAL_H
	/*
	 * Set up signal action
	 */
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = signal_cancel;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
#ifdef SA_RESTART
	sa.sa_flags = SA_RESTART;
#endif
	e2fsck_global_ctx = ctx;
	sa.sa_handler = signal_progress_on;
	sigaction(SIGUSR1, &sa, 0);
	sa.sa_handler = signal_progress_off;
	sigaction(SIGUSR2, &sa, 0);
#endif

	/* Update our PATH to include /sbin if we need to run badblocks  */
	if (cflag) {
		char *oldpath = getenv("PATH");
		char *newpath;
		int len = sizeof(PATH_SET) + 1;

		if (oldpath)
			len += strlen(oldpath);

		newpath = malloc(len);
		if (!newpath)
			fatal_error(ctx, "Couldn't malloc() newpath");
		strcpy(newpath, PATH_SET);

		if (oldpath) {
			strcat(newpath, ":");
			strcat(newpath, oldpath);
		}
		putenv(newpath);
	}
#ifdef CONFIG_JBD_DEBUG
	jbd_debug = getenv("E2FSCK_JBD_DEBUG");
	if (jbd_debug) {
		res = sscanf(jbd_debug, "%d", &journal_enable_debug);
		if (res != 1) {
			fprintf(stderr,
			        _("E2FSCK_JBD_DEBUG \"%s\" not an integer\n\n"),
			        jbd_debug);
			exit (1);
		}
	}
#endif
	return 0;

sscanf_err:
	fprintf(stderr, _("\nInvalid non-numeric argument to -%c (\"%s\")\n\n"),
	        c, optarg);
	exit (1);
}

static errcode_t try_open_fs(e2fsck_t ctx, int flags, io_manager io_ptr,
			     ext2_filsys *ret_fs)
{
	errcode_t retval;

	*ret_fs = NULL;
	if (ctx->superblock && ctx->blocksize) {
		retval = ext2fs_open2(ctx->filesystem_name, ctx->io_options,
				      flags, ctx->superblock, ctx->blocksize,
				      io_ptr, ret_fs);
	} else if (ctx->superblock) {
		int blocksize;
		for (blocksize = EXT2_MIN_BLOCK_SIZE;
		     blocksize <= EXT2_MAX_BLOCK_SIZE; blocksize *= 2) {
			if (*ret_fs) {
				ext2fs_free(*ret_fs);
				*ret_fs = NULL;
			}
			retval = ext2fs_open2(ctx->filesystem_name,
					      ctx->io_options, flags,
					      ctx->superblock, blocksize,
					      io_ptr, ret_fs);
			if (!retval)
				break;
		}
	} else
		retval = ext2fs_open2(ctx->filesystem_name, ctx->io_options,
				      flags, 0, 0, io_ptr, ret_fs);

	if (retval == 0)
		e2fsck_set_bitmap_type(*ret_fs, EXT2FS_BMAP64_RBTREE,
				       "default", NULL);
	return retval;
}

static const char *my_ver_string = E2FSPROGS_VERSION;
static const char *my_ver_date = E2FSPROGS_DATE;

static int e2fsck_check_mmp(ext2_filsys fs, e2fsck_t ctx)
{
	struct mmp_struct *mmp_s;
	unsigned int mmp_check_interval;
	errcode_t retval = 0;
	struct problem_context pctx;
	unsigned int wait_time = 0;

	clear_problem_context(&pctx);
	if (fs->mmp_buf == NULL) {
		retval = ext2fs_get_mem(fs->blocksize, &fs->mmp_buf);
		if (retval)
			goto check_error;
	}

	retval = ext2fs_mmp_read(fs, fs->super->s_mmp_block, fs->mmp_buf);
	if (retval)
		goto check_error;

	mmp_s = fs->mmp_buf;

	mmp_check_interval = fs->super->s_mmp_update_interval;
	if (mmp_check_interval < EXT4_MMP_MIN_CHECK_INTERVAL)
		mmp_check_interval = EXT4_MMP_MIN_CHECK_INTERVAL;

	/*
	 * If check_interval in MMP block is larger, use that instead of
	 * check_interval from the superblock.
	 */
	if (mmp_s->mmp_check_interval > mmp_check_interval)
		mmp_check_interval = mmp_s->mmp_check_interval;

	wait_time = mmp_check_interval * 2 + 1;

	if (mmp_s->mmp_seq == EXT4_MMP_SEQ_CLEAN)
		retval = 0;
	else if (mmp_s->mmp_seq == EXT4_MMP_SEQ_FSCK)
		retval = EXT2_ET_MMP_FSCK_ON;
	else if (mmp_s->mmp_seq > EXT4_MMP_SEQ_MAX)
		retval = EXT2_ET_MMP_UNKNOWN_SEQ;

	if (retval)
		goto check_error;

	/* Print warning if e2fck will wait for more than 20 secs. */
	if (verbose || wait_time > EXT4_MMP_MIN_CHECK_INTERVAL * 4) {
		log_out(ctx, _("MMP interval is %u seconds and total wait "
			       "time is %u seconds. Please wait...\n"),
			mmp_check_interval, wait_time * 2);
	}

	return 0;

check_error:

	if (retval == EXT2_ET_MMP_BAD_BLOCK) {
		if (fix_problem(ctx, PR_0_MMP_INVALID_BLK, &pctx)) {
			fs->super->s_mmp_block = 0;
			ext2fs_mark_super_dirty(fs);
			retval = 0;
		}
	} else if (retval == EXT2_ET_MMP_FAILED) {
		com_err(ctx->program_name, retval,
			_("while checking MMP block"));
		dump_mmp_msg(fs->mmp_buf, NULL);
	} else if (retval == EXT2_ET_MMP_FSCK_ON ||
		   retval == EXT2_ET_MMP_UNKNOWN_SEQ) {
		com_err(ctx->program_name, retval,
			_("while checking MMP block"));
		dump_mmp_msg(fs->mmp_buf,
			     _("If you are sure the filesystem is not "
			       "in use on any node, run:\n"
			       "'tune2fs -f -E clear_mmp {device}'\n"));
	} else if (retval == EXT2_ET_MMP_MAGIC_INVALID) {
		if (fix_problem(ctx, PR_0_MMP_INVALID_MAGIC, &pctx)) {
			ext2fs_mmp_clear(fs);
			retval = 0;
		}
	}
	return retval;
}

int main (int argc, char *argv[])
{
	errcode_t	retval = 0, retval2 = 0, orig_retval = 0;
	int		exit_value = FSCK_OK;
	ext2_filsys	fs = 0;
	io_manager	io_ptr;
	struct ext2_super_block *sb;
	const char	*lib_ver_date;
	int		my_ver, lib_ver;
	e2fsck_t	ctx;
	blk_t		orig_superblock;
	struct problem_context pctx;
	int flags, run_result;
	int journal_size;
	int sysval, sys_page_size = 4096;
	int old_bitmaps;
	__u32 features[3];
	char *cp;
	int qtype = -99;  /* quota type */

	clear_problem_context(&pctx);
	sigcatcher_setup();
#ifdef MTRACE
	mtrace();
#endif
#ifdef MCHECK
	mcheck(0);
#endif
#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
	set_com_err_gettext(gettext);
#endif
	my_ver = ext2fs_parse_version_string(my_ver_string);
	lib_ver = ext2fs_get_library_version(0, &lib_ver_date);
	if (my_ver > lib_ver) {
		fprintf( stderr, _("Error: ext2fs library version "
			"out of date!\n"));
		show_version_only++;
	}

	retval = PRS(argc, argv, &ctx);
	if (retval) {
		com_err("e2fsck", retval,
			_("while trying to initialize program"));
		exit(FSCK_ERROR);
	}
	reserve_stdio_fds();

	set_up_logging(ctx);
	if (ctx->logf) {
		int i;
		fputs("E2fsck run: ", ctx->logf);
		for (i = 0; i < argc; i++) {
			if (i)
				fputc(' ', ctx->logf);
			fputs(argv[i], ctx->logf);
		}
		fputc('\n', ctx->logf);
	}

	init_resource_track(&ctx->global_rtrack, NULL);
	if (!(ctx->options & E2F_OPT_PREEN) || show_version_only)
		log_err(ctx, "e2fsck %s (%s)\n", my_ver_string,
			 my_ver_date);

	if (show_version_only) {
		log_err(ctx, _("\tUsing %s, %s\n"),
			error_message(EXT2_ET_BASE), lib_ver_date);
		exit(FSCK_OK);
	}

	check_mount(ctx);

	if (!(ctx->options & E2F_OPT_PREEN) &&
	    !(ctx->options & E2F_OPT_NO) &&
	    !(ctx->options & E2F_OPT_YES)) {
		if (!ctx->interactive)
			fatal_error(ctx,
				    _("need terminal for interactive repairs"));
	}
	ctx->superblock = ctx->use_superblock;

	flags = EXT2_FLAG_SKIP_MMP;
restart:
#ifdef CONFIG_TESTIO_DEBUG
	if (getenv("TEST_IO_FLAGS") || getenv("TEST_IO_BLOCK")) {
		io_ptr = test_io_manager;
		test_io_backing_manager = unix_io_manager;
	} else
#endif
		io_ptr = unix_io_manager;
	flags |= EXT2_FLAG_NOFREE_ON_ERROR;
	profile_get_boolean(ctx->profile, "options", "old_bitmaps", 0, 0,
			    &old_bitmaps);
	if (!old_bitmaps)
		flags |= EXT2_FLAG_64BITS;
	if ((ctx->options & E2F_OPT_READONLY) == 0) {
		flags |= EXT2_FLAG_RW;
		if (!(ctx->mount_flags & EXT2_MF_ISROOT &&
		      ctx->mount_flags & EXT2_MF_READONLY))
			flags |= EXT2_FLAG_EXCLUSIVE;
		if ((ctx->mount_flags & EXT2_MF_READONLY) &&
		    (ctx->options & E2F_OPT_FORCE))
			flags &= ~EXT2_FLAG_EXCLUSIVE;
	}

	retval = try_open_fs(ctx, flags, io_ptr, &fs);

	if (!ctx->superblock && !(ctx->options & E2F_OPT_PREEN) &&
	    !(ctx->flags & E2F_FLAG_SB_SPECIFIED) &&
	    ((retval == EXT2_ET_BAD_MAGIC) ||
	     (retval == EXT2_ET_CORRUPT_SUPERBLOCK) ||
	     ((retval == 0) && (retval2 = ext2fs_check_desc(fs))))) {
		if (retval) {
			pctx.errcode = retval;
			fix_problem(ctx, PR_0_OPEN_FAILED, &pctx);
		}
		if (retval2) {
			pctx.errcode = retval2;
			fix_problem(ctx, PR_0_CHECK_DESC_FAILED, &pctx);
		}
		pctx.errcode = 0;
		if (retval2 == ENOMEM || retval2 == EXT2_ET_NO_MEMORY) {
			retval = retval2;
			goto failure;
		}
		if (fs->flags & EXT2_FLAG_NOFREE_ON_ERROR) {
			ext2fs_free(fs);
			fs = NULL;
		}
		if (!fs || (fs->group_desc_count > 1)) {
			log_out(ctx, _("%s: %s trying backup blocks...\n"),
				ctx->program_name,
				retval ? _("Superblock invalid,") :
				_("Group descriptors look bad..."));
			orig_superblock = ctx->superblock;
			get_backup_sb(ctx, fs, ctx->filesystem_name, io_ptr);
			if (fs)
				ext2fs_close(fs);
			orig_retval = retval;
			retval = try_open_fs(ctx, flags, io_ptr, &fs);
			if ((orig_retval == 0) && retval != 0) {
				if (fs)
					ext2fs_close(fs);
				log_out(ctx, _("%s: %s while using the "
					       "backup blocks"),
					ctx->program_name,
					error_message(retval));
				log_out(ctx, _("%s: going back to original "
					       "superblock\n"),
					ctx->program_name);
				ctx->superblock = orig_superblock;
				retval = try_open_fs(ctx, flags, io_ptr, &fs);
			}
		}
	}
	if (((retval == EXT2_ET_UNSUPP_FEATURE) ||
	     (retval == EXT2_ET_RO_UNSUPP_FEATURE)) &&
	    fs && fs->super) {
		sb = fs->super;
		features[0] = (sb->s_feature_compat &
			       ~EXT2_LIB_FEATURE_COMPAT_SUPP);
		features[1] = (sb->s_feature_incompat &
			       ~EXT2_LIB_FEATURE_INCOMPAT_SUPP);
		features[2] = (sb->s_feature_ro_compat &
			       ~EXT2_LIB_FEATURE_RO_COMPAT_SUPP);
		if (features[0] || features[1] || features[2])
			goto print_unsupp_features;
	}
failure:
	if (retval) {
		if (orig_retval)
			retval = orig_retval;
		com_err(ctx->program_name, retval, _("while trying to open %s"),
			ctx->filesystem_name);
		if (retval == EXT2_ET_REV_TOO_HIGH) {
			log_out(ctx, _("The filesystem revision is apparently "
			       "too high for this version of e2fsck.\n"
			       "(Or the filesystem superblock "
			       "is corrupt)\n\n"));
			fix_problem(ctx, PR_0_SB_CORRUPT, &pctx);
		} else if (retval == EXT2_ET_SHORT_READ)
			log_out(ctx, _("Could this be a zero-length "
				       "partition?\n"));
		else if ((retval == EPERM) || (retval == EACCES))
			log_out(ctx, _("You must have %s access to the "
			       "filesystem or be root\n"),
			       (ctx->options & E2F_OPT_READONLY) ?
			       "r/o" : "r/w");
		else if (retval == ENXIO)
			log_out(ctx, _("Possibly non-existent or "
				       "swap device?\n"));
		else if (retval == EBUSY)
			log_out(ctx, _("Filesystem mounted or opened "
				 "exclusively by another program?\n"));
		else if (retval == ENOENT)
			log_out(ctx, _("Possibly non-existent device?\n"));
#ifdef EROFS
		else if (retval == EROFS)
			log_out(ctx, _("Disk write-protected; use the -n "
				       "option to do a read-only\n"
				       "check of the device.\n"));
#endif
		else
			fix_problem(ctx, PR_0_SB_CORRUPT, &pctx);
		fatal_error(ctx, 0);
	}
	/*
	 * We only update the master superblock because (a) paranoia;
	 * we don't want to corrupt the backup superblocks, and (b) we
	 * don't need to update the mount count and last checked
	 * fields in the backup superblock (the kernel doesn't update
	 * the backup superblocks anyway).  With newer versions of the
	 * library this flag is set by ext2fs_open2(), but we set this
	 * here just to be sure.  (No, we don't support e2fsck running
	 * with some other libext2fs than the one that it was shipped
	 * with, but just in case....)
	 */
	fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;

	if (!(ctx->flags & E2F_FLAG_GOT_DEVSIZE)) {
		__u32 blocksize = EXT2_BLOCK_SIZE(fs->super);
		int need_restart = 0;

		pctx.errcode = ext2fs_get_device_size2(ctx->filesystem_name,
						       blocksize,
						       &ctx->num_blocks);
		/*
		 * The floppy driver refuses to allow anyone else to
		 * open the device if has been opened with O_EXCL;
		 * this is unlike other block device drivers in Linux.
		 * To handle this, we close the filesystem and then
		 * reopen the filesystem after we get the device size.
		 */
		if (pctx.errcode == EBUSY) {
			ext2fs_close(fs);
			need_restart++;
			pctx.errcode =
				ext2fs_get_device_size2(ctx->filesystem_name,
							blocksize,
							&ctx->num_blocks);
		}
		if (pctx.errcode == EXT2_ET_UNIMPLEMENTED)
			ctx->num_blocks = 0;
		else if (pctx.errcode) {
			fix_problem(ctx, PR_0_GETSIZE_ERROR, &pctx);
			ctx->flags |= E2F_FLAG_ABORT;
			fatal_error(ctx, 0);
		}
		ctx->flags |= E2F_FLAG_GOT_DEVSIZE;
		if (need_restart)
			goto restart;
	}

	ctx->fs = fs;
	fs->priv_data = ctx;
	fs->now = ctx->now;
	sb = fs->super;

	if (sb->s_rev_level > E2FSCK_CURRENT_REV) {
		com_err(ctx->program_name, EXT2_ET_REV_TOO_HIGH,
			_("while trying to open %s"),
			ctx->filesystem_name);
	get_newer:
		fatal_error(ctx, _("Get a newer version of e2fsck!"));
	}

	/*
	 * Set the device name, which is used whenever we print error
	 * or informational messages to the user.
	 */
	if (ctx->device_name == 0 &&
	    (sb->s_volume_name[0] != 0)) {
		ctx->device_name = string_copy(ctx, sb->s_volume_name,
					       sizeof(sb->s_volume_name));
	}
	if (ctx->device_name == 0)
		ctx->device_name = string_copy(ctx, ctx->filesystem_name, 0);
	for (cp = ctx->device_name; *cp; cp++)
		if (isspace(*cp) || *cp == ':')
			*cp = '_';

	ehandler_init(fs->io);

	if ((fs->super->s_feature_incompat & EXT4_FEATURE_INCOMPAT_MMP) &&
	    (flags & EXT2_FLAG_SKIP_MMP)) {
		if (e2fsck_check_mmp(fs, ctx))
			fatal_error(ctx, 0);

		/*
		 * Restart in order to reopen fs but this time start mmp.
		 */
		ext2fs_close(fs);
		ctx->fs = NULL;
		flags &= ~EXT2_FLAG_SKIP_MMP;
		goto restart;
	}

	if (ctx->logf)
		fprintf(ctx->logf, "Filesystem UUID: %s\n",
			e2p_uuid2str(sb->s_uuid));

	/*
	 * Make sure the ext3 superblock fields are consistent.
	 */
	retval = e2fsck_check_ext3_journal(ctx);
	if (retval) {
		com_err(ctx->program_name, retval,
			_("while checking ext3 journal for %s"),
			ctx->device_name);
		fatal_error(ctx, 0);
	}

	/*
	 * Check to see if we need to do ext3-style recovery.  If so,
	 * do it, and then restart the fsck.
	 */
	if (sb->s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER) {
		if (ctx->options & E2F_OPT_READONLY) {
			log_out(ctx, _("Warning: skipping journal recovery "
				       "because doing a read-only filesystem "
				       "check.\n"));
			io_channel_flush(ctx->fs->io);
		} else {
			if (ctx->flags & E2F_FLAG_RESTARTED) {
				/*
				 * Whoops, we attempted to run the
				 * journal twice.  This should never
				 * happen, unless the hardware or
				 * device driver is being bogus.
				 */
				com_err(ctx->program_name, 0,
					_("unable to set superblock flags on %s\n"), ctx->device_name);
				fatal_error(ctx, 0);
			}
			retval = e2fsck_run_ext3_journal(ctx);
			if (retval) {
				com_err(ctx->program_name, retval,
				_("while recovering ext3 journal of %s"),
					ctx->device_name);
				fatal_error(ctx, 0);
			}
			ext2fs_close(ctx->fs);
			ctx->fs = 0;
			ctx->flags |= E2F_FLAG_RESTARTED;
			goto restart;
		}
	}

	/*
	 * Check for compatibility with the feature sets.  We need to
	 * be more stringent than ext2fs_open().
	 */
	features[0] = sb->s_feature_compat & ~EXT2_LIB_FEATURE_COMPAT_SUPP;
	features[1] = sb->s_feature_incompat & ~EXT2_LIB_FEATURE_INCOMPAT_SUPP;
	features[2] = (sb->s_feature_ro_compat &
		       ~EXT2_LIB_FEATURE_RO_COMPAT_SUPP);
print_unsupp_features:
	if (features[0] || features[1] || features[2]) {
		int	i, j;
		__u32	*mask = features, m;

		log_err(ctx, _("%s has unsupported feature(s):"),
			ctx->filesystem_name);

		for (i=0; i <3; i++,mask++) {
			for (j=0,m=1; j < 32; j++, m<<=1) {
				if (*mask & m)
					log_err(ctx, " %s",
						e2p_feature2string(i, m));
			}
		}
		log_err(ctx, "\n");
		goto get_newer;
	}
#ifdef ENABLE_COMPRESSION
	if (sb->s_feature_incompat & EXT2_FEATURE_INCOMPAT_COMPRESSION)
		log_err(ctx, _("%s: warning: compression support "
			       "is experimental.\n"),
			ctx->program_name);
#endif
#ifndef ENABLE_HTREE
	if (sb->s_feature_compat & EXT2_FEATURE_COMPAT_DIR_INDEX) {
		log_err(ctx, _("%s: e2fsck not compiled with HTREE support,\n\t"
			  "but filesystem %s has HTREE directories.\n"),
			ctx->program_name, ctx->device_name);
		goto get_newer;
	}
#endif

	/*
	 * If the user specified a specific superblock, presumably the
	 * master superblock has been trashed.  So we mark the
	 * superblock as dirty, so it can be written out.
	 */
	if (ctx->superblock &&
	    !(ctx->options & E2F_OPT_READONLY))
		ext2fs_mark_super_dirty(fs);

	/*
	 * Calculate the number of filesystem blocks per pagesize.  If
	 * fs->blocksize > page_size, set the number of blocks per
	 * pagesize to 1 to avoid division by zero errors.
	 */
#ifdef _SC_PAGESIZE
	sysval = sysconf(_SC_PAGESIZE);
	if (sysval > 0)
		sys_page_size = sysval;
#endif /* _SC_PAGESIZE */
	ctx->blocks_per_page = sys_page_size / fs->blocksize;
	if (ctx->blocks_per_page == 0)
		ctx->blocks_per_page = 1;

	if (ctx->superblock)
		set_latch_flags(PR_LATCH_RELOC, PRL_LATCHED, 0);
	ext2fs_mark_valid(fs);
	check_super_block(ctx);
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		fatal_error(ctx, 0);
	check_if_skip(ctx);
	check_resize_inode(ctx);
	if (bad_blocks_file)
		read_bad_blocks_file(ctx, bad_blocks_file, replace_bad_blocks);
	else if (cflag)
		read_bad_blocks_file(ctx, 0, !keep_bad_blocks); /* Test disk */
	if (ctx->flags & E2F_FLAG_SIGNAL_MASK)
		fatal_error(ctx, 0);

	/*
	 * Mark the system as valid, 'til proven otherwise
	 */
	ext2fs_mark_valid(fs);

	retval = ext2fs_read_bb_inode(fs, &fs->badblocks);
	if (retval) {
		log_out(ctx, _("%s: %s while reading bad blocks inode\n"),
			ctx->program_name, error_message(retval));
		preenhalt(ctx);
		log_out(ctx, _("This doesn't bode well, "
			       "but we'll try to go on...\n"));
	}

	/*
	 * Save the journal size in megabytes.
	 * Try and use the journal size from the backup else let e2fsck
	 * find the default journal size.
	 */
	if (sb->s_jnl_backup_type == EXT3_JNL_BACKUP_BLOCKS)
		journal_size = (sb->s_jnl_blocks[15] << (32 - 20)) |
			       (sb->s_jnl_blocks[16] >> 20);
	else
		journal_size = -1;

	if (sb->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_QUOTA) {
		/* Quotas were enabled. Do quota accounting during fsck. */
		if ((sb->s_usr_quota_inum && sb->s_grp_quota_inum) ||
		    (!sb->s_usr_quota_inum && !sb->s_grp_quota_inum))
			qtype = -1;
		else
			qtype = sb->s_usr_quota_inum ? USRQUOTA : GRPQUOTA;

		quota_init_context(&ctx->qctx, ctx->fs, qtype);
	}

	run_result = e2fsck_run(ctx);
	e2fsck_clear_progbar(ctx);

	if (ctx->flags & E2F_FLAG_JOURNAL_INODE) {
		if (fix_problem(ctx, PR_6_RECREATE_JOURNAL, &pctx)) {
			if (journal_size < 1024)
				journal_size = ext2fs_default_journal_size(ext2fs_blocks_count(fs->super));
			if (journal_size < 0) {
				fs->super->s_feature_compat &=
					~EXT3_FEATURE_COMPAT_HAS_JOURNAL;
				fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
				log_out(ctx, "%s: Couldn't determine "
					"journal size\n", ctx->program_name);
				goto no_journal;
			}
			log_out(ctx, _("Creating journal (%d blocks): "),
			       journal_size);
			fflush(stdout);
			retval = ext2fs_add_journal_inode(fs,
							  journal_size, 0);
			if (retval) {
				log_out(ctx, "%s: while trying to create "
					"journal\n", error_message(retval));
				goto no_journal;
			}
			log_out(ctx, _(" Done.\n"));
			log_out(ctx, _("\n*** journal has been re-created - "
				       "filesystem is now ext3 again ***\n"));
		}
	}
no_journal:

	if (ctx->qctx) {
		int i, needs_writeout;
		for (i = 0; i < MAXQUOTAS; i++) {
			if (qtype != -1 && qtype != i)
				continue;
			needs_writeout = 0;
			pctx.num = i;
			retval = quota_compare_and_update(ctx->qctx, i,
							  &needs_writeout);
			if ((retval || needs_writeout) &&
			    fix_problem(ctx, PR_6_UPDATE_QUOTAS, &pctx))
				quota_write_inode(ctx->qctx, i);
		}
		quota_release_context(&ctx->qctx);
	}

	if (run_result == E2F_FLAG_RESTART) {
		log_out(ctx, _("Restarting e2fsck from the beginning...\n"));
		retval = e2fsck_reset_context(ctx);
		if (retval) {
			com_err(ctx->program_name, retval,
				_("while resetting context"));
			fatal_error(ctx, 0);
		}
		ext2fs_close(fs);
		goto restart;
	}
	if (run_result & E2F_FLAG_CANCEL) {
		log_out(ctx, _("%s: e2fsck canceled.\n"), ctx->device_name ?
			ctx->device_name : ctx->filesystem_name);
		exit_value |= FSCK_CANCELED;
	}
	if (run_result & E2F_FLAG_ABORT)
		fatal_error(ctx, _("aborted"));
	if (check_backup_super_block(ctx)) {
		fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
		ext2fs_mark_super_dirty(fs);
	}

#ifdef MTRACE
	mtrace_print("Cleanup");
#endif
	if (ext2fs_test_changed(fs)) {
		exit_value |= FSCK_NONDESTRUCT;
		if (!(ctx->options & E2F_OPT_PREEN))
			log_out(ctx, _("\n%s: ***** FILE SYSTEM WAS "
				       "MODIFIED *****\n"),
				ctx->device_name);
		if (ctx->mount_flags & EXT2_MF_ISROOT) {
			log_out(ctx, _("%s: ***** REBOOT LINUX *****\n"),
				ctx->device_name);
			exit_value |= FSCK_REBOOT;
		}
	}
	if (!ext2fs_test_valid(fs) ||
	    ((exit_value & FSCK_CANCELED) &&
	     (sb->s_state & EXT2_ERROR_FS))) {
		log_out(ctx, _("\n%s: ********** WARNING: Filesystem still has "
			       "errors **********\n\n"), ctx->device_name);
		exit_value |= FSCK_UNCORRECTED;
		exit_value &= ~FSCK_NONDESTRUCT;
	}
	if (exit_value & FSCK_CANCELED) {
		int	allow_cancellation;

		profile_get_boolean(ctx->profile, "options",
				    "allow_cancellation", 0, 0,
				    &allow_cancellation);
		exit_value &= ~FSCK_NONDESTRUCT;
		if (allow_cancellation && ext2fs_test_valid(fs) &&
		    (sb->s_state & EXT2_VALID_FS) &&
		    !(sb->s_state & EXT2_ERROR_FS))
			exit_value = 0;
	} else {
		show_stats(ctx);
		if (!(ctx->options & E2F_OPT_READONLY)) {
			if (ext2fs_test_valid(fs)) {
				if (!(sb->s_state & EXT2_VALID_FS))
					exit_value |= FSCK_NONDESTRUCT;
				sb->s_state = EXT2_VALID_FS;
			} else
				sb->s_state &= ~EXT2_VALID_FS;
			sb->s_mnt_count = 0;
			if (!(ctx->flags & E2F_FLAG_TIME_INSANE))
				sb->s_lastcheck = ctx->now;
			memset(((char *) sb) + EXT4_S_ERR_START, 0,
			       EXT4_S_ERR_LEN);
			ext2fs_mark_super_dirty(fs);
		}
	}

	if ((run_result & E2F_FLAG_CANCEL) == 0 &&
	    sb->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM &&
	    !(ctx->options & E2F_OPT_READONLY)) {
		retval = ext2fs_set_gdt_csum(ctx->fs);
		if (retval) {
			com_err(ctx->program_name, retval,
				_("while setting block group checksum info"));
			fatal_error(ctx, 0);
		}
	}

	e2fsck_write_bitmaps(ctx);
	io_channel_flush(ctx->fs->io);
	print_resource_track(ctx, NULL, &ctx->global_rtrack, ctx->fs->io);

	ext2fs_close(fs);
	ctx->fs = NULL;
	free(ctx->journal_name);

	e2fsck_free_context(ctx);
	remove_error_table(&et_ext2_error_table);
	remove_error_table(&et_prof_error_table);
	return exit_value;
}
