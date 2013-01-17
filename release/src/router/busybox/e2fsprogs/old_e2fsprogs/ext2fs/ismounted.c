/* vi: set sw=4 ts=4: */
/*
 * ismounted.c --- Check to see if the filesystem was mounted
 *
 * Copyright (C) 1995,1996,1997,1998,1999,2000 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#ifdef HAVE_LINUX_FD_H
#include <linux/fd.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#ifdef HAVE_GETMNTINFO
#include <paths.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif /* HAVE_GETMNTINFO */
#include <string.h>
#include <sys/stat.h>

#include "ext2_fs.h"
#include "ext2fs.h"

#ifdef HAVE_MNTENT_H
/*
 * Helper function which checks a file in /etc/mtab format to see if a
 * filesystem is mounted.  Returns an error if the file doesn't exist
 * or can't be opened.
 */
static errcode_t check_mntent_file(const char *mtab_file, const char *file,
				   int *mount_flags, char *mtpt, int mtlen)
{
	struct mntent	*mnt;
	struct stat	st_buf;
	errcode_t	retval = 0;
	dev_t		file_dev=0, file_rdev=0;
	ino_t		file_ino=0;
	FILE		*f;
	int		fd;

	*mount_flags = 0;
	if ((f = setmntent (mtab_file, "r")) == NULL)
		return errno;
	if (stat(file, &st_buf) == 0) {
		if (S_ISBLK(st_buf.st_mode)) {
#ifndef __GNU__ /* The GNU hurd is broken with respect to stat devices */
			file_rdev = st_buf.st_rdev;
#endif  /* __GNU__ */
		} else {
			file_dev = st_buf.st_dev;
			file_ino = st_buf.st_ino;
		}
	}
	while ((mnt = getmntent (f)) != NULL) {
		if (strcmp(file, mnt->mnt_fsname) == 0)
			break;
		if (stat(mnt->mnt_fsname, &st_buf) == 0) {
			if (S_ISBLK(st_buf.st_mode)) {
#ifndef __GNU__
				if (file_rdev && (file_rdev == st_buf.st_rdev))
					break;
#endif  /* __GNU__ */
			} else {
				if (file_dev && ((file_dev == st_buf.st_dev) &&
						 (file_ino == st_buf.st_ino)))
					break;
			}
		}
	}

	if (mnt == 0) {
#ifndef __GNU__ /* The GNU hurd is broken with respect to stat devices */
		/*
		 * Do an extra check to see if this is the root device.  We
		 * can't trust /etc/mtab, and /proc/mounts will only list
		 * /dev/root for the root filesystem.  Argh.  Instead we
		 * check if the given device has the same major/minor number
		 * as the device that the root directory is on.
		 */
		if (file_rdev && stat("/", &st_buf) == 0) {
			if (st_buf.st_dev == file_rdev) {
				*mount_flags = EXT2_MF_MOUNTED;
				if (mtpt)
					strncpy(mtpt, "/", mtlen);
				goto is_root;
			}
		}
#endif  /* __GNU__ */
		goto errout;
	}
#ifndef __GNU__ /* The GNU hurd is deficient; what else is new? */
	/* Validate the entry in case /etc/mtab is out of date */
	/*
	 * We need to be paranoid, because some broken distributions
	 * (read: Slackware) don't initialize /etc/mtab before checking
	 * all of the non-root filesystems on the disk.
	 */
	if (stat(mnt->mnt_dir, &st_buf) < 0) {
		retval = errno;
		if (retval == ENOENT) {
#ifdef DEBUG
			printf("Bogus entry in %s!  (%s does not exist)\n",
			       mtab_file, mnt->mnt_dir);
#endif /* DEBUG */
			retval = 0;
		}
		goto errout;
	}
	if (file_rdev && (st_buf.st_dev != file_rdev)) {
#ifdef DEBUG
		printf("Bogus entry in %s!  (%s not mounted on %s)\n",
		       mtab_file, file, mnt->mnt_dir);
#endif /* DEBUG */
		goto errout;
	}
#endif /* __GNU__ */
	*mount_flags = EXT2_MF_MOUNTED;

#ifdef MNTOPT_RO
	/* Check to see if the ro option is set */
	if (hasmntopt(mnt, MNTOPT_RO))
		*mount_flags |= EXT2_MF_READONLY;
#endif

	if (mtpt)
		strncpy(mtpt, mnt->mnt_dir, mtlen);
	/*
	 * Check to see if we're referring to the root filesystem.
	 * If so, do a manual check to see if we can open /etc/mtab
	 * read/write, since if the root is mounted read/only, the
	 * contents of /etc/mtab may not be accurate.
	 */
	if (LONE_CHAR(mnt->mnt_dir, '/')) {
is_root:
#define TEST_FILE "/.ismount-test-file"
		*mount_flags |= EXT2_MF_ISROOT;
		fd = open(TEST_FILE, O_RDWR|O_CREAT);
		if (fd < 0) {
			if (errno == EROFS)
				*mount_flags |= EXT2_MF_READONLY;
		} else
			close(fd);
		(void) unlink(TEST_FILE);
	}
	retval = 0;
errout:
	endmntent (f);
	return retval;
}

static errcode_t check_mntent(const char *file, int *mount_flags,
			      char *mtpt, int mtlen)
{
	errcode_t	retval;

#ifdef DEBUG
	retval = check_mntent_file("/tmp/mtab", file, mount_flags,
				   mtpt, mtlen);
	if (retval == 0)
		return 0;
#endif /* DEBUG */
#ifdef __linux__
	retval = check_mntent_file("/proc/mounts", file, mount_flags,
				   mtpt, mtlen);
	if (retval == 0 && (*mount_flags != 0))
		return 0;
#endif /* __linux__ */
#if defined(MOUNTED) || defined(_PATH_MOUNTED)
#ifndef MOUNTED
#define MOUNTED _PATH_MOUNTED
#endif /* MOUNTED */
	retval = check_mntent_file(MOUNTED, file, mount_flags, mtpt, mtlen);
	return retval;
#else
	*mount_flags = 0;
	return 0;
#endif /* defined(MOUNTED) || defined(_PATH_MOUNTED) */
}

