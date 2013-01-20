/*
 * util.c --- helper functions used by tune2fs and mke2fs
 *
 * Copyright 1995, 1996, 1997, 1998, 1999, 2000 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LINUX_MAJOR_H
#include <linux/major.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include "et/com_err.h"
#include "e2p/e2p.h"
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "nls-enable.h"
#include "blkid/blkid.h"
#include "util.h"

#ifndef HAVE_STRCASECMP
int strcasecmp (char *s1, char *s2)
{
	while (*s1 && *s2) {
		int ch1 = *s1++, ch2 = *s2++;
		if (isupper (ch1))
			ch1 = tolower (ch1);
		if (isupper (ch2))
			ch2 = tolower (ch2);
		if (ch1 != ch2)
			return ch1 - ch2;
	}
	return *s1 ? 1 : *s2 ? -1 : 0;
}
#endif

/*
 * Given argv[0], return the program name.
 */
char *get_progname(char *argv_zero)
{
	char	*cp;

	cp = strrchr(argv_zero, '/');
	if (!cp )
		return argv_zero;
	else
		return cp+1;
}

void proceed_question(void)
{
	char buf[256];
	const char *short_yes = _("yY");

	fflush(stdout);
	fflush(stderr);
	fputs(_("Proceed anyway? (y,n) "), stdout);
	buf[0] = 0;
	if (!fgets(buf, sizeof(buf), stdin) ||
	    strchr(short_yes, buf[0]) == 0)
		exit(1);
}

void check_plausibility(const char *device)
{
	int val;
#ifdef HAVE_OPEN64
	struct stat64 s;

	val = stat64(device, &s);
#else
	struct stat s;

	val = stat(device, &s);
#endif

	if(val == -1) {
		fprintf(stderr, _("Could not stat %s --- %s\n"),
			device, error_message(errno));
		if (errno == ENOENT)
			fputs(_("\nThe device apparently does not exist; "
				"did you specify it correctly?\n"), stderr);
		exit(1);
	}
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	/* On FreeBSD, all disk devices are character specials */
	if (!S_ISBLK(s.st_mode) && !S_ISCHR(s.st_mode))
#else
	if (!S_ISBLK(s.st_mode))
#endif
	{
		printf(_("%s is not a block special device.\n"), device);
		proceed_question();
		return;
	}

#ifdef HAVE_LINUX_MAJOR_H
#ifndef MAJOR
#define MAJOR(dev)	((dev)>>8)
#define MINOR(dev)	((dev) & 0xff)
#endif
#ifndef SCSI_BLK_MAJOR
#ifdef SCSI_DISK0_MAJOR
#ifdef SCSI_DISK8_MAJOR
#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR || \
  ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR) || \
  ((M) >= SCSI_DISK8_MAJOR && (M) <= SCSI_DISK15_MAJOR))
#else
#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR || \
  ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR))
#endif /* defined(SCSI_DISK8_MAJOR) */
#define SCSI_BLK_MAJOR(M) (SCSI_DISK_MAJOR((M)) || (M) == SCSI_CDROM_MAJOR)
#else
#define SCSI_BLK_MAJOR(M)  ((M) == SCSI_DISK_MAJOR || (M) == SCSI_CDROM_MAJOR)
#endif /* defined(SCSI_DISK0_MAJOR) */
#endif /* defined(SCSI_BLK_MAJOR) */
	if (((MAJOR(s.st_rdev) == HD_MAJOR &&
	      MINOR(s.st_rdev)%64 == 0) ||
	     (SCSI_BLK_MAJOR(MAJOR(s.st_rdev)) &&
	      MINOR(s.st_rdev)%16 == 0))) {
		printf(_("%s is entire device, not just one partition!\n"),
		       device);
		proceed_question();
	}
#endif
}

