/*
 * getsize.c --- get the size of a partition.
 *
 * Copyright (C) 1995, 1995 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

/* include this before sys/queues.h! */
#include "blkidP.h"

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_LINUX_FD_H
#include <linux/fd.h>
#endif
#ifdef HAVE_SYS_DISKLABEL_H
#include <sys/disklabel.h>
#endif
#ifdef HAVE_SYS_DISK_H
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h> /* for LIST_HEAD */
#endif
#include <sys/disk.h>
#endif
#ifdef __linux__
#include <sys/utsname.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif


#if defined(__linux__) && defined(_IO) && !defined(BLKGETSIZE)
#define BLKGETSIZE _IO(0x12,96)	/* return device size */
#endif

#if defined(__linux__) && defined(_IOR) && !defined(BLKGETSIZE64)
#define BLKGETSIZE64 _IOR(0x12,114,size_t)	/* return device size in bytes (u64 *arg) */
#endif

#ifdef APPLE_DARWIN
#define BLKGETSIZE DKIOCGETBLOCKCOUNT32
#endif /* APPLE_DARWIN */

static int valid_offset(int fd, blkid_loff_t offset)
{
	char ch;

	if (blkid_llseek(fd, offset, 0) < 0)
		return 0;
	if (read(fd, &ch, 1) < 1)
		return 0;
	return 1;
}

/*
 * Returns the number of bytes in a partition
 */
blkid_loff_t blkid_get_dev_size(int fd)
{
	int valid_blkgetsize64 = 1;
#ifdef __linux__
	struct 		utsname ut;
#endif
	unsigned long long size64;
	unsigned long size;
	blkid_loff_t high, low;
#ifdef FDGETPRM
	struct floppy_struct this_floppy;
#endif
#ifdef HAVE_SYS_DISKLABEL_H
	int part = -1;
	struct disklabel lab;
	struct partition *pp;
	char ch;
	struct stat st;
#endif /* HAVE_SYS_DISKLABEL_H */

#ifdef DKIOCGETBLOCKCOUNT	/* For Apple Darwin */
	if (ioctl(fd, DKIOCGETBLOCKCOUNT, &size64) >= 0) {
		if ((sizeof(blkid_loff_t) < sizeof(unsigned long long))
		    && (size64 << 9 > 0xFFFFFFFF))
			return 0; /* EFBIG */
		return (blkid_loff_t) size64 << 9;
	}
#endif

#ifdef BLKGETSIZE64
#ifdef __linux__
	if ((uname(&ut) == 0) &&
	    ((ut.release[0] == '2') && (ut.release[1] == '.') &&
	     (ut.release[2] < '6') && (ut.release[3] == '.')))
		valid_blkgetsize64 = 0;
#endif
	if (valid_blkgetsize64 &&
	    ioctl(fd, BLKGETSIZE64, &size64) >= 0) {
		if ((sizeof(blkid_loff_t) < sizeof(unsigned long long))
		    && ((size64) > 0xFFFFFFFF))
			return 0; /* EFBIG */
		return size64;
	}
#endif

#ifdef BLKGETSIZE
	if (ioctl(fd, BLKGETSIZE, &size) >= 0)
		return (blkid_loff_t)size << 9;
#endif

/* tested on FreeBSD 6.1-RELEASE i386 */
#ifdef DIOCGMEDIASIZE
	if (ioctl(fd, DIOCGMEDIASIZE, &size64) >= 0)
		return (off_t)size64;
#endif /* DIOCGMEDIASIZE */

#ifdef FDGETPRM
	if (ioctl(fd, FDGETPRM, &this_floppy) >= 0)
		return (blkid_loff_t)this_floppy.size << 9;
#endif
#ifdef HAVE_SYS_DISKLABEL_H
	/*
	 * This code works for FreeBSD 4.11 i386, except for the full device
	 * (such as /dev/ad0). It doesn't work properly for newer FreeBSD
	 * though. FreeBSD >= 5.0 should be covered by the DIOCGMEDIASIZE
	 * above however.
	 *
	 * Note that FreeBSD >= 4.0 has disk devices as unbuffered (raw,
	 * character) devices, so we need to check for S_ISCHR, too.
	 */
	if ((fstat(fd, &st) >= 0) && (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)))
		part = st.st_rdev & 7;
	if (part >= 0 && (ioctl(fd, DIOCGDINFO, (char *)&lab) >= 0)) {
		pp = &lab.d_partitions[part];
		if (pp->p_size)
			return pp->p_size << 9;
	}
#endif /* HAVE_SYS_DISKLABEL_H */
	{
#ifdef HAVE_FSTAT64
		struct stat64   st;
		if (fstat64(fd, &st) == 0)
#else
		struct stat	st;
		if (fstat(fd, &st) == 0)
#endif
			if (S_ISREG(st.st_mode))
				return st.st_size;
	}


	/*
	 * OK, we couldn't figure it out by using a specialized ioctl,
	 * which is generally the best way.  So do binary search to
	 * find the size of the partition.
	 */
	low = 0;
	for (high = 1024; valid_offset(fd, high); high *= 2)
		low = high;
	while (low < high - 1)
	{
		const blkid_loff_t mid = (low + high) / 2;

		if (valid_offset(fd, mid))
			low = mid;
		else
			high = mid;
	}
	return low + 1;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	long long bytes;
	int	fd;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s device\n"
			"Determine the size of a device\n", argv[0]);
		return 1;
	}

	if ((fd = open(argv[1], O_RDONLY)) < 0)
		perror(argv[0]);

	bytes = blkid_get_dev_size(fd);
	printf("Device %s has %Ld 1k blocks.\n", argv[1],
	       (unsigned long long) bytes >> 10);

	return 0;
}
#endif