#else
#if defined(HAVE_GETMNTINFO)

static errcode_t check_getmntinfo(const char *file, int *mount_flags,
				  char *mtpt, int mtlen)
{
	struct statfs *mp;
	int    len, n;
	const  char   *s1;
	char	*s2;

	n = getmntinfo(&mp, MNT_NOWAIT);
	if (n == 0)
		return errno;

	len = sizeof(_PATH_DEV) - 1;
	s1 = file;
	if (strncmp(_PATH_DEV, s1, len) == 0)
		s1 += len;

	*mount_flags = 0;
	while (--n >= 0) {
		s2 = mp->f_mntfromname;
		if (strncmp(_PATH_DEV, s2, len) == 0) {
			s2 += len - 1;
			*s2 = 'r';
		}
		if (strcmp(s1, s2) == 0 || strcmp(s1, &s2[1]) == 0) {
			*mount_flags = EXT2_MF_MOUNTED;
			break;
		}
		++mp;
	}
	if (mtpt)
		strncpy(mtpt, mp->f_mntonname, mtlen);
	return 0;
}
#endif /* HAVE_GETMNTINFO */
#endif /* HAVE_MNTENT_H */

/*
 * Check to see if we're dealing with the swap device.
 */
static int is_swap_device(const char *file)
{
	FILE		*f;
	char		buf[1024], *cp;
	dev_t		file_dev;
	struct stat	st_buf;
	int		ret = 0;

	file_dev = 0;
#ifndef __GNU__ /* The GNU hurd is broken with respect to stat devices */
	if ((stat(file, &st_buf) == 0) &&
	    S_ISBLK(st_buf.st_mode))
		file_dev = st_buf.st_rdev;
#endif  /* __GNU__ */

	if (!(f = fopen_for_read("/proc/swaps")))
		return 0;
	/* Skip the first line */
	fgets(buf, sizeof(buf), f);
	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		if ((cp = strchr(buf, ' ')) != NULL)
			*cp = 0;
		if ((cp = strchr(buf, '\t')) != NULL)
			*cp = 0;
		if (strcmp(buf, file) == 0) {
			ret++;
			break;
		}
#ifndef __GNU__
		if (file_dev && (stat(buf, &st_buf) == 0) &&
		    S_ISBLK(st_buf.st_mode) &&
		    file_dev == st_buf.st_rdev) {
			ret++;
			break;
		}
#endif  /* __GNU__ */
	}
	fclose(f);
	return ret;
}


/*
 * ext2fs_check_mount_point() returns 1 if the device is mounted, 0
 * otherwise.  If mtpt is non-NULL, the directory where the device is
 * mounted is copied to where mtpt is pointing, up to mtlen
 * characters.
 */
#ifdef __TURBOC__
# pragma argsused
#endif
errcode_t ext2fs_check_mount_point(const char *device, int *mount_flags,
				  char *mtpt, int mtlen)
{
	if (is_swap_device(device)) {
		*mount_flags = EXT2_MF_MOUNTED | EXT2_MF_SWAP;
		strncpy(mtpt, "<swap>", mtlen);
		return 0;
	}
#ifdef HAVE_MNTENT_H
	return check_mntent(device, mount_flags, mtpt, mtlen);
#else
#ifdef HAVE_GETMNTINFO
	return check_getmntinfo(device, mount_flags, mtpt, mtlen);
#else
#ifdef __GNUC__
 #warning "Can't use getmntent or getmntinfo to check for mounted filesystems!"
#endif
	*mount_flags = 0;
	return 0;
#endif /* HAVE_GETMNTINFO */
#endif /* HAVE_MNTENT_H */
}

/*
 * ext2fs_check_if_mounted() sets the mount_flags EXT2_MF_MOUNTED,
 * EXT2_MF_READONLY, and EXT2_MF_ROOT
 *
 */
errcode_t ext2fs_check_if_mounted(const char *file, int *mount_flags)
{
	return ext2fs_check_mount_point(file, mount_flags, NULL, 0);
}

#ifdef DEBUG
int main(int argc, char **argv)
{
	int	retval, mount_flags;
	char	mntpt[80];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s device\n", argv[0]);
		exit(1);
	}

	mntpt[0] = 0;
	retval = ext2fs_check_mount_point(argv[1], &mount_flags,
					  mntpt, sizeof(mntpt));
	if (retval) {
		com_err(argv[0], retval,
			"while calling ext2fs_check_if_mounted");
		exit(1);
	}
	printf("Device %s reports flags %02x\n", argv[1], mount_flags);
	if (mount_flags & EXT2_MF_BUSY)
		printf("\t%s is apparently in use.\n", argv[1]);
	if (mount_flags & EXT2_MF_MOUNTED)
		printf("\t%s is mounted.\n", argv[1]);
	if (mount_flags & EXT2_MF_SWAP)
		printf("\t%s is a swap device.\n", argv[1]);
	if (mount_flags & EXT2_MF_READONLY)
		printf("\t%s is read-only.\n", argv[1]);
	if (mount_flags & EXT2_MF_ISROOT)
		printf("\t%s is the root filesystem.\n", argv[1]);
	if (mntpt[0])
		printf("\t%s is mounted on %s.\n", argv[1], mntpt);
	exit(0);
}
#endif /* DEBUG */
