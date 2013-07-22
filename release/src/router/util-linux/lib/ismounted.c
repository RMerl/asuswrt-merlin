/*
 * ismounted.c --- Check to see if the filesystem was mounted
 *
 * Copyright (C) 1995,1996,1997,1998,1999,2000,2008 Theodore Ts'o.
 *
 * This file may be redistributed under the terms of the GNU Public
 * License.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/param.h>
#ifdef __APPLE__
#include <sys/ucred.h>
#include <sys/mount.h>
#endif

#include "pathnames.h"
#include "ismounted.h"
#include "c.h"

#ifdef HAVE_MNTENT_H
/*
 * Helper function which checks a file in /etc/mtab format to see if a
 * filesystem is mounted.  Returns an error if the file doesn't exist
 * or can't be opened.
 */
static int check_mntent_file(const char *mtab_file, const char *file,
				   int *mount_flags, char *mtpt, int mtlen)
{
	struct mntent	*mnt;
	struct stat	st_buf;
	int		retval = 0;
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
#endif	/* __GNU__ */
		} else {
			file_dev = st_buf.st_dev;
			file_ino = st_buf.st_ino;
		}
	}
	while ((mnt = getmntent (f)) != NULL) {
		if (mnt->mnt_fsname[0] != '/')
			continue;
		if (strcmp(file, mnt->mnt_fsname) == 0)
			break;
		if (stat(mnt->mnt_fsname, &st_buf) == 0) {
			if (S_ISBLK(st_buf.st_mode)) {
#ifndef __GNU__
				if (file_rdev && (file_rdev == st_buf.st_rdev))
					break;
#endif	/* __GNU__ */
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
		if (file_rdev && stat("/", &st_buf) == 0 &&
		    st_buf.st_dev == file_rdev) {
			*mount_flags = MF_MOUNTED;
			if (mtpt)
				strncpy(mtpt, "/", mtlen);
			goto is_root;
		}
#endif	/* __GNU__ */
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
	*mount_flags = MF_MOUNTED;

#ifdef MNTOPT_RO
	/* Check to see if the ro option is set */
	if (hasmntopt(mnt, MNTOPT_RO))
		*mount_flags |= MF_READONLY;
#endif

	if (mtpt)
		strncpy(mtpt, mnt->mnt_dir, mtlen);
	/*
	 * Check to see if we're referring to the root filesystem.
	 * If so, do a manual check to see if we can open /etc/mtab
	 * read/write, since if the root is mounted read/only, the
	 * contents of /etc/mtab may not be accurate.
	 */
	if (!strcmp(mnt->mnt_dir, "/")) {
is_root:
#define TEST_FILE "/.ismount-test-file"
		*mount_flags |= MF_ISROOT;
		fd = open(TEST_FILE, O_RDWR|O_CREAT, 0600);
		if (fd < 0) {
			if (errno == EROFS)
				*mount_flags |= MF_READONLY;
		} else
			close(fd);
		(void) unlink(TEST_FILE);
	}
	retval = 0;
errout:
	endmntent (f);
	return retval;
}

static int check_mntent(const char *file, int *mount_flags,
			      char *mtpt, int mtlen)
{
	int	retval;

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
	if (access("/proc/mounts", R_OK) == 0) {
		*mount_flags = 0;
		return retval;
	}
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

static int check_getmntinfo(const char *file, int *mount_flags,
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
			*mount_flags = MF_MOUNTED;
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
#endif	/* __GNU__ */

	if (!(f = fopen("/proc/swaps", "r")))
		return 0;
	/* Skip the first line */
	if (!fgets(buf, sizeof(buf), f))
		goto leave;
	if (*buf && strncmp(buf, "Filename\t", 9))
		/* Linux <=2.6.19 contained a bug in the /proc/swaps
		 * code where the header would not be displayed
		 */
		goto valid_first_line;

	while (fgets(buf, sizeof(buf), f)) {
valid_first_line:
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
#endif	/* __GNU__ */
	}

leave:
	fclose(f);
	return ret;
}


/*
 * check_mount_point() fills determines if the device is mounted or otherwise
 * busy, and fills in mount_flags with one or more of the following flags:
 * MF_MOUNTED, MF_ISROOT, MF_READONLY, MF_SWAP, and MF_BUSY.  If mtpt is
 * non-NULL, the directory where the device is mounted is copied to where mtpt
 * is pointing, up to mtlen characters.
 */
#ifdef __TURBOC__
 #pragma argsused
#endif
int check_mount_point(const char *device, int *mount_flags,
				  char *mtpt, int mtlen)
{
	struct stat	st_buf;
	int	retval = 0;
	int		fd;

	if (is_swap_device(device)) {
		*mount_flags = MF_MOUNTED | MF_SWAP;
		strncpy(mtpt, "<swap>", mtlen);
	} else {
#ifdef HAVE_MNTENT_H
		retval = check_mntent(device, mount_flags, mtpt, mtlen);
#else
#ifdef HAVE_GETMNTINFO
		retval = check_getmntinfo(device, mount_flags, mtpt, mtlen);
#else
#ifdef __GNUC__
 #warning "Can't use getmntent or getmntinfo to check for mounted filesystems!"
#endif
		*mount_flags = 0;
#endif /* HAVE_GETMNTINFO */
#endif /* HAVE_MNTENT_H */
	}
	if (retval)
		return retval;

#ifdef __linux__ /* This only works on Linux 2.6+ systems */
	if ((stat(device, &st_buf) != 0) ||
	    !S_ISBLK(st_buf.st_mode))
		return 0;
	fd = open(device, O_RDONLY | O_EXCL);
	if (fd < 0) {
		if (errno == EBUSY)
			*mount_flags |= MF_BUSY;
	} else
		close(fd);
#endif

	return 0;
}

int is_mounted(const char *file)
{
	int	retval;
	int	mount_flags = 0;

	retval = check_mount_point(file, &mount_flags, NULL, 0);
	if (retval)
		return 0;
	return mount_flags & MF_MOUNTED;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	int flags = 0;
	char devname[PATH_MAX];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s device\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (check_mount_point(argv[1], &flags, devname, sizeof(devname)) == 0 &&
	    (flags & MF_MOUNTED)) {
		if (flags & MF_SWAP)
			printf("used swap device\n");
		else
			printf("mounted on %s\n", devname);
		return EXIT_SUCCESS;
	}

	printf("not mounted\n");
	return EXIT_FAILURE;
}
#endif /* DEBUG */