void check_mount(const char *device, int force, const char *type)
{
	errcode_t	retval;
	int		mount_flags;

	retval = ext2fs_check_if_mounted(device, &mount_flags);
	if (retval) {
		com_err("ext2fs_check_if_mount", retval,
			_("while determining whether %s is mounted."),
			device);
		return;
	}
	if (mount_flags & EXT2_MF_MOUNTED) {
		fprintf(stderr, _("%s is mounted; "), device);
		if (force > 2) {
			fputs(_("mke2fs forced anyway.  Hope /etc/mtab is "
				"incorrect.\n"), stderr);
			return;
		}
	abort_mke2fs:
		fprintf(stderr, _("will not make a %s here!\n"), type);
		exit(1);
	}
	if (mount_flags & EXT2_MF_BUSY) {
		fprintf(stderr, _("%s is apparently in use by the system; "),
			device);
		if (force > 2) {
			fputs(_("mke2fs forced anyway.\n"), stderr);
			return;
		}
		goto abort_mke2fs;
	}
}

void parse_journal_opts(const char *opts)
{
	char	*buf, *token, *next, *p, *arg;
	int	len;
	int	journal_usage = 0;

	len = strlen(opts);
	buf = malloc(len+1);
	if (!buf) {
		fputs(_("Couldn't allocate memory to parse journal "
			"options!\n"), stderr);
		exit(1);
	}
	strcpy(buf, opts);
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
#if 0
		printf("Journal option=%s, argument=%s\n", token,
		       arg ? arg : "NONE");
#endif
		if (strcmp(token, "device") == 0) {
			journal_device = blkid_get_devname(NULL, arg, NULL);
			if (!journal_device) {
				if (arg)
					fprintf(stderr, _("\nCould not find "
						"journal device matching %s\n"),
						arg);
				journal_usage++;
				continue;
			}
		} else if (strcmp(token, "size") == 0) {
			if (!arg) {
				journal_usage++;
				continue;
			}
			journal_size = strtoul(arg, &p, 0);
			if (*p)
				journal_usage++;
		} else if (strcmp(token, "v1_superblock") == 0) {
			journal_flags |= EXT2_MKJOURNAL_V1_SUPER;
			continue;
		} else
			journal_usage++;
	}
	if (journal_usage) {
		fputs(_("\nBad journal options specified.\n\n"
			"Journal options are separated by commas, "
			"and may take an argument which\n"
			"\tis set off by an equals ('=') sign.\n\n"
			"Valid journal options are:\n"
			"\tsize=<journal size in megabytes>\n"
			"\tdevice=<journal device>\n\n"
			"The journal size must be between "
			"1024 and 10240000 filesystem blocks.\n\n"), stderr);
		free(buf);
		exit(1);
	}
	free(buf);
}

/*
 * Determine the number of journal blocks to use, either via
 * user-specified # of megabytes, or via some intelligently selected
 * defaults.
 *
 * Find a reasonable journal file size (in blocks) given the number of blocks
 * in the filesystem.  For very small filesystems, it is not reasonable to
 * have a journal that fills more than half of the filesystem.
 */
unsigned int figure_journal_size(int size, ext2_filsys fs)
{
	int j_blocks;

	j_blocks = ext2fs_default_journal_size(fs->super->s_blocks_count);
	if (j_blocks < 0) {
		fputs(_("\nFilesystem too small for a journal\n"), stderr);
		return 0;
	}

	if (size > 0) {
		j_blocks = size * 1024 / (fs->blocksize	/ 1024);
		if (j_blocks < 1024 || j_blocks > 10240000) {
			fprintf(stderr, _("\nThe requested journal "
				"size is %d blocks; it must be\n"
				"between 1024 and 10240000 blocks.  "
				"Aborting.\n"),
				j_blocks);
			exit(1);
		}
		if ((unsigned) j_blocks > fs->super->s_free_blocks_count / 2) {
			fputs(_("\nJournal size too big for filesystem.\n"),
			      stderr);
			exit(1);
		}
	}
	return j_blocks;
}

void print_check_message(ext2_filsys fs)
{
	printf(_("This filesystem will be automatically "
		 "checked every %d mounts or\n"
		 "%g days, whichever comes first.  "
		 "Use tune2fs -c or -i to override.\n"),
	       fs->super->s_max_mnt_count,
	       (double)fs->super->s_checkinterval / (3600 * 24));
}
