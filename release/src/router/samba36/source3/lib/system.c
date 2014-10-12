/* 
   Unix SMB/CIFS implementation.
   Samba system utilities
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison  1998-2005
   Copyright (C) Timur Bakeyev        2005
   Copyright (C) Bjoern Jacke    2006-2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/syslog.h"
#include "system/capability.h"
#include "system/passwd.h"
#include "system/filesys.h"

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

/*
   The idea is that this file will eventually have wrappers around all
   important system calls in samba. The aims are:

   - to enable easier porting by putting OS dependent stuff in here

   - to allow for hooks into other "pseudo-filesystems"

   - to allow easier integration of things like the japanese extensions

   - to support the philosophy of Samba to expose the features of
     the OS within the SMB model. In general whatever file/printer/variable
     expansions/etc make sense to the OS should be acceptable to Samba.
*/



/*******************************************************************
 A wrapper for memalign
********************************************************************/

void *sys_memalign( size_t align, size_t size )
{
#if defined(HAVE_POSIX_MEMALIGN)
	void *p = NULL;
	int ret = posix_memalign( &p, align, size );
	if ( ret == 0 )
		return p;

	return NULL;
#elif defined(HAVE_MEMALIGN)
	return memalign( align, size );
#else
	/* On *BSD systems memaligns doesn't exist, but memory will
	 * be aligned on allocations of > pagesize. */
#if defined(SYSCONF_SC_PAGESIZE)
	size_t pagesize = (size_t)sysconf(_SC_PAGESIZE);
#elif defined(HAVE_GETPAGESIZE)
	size_t pagesize = (size_t)getpagesize();
#else
	size_t pagesize = (size_t)-1;
#endif
	if (pagesize == (size_t)-1) {
		DEBUG(0,("memalign functionalaity not available on this platform!\n"));
		return NULL;
	}
	if (size < pagesize) {
		size = pagesize;
	}
	return SMB_MALLOC(size);
#endif
}

/*******************************************************************
 A wrapper for usleep in case we don't have one.
********************************************************************/

int sys_usleep(long usecs)
{
#ifndef HAVE_USLEEP
	struct timeval tval;
#endif

	/*
	 * We need this braindamage as the glibc usleep
	 * is not SPEC1170 complient... grumble... JRA.
	 */

	if(usecs < 0 || usecs > 999999) {
		errno = EINVAL;
		return -1;
	}

#if HAVE_USLEEP
	usleep(usecs);
	return 0;
#else /* HAVE_USLEEP */
	/*
	 * Fake it with select...
	 */
	tval.tv_sec = 0;
	tval.tv_usec = usecs/1000;
	select(0,NULL,NULL,NULL,&tval);
	return 0;
#endif /* HAVE_USLEEP */
}

/*******************************************************************
A read wrapper that will deal with EINTR.
********************************************************************/

ssize_t sys_read(int fd, void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = read(fd, buf, count);
#if defined(EWOULDBLOCK)
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
#else
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
#endif
	return ret;
}

/*******************************************************************
A write wrapper that will deal with EINTR.
********************************************************************/

ssize_t sys_write(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = write(fd, buf, count);
#if defined(EWOULDBLOCK)
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
#else
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
#endif
	return ret;
}

/*******************************************************************
A writev wrapper that will deal with EINTR.
********************************************************************/

ssize_t sys_writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret;

#if 0
	/* Try to confuse write_data_iov a bit */
	if ((random() % 5) == 0) {
		return sys_write(fd, iov[0].iov_base, iov[0].iov_len);
	}
	if (iov[0].iov_len > 1) {
		return sys_write(fd, iov[0].iov_base,
				 (random() % (iov[0].iov_len-1)) + 1);
	}
#endif

	do {
		ret = writev(fd, iov, iovcnt);
#if defined(EWOULDBLOCK)
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
#else
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
#endif
	return ret;
}

/*******************************************************************
A pread wrapper that will deal with EINTR and 64-bit file offsets.
********************************************************************/

#if defined(HAVE_PREAD) || defined(HAVE_PREAD64)
ssize_t sys_pread(int fd, void *buf, size_t count, SMB_OFF_T off)
{
	ssize_t ret;

	do {
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_PREAD64)
		ret = pread64(fd, buf, count, off);
#else
		ret = pread(fd, buf, count, off);
#endif
	} while (ret == -1 && errno == EINTR);
	return ret;
}
#endif

/*******************************************************************
A write wrapper that will deal with EINTR and 64-bit file offsets.
********************************************************************/

#if defined(HAVE_PWRITE) || defined(HAVE_PWRITE64)
ssize_t sys_pwrite(int fd, const void *buf, size_t count, SMB_OFF_T off)
{
	ssize_t ret;

	do {
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_PWRITE64)
		ret = pwrite64(fd, buf, count, off);
#else
		ret = pwrite(fd, buf, count, off);
#endif
	} while (ret == -1 && errno == EINTR);
	return ret;
}
#endif

/*******************************************************************
A send wrapper that will deal with EINTR or EAGAIN or EWOULDBLOCK.
********************************************************************/

ssize_t sys_send(int s, const void *msg, size_t len, int flags)
{
	ssize_t ret;

	do {
		ret = send(s, msg, len, flags);
#if defined(EWOULDBLOCK)
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
#else
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
#endif
	return ret;
}

/*******************************************************************
A sendto wrapper that will deal with EINTR or EAGAIN or EWOULDBLOCK.
********************************************************************/

ssize_t sys_sendto(int s,  const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	ssize_t ret;

	do {
		ret = sendto(s, msg, len, flags, to, tolen);
#if defined(EWOULDBLOCK)
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
#else
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
#endif
	return ret;
}

/*******************************************************************
A recv wrapper that will deal with EINTR or EAGAIN or EWOULDBLOCK.
********************************************************************/

ssize_t sys_recv(int fd, void *buf, size_t count, int flags)
{
	ssize_t ret;

	do {
		ret = recv(fd, buf, count, flags);
#if defined(EWOULDBLOCK)
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
#else
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
#endif
	return ret;
}

/*******************************************************************
A recvfrom wrapper that will deal with EINTR.
NB. As used with non-blocking sockets, return on EAGAIN/EWOULDBLOCK
********************************************************************/

ssize_t sys_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	ssize_t ret;

	do {
		ret = recvfrom(s, buf, len, flags, from, fromlen);
	} while (ret == -1 && (errno == EINTR));
	return ret;
}

/*******************************************************************
A fcntl wrapper that will deal with EINTR.
********************************************************************/

int sys_fcntl_ptr(int fd, int cmd, void *arg)
{
	int ret;

	do {
		ret = fcntl(fd, cmd, arg);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

/*******************************************************************
A fcntl wrapper that will deal with EINTR.
********************************************************************/

int sys_fcntl_long(int fd, int cmd, long arg)
{
	int ret;

	do {
		ret = fcntl(fd, cmd, arg);
	} while (ret == -1 && errno == EINTR);
	return ret;
}

/****************************************************************************
 Get/Set all the possible time fields from a stat struct as a timespec.
****************************************************************************/

static struct timespec get_atimespec(const struct stat *pst)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	struct timespec ret;

	/* Old system - no ns timestamp. */
	ret.tv_sec = pst->st_atime;
	ret.tv_nsec = 0;
	return ret;
#else
#if defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
	return pst->st_atim;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMENSEC)
	struct timespec ret;
	ret.tv_sec = pst->st_atime;
	ret.tv_nsec = pst->st_atimensec;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_MTIME_N)
	struct timespec ret;
	ret.tv_sec = pst->st_atime;
	ret.tv_nsec = pst->st_atime_n;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_UMTIME)
	struct timespec ret;
	ret.tv_sec = pst->st_atime;
	ret.tv_nsec = pst->st_uatime * 1000;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
	return pst->st_atimespec;
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT
#endif
#endif
}

static struct timespec get_mtimespec(const struct stat *pst)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	struct timespec ret;

	/* Old system - no ns timestamp. */
	ret.tv_sec = pst->st_mtime;
	ret.tv_nsec = 0;
	return ret;
#else
#if defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
	return pst->st_mtim;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMENSEC)
	struct timespec ret;
	ret.tv_sec = pst->st_mtime;
	ret.tv_nsec = pst->st_mtimensec;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_MTIME_N)
	struct timespec ret;
	ret.tv_sec = pst->st_mtime;
	ret.tv_nsec = pst->st_mtime_n;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_UMTIME)
	struct timespec ret;
	ret.tv_sec = pst->st_mtime;
	ret.tv_nsec = pst->st_umtime * 1000;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
	return pst->st_mtimespec;
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT
#endif
#endif
}

static struct timespec get_ctimespec(const struct stat *pst)
{
#if !defined(HAVE_STAT_HIRES_TIMESTAMPS)
	struct timespec ret;

	/* Old system - no ns timestamp. */
	ret.tv_sec = pst->st_ctime;
	ret.tv_nsec = 0;
	return ret;
#else
#if defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
	return pst->st_ctim;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMENSEC)
	struct timespec ret;
	ret.tv_sec = pst->st_ctime;
	ret.tv_nsec = pst->st_ctimensec;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_MTIME_N)
	struct timespec ret;
	ret.tv_sec = pst->st_ctime;
	ret.tv_nsec = pst->st_ctime_n;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_UMTIME)
	struct timespec ret;
	ret.tv_sec = pst->st_ctime;
	ret.tv_nsec = pst->st_uctime * 1000;
	return ret;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
	return pst->st_ctimespec;
#else
#error	CONFIGURE_ERROR_IN_DETECTING_TIMESPEC_IN_STAT
#endif
#endif
}

/****************************************************************************
 Return the best approximation to a 'create time' under UNIX from a stat
 structure.
****************************************************************************/

static struct timespec calc_create_time_stat(const struct stat *st)
{
	struct timespec ret, ret1;
	struct timespec c_time = get_ctimespec(st);
	struct timespec m_time = get_mtimespec(st);
	struct timespec a_time = get_atimespec(st);

	ret = timespec_compare(&c_time, &m_time) < 0 ? c_time : m_time;
	ret1 = timespec_compare(&ret, &a_time) < 0 ? ret : a_time;

	if(!null_timespec(ret1)) {
		return ret1;
	}

	/*
	 * One of ctime, mtime or atime was zero (probably atime).
	 * Just return MIN(ctime, mtime).
	 */
	return ret;
}

/****************************************************************************
 Return the best approximation to a 'create time' under UNIX from a stat_ex
 structure.
****************************************************************************/

static struct timespec calc_create_time_stat_ex(const struct stat_ex *st)
{
	struct timespec ret, ret1;
	struct timespec c_time = st->st_ex_ctime;
	struct timespec m_time = st->st_ex_mtime;
	struct timespec a_time = st->st_ex_atime;

	ret = timespec_compare(&c_time, &m_time) < 0 ? c_time : m_time;
	ret1 = timespec_compare(&ret, &a_time) < 0 ? ret : a_time;

	if(!null_timespec(ret1)) {
		return ret1;
	}

	/*
	 * One of ctime, mtime or atime was zero (probably atime).
	 * Just return MIN(ctime, mtime).
	 */
	return ret;
}

/****************************************************************************
 Return the 'create time' from a stat struct if it exists (birthtime) or else
 use the best approximation.
****************************************************************************/

static void make_create_timespec(const struct stat *pst, struct stat_ex *dst,
				 bool fake_dir_create_times)
{
	if (S_ISDIR(pst->st_mode) && fake_dir_create_times) {
		dst->st_ex_btime.tv_sec = 315493200L;          /* 1/1/1980 */
		dst->st_ex_btime.tv_nsec = 0;
	}

	dst->st_ex_calculated_birthtime = false;

#if defined(HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC)
	dst->st_ex_btime = pst->st_birthtimespec;
#elif defined(HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC)
	dst->st_ex_btime.tv_sec = pst->st_birthtime;
	dst->st_ex_btime.tv_nsec = pst->st_birthtimenspec;
#elif defined(HAVE_STRUCT_STAT_ST_BIRTHTIME)
	dst->st_ex_btime.tv_sec = pst->st_birthtime;
	dst->st_ex_btime.tv_nsec = 0;
#else
	dst->st_ex_btime = calc_create_time_stat(pst);
	dst->st_ex_calculated_birthtime = true;
#endif

	/* Deal with systems that don't initialize birthtime correctly.
	 * Pointed out by SATOH Fumiyasu <fumiyas@osstech.jp>.
	 */
	if (null_timespec(dst->st_ex_btime)) {
		dst->st_ex_btime = calc_create_time_stat(pst);
		dst->st_ex_calculated_birthtime = true;
	}
}

/****************************************************************************
 If we update a timestamp in a stat_ex struct we may have to recalculate
 the birthtime. For now only implement this for write time, but we may
 also need to do it for atime and ctime. JRA.
****************************************************************************/

void update_stat_ex_mtime(struct stat_ex *dst,
				struct timespec write_ts)
{
	dst->st_ex_mtime = write_ts;

	/* We may have to recalculate btime. */
	if (dst->st_ex_calculated_birthtime) {
		dst->st_ex_btime = calc_create_time_stat_ex(dst);
	}
}

void update_stat_ex_create_time(struct stat_ex *dst,
                                struct timespec create_time)
{
	dst->st_ex_btime = create_time;
	dst->st_ex_calculated_birthtime = false;
}

#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_STAT64)
static void init_stat_ex_from_stat (struct stat_ex *dst,
				    const struct stat64 *src,
				    bool fake_dir_create_times)
#else
static void init_stat_ex_from_stat (struct stat_ex *dst,
				    const struct stat *src,
				    bool fake_dir_create_times)
#endif
{
	dst->st_ex_dev = src->st_dev;
	dst->st_ex_ino = src->st_ino;
	dst->st_ex_mode = src->st_mode;
	dst->st_ex_nlink = src->st_nlink;
	dst->st_ex_uid = src->st_uid;
	dst->st_ex_gid = src->st_gid;
	dst->st_ex_rdev = src->st_rdev;
	dst->st_ex_size = src->st_size;
	dst->st_ex_atime = get_atimespec(src);
	dst->st_ex_mtime = get_mtimespec(src);
	dst->st_ex_ctime = get_ctimespec(src);
	make_create_timespec(src, dst, fake_dir_create_times);
#ifdef HAVE_STAT_ST_BLKSIZE
	dst->st_ex_blksize = src->st_blksize;
#else
	dst->st_ex_blksize = STAT_ST_BLOCKSIZE;
#endif

#ifdef HAVE_STAT_ST_BLOCKS
	dst->st_ex_blocks = src->st_blocks;
#else
	dst->st_ex_blocks = src->st_size / dst->st_ex_blksize + 1;
#endif

#ifdef HAVE_STAT_ST_FLAGS
	dst->st_ex_flags = src->st_flags;
#else
	dst->st_ex_flags = 0;
#endif
}

/*******************************************************************
A stat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_stat(const char *fname, SMB_STRUCT_STAT *sbuf,
	     bool fake_dir_create_times)
{
	int ret;
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_STAT64)
	struct stat64 statbuf;
	ret = stat64(fname, &statbuf);
#else
	struct stat statbuf;
	ret = stat(fname, &statbuf);
#endif
	if (ret == 0) {
		/* we always want directories to appear zero size */
		if (S_ISDIR(statbuf.st_mode)) {
			statbuf.st_size = 0;
		}
		init_stat_ex_from_stat(sbuf, &statbuf, fake_dir_create_times);
	}
	return ret;
}

/*******************************************************************
 An fstat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_fstat(int fd, SMB_STRUCT_STAT *sbuf, bool fake_dir_create_times)
{
	int ret;
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_FSTAT64)
	struct stat64 statbuf;
	ret = fstat64(fd, &statbuf);
#else
	struct stat statbuf;
	ret = fstat(fd, &statbuf);
#endif
	if (ret == 0) {
		/* we always want directories to appear zero size */
		if (S_ISDIR(statbuf.st_mode)) {
			statbuf.st_size = 0;
		}
		init_stat_ex_from_stat(sbuf, &statbuf, fake_dir_create_times);
	}
	return ret;
}

/*******************************************************************
 An lstat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_lstat(const char *fname,SMB_STRUCT_STAT *sbuf,
	      bool fake_dir_create_times)
{
	int ret;
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_LSTAT64)
	struct stat64 statbuf;
	ret = lstat64(fname, &statbuf);
#else
	struct stat statbuf;
	ret = lstat(fname, &statbuf);
#endif
	if (ret == 0) {
		/* we always want directories to appear zero size */
		if (S_ISDIR(statbuf.st_mode)) {
			statbuf.st_size = 0;
		}
		init_stat_ex_from_stat(sbuf, &statbuf, fake_dir_create_times);
	}
	return ret;
}

/*******************************************************************
 An posix_fallocate() wrapper that will deal with 64 bit filesizes.
********************************************************************/
int sys_posix_fallocate(int fd, SMB_OFF_T offset, SMB_OFF_T len)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_POSIX_FALLOCATE64) && !defined(HAVE_BROKEN_POSIX_FALLOCATE)
	return posix_fallocate64(fd, offset, len);
#elif defined(HAVE_POSIX_FALLOCATE) && !defined(HAVE_BROKEN_POSIX_FALLOCATE)
	return posix_fallocate(fd, offset, len);
#elif defined(F_RESVSP64)
	/* this handles XFS on IRIX */
	struct flock64 fl;
	SMB_OFF_T new_len = offset + len;
	int ret;
	struct stat64 sbuf;

	/* unlikely to get a too large file on a 64bit system but ... */
	if (new_len < 0)
		return EFBIG;

	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = len;

	ret=fcntl(fd, F_RESVSP64, &fl);

	if (ret != 0)
		return errno;

	/* Make sure the file gets enlarged after we allocated space: */
	fstat64(fd, &sbuf);
	if (new_len > sbuf.st_size)
		ftruncate64(fd, new_len);
	return 0;
#else
	return ENOSYS;
#endif
}

/*******************************************************************
 An fallocate() function that matches the semantics of the Linux one.
********************************************************************/

#ifdef HAVE_LINUX_FALLOC_H
#include <linux/falloc.h>
#endif

int sys_fallocate(int fd, enum vfs_fallocate_mode mode, SMB_OFF_T offset, SMB_OFF_T len)
{
#if defined(HAVE_LINUX_FALLOCATE64) || defined(HAVE_LINUX_FALLOCATE)
	int lmode;
	switch (mode) {
	case VFS_FALLOCATE_EXTEND_SIZE:
		lmode = 0;
		break;
	case VFS_FALLOCATE_KEEP_SIZE:
		lmode = FALLOC_FL_KEEP_SIZE;
		break;
	default:
		errno = EINVAL;
		return -1;
	}
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_LINUX_FALLOCATE64)
	return fallocate64(fd, lmode, offset, len);
#elif defined(HAVE_LINUX_FALLOCATE)
	return fallocate(fd, lmode, offset, len);
#endif
#else
	/* TODO - plumb in fallocate from other filesysetms like VXFS etc. JRA. */
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
 An ftruncate() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_ftruncate(int fd, SMB_OFF_T offset)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_FTRUNCATE64)
	return ftruncate64(fd, offset);
#else
	return ftruncate(fd, offset);
#endif
}

/*******************************************************************
 An lseek() wrapper that will deal with 64 bit filesizes.
********************************************************************/

SMB_OFF_T sys_lseek(int fd, SMB_OFF_T offset, int whence)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OFF64_T) && defined(HAVE_LSEEK64)
	return lseek64(fd, offset, whence);
#else
	return lseek(fd, offset, whence);
#endif
}

/*******************************************************************
 An fseek() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_fseek(FILE *fp, SMB_OFF_T offset, int whence)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FSEEK64)
	return fseek64(fp, offset, whence);
#elif defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FSEEKO64)
	return fseeko64(fp, offset, whence);
#elif defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FSEEKO)
	return fseeko(fp, offset, whence);
#else
	return fseek(fp, offset, whence);
#endif
}

/*******************************************************************
 An ftell() wrapper that will deal with 64 bit filesizes.
********************************************************************/

SMB_OFF_T sys_ftell(FILE *fp)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FTELL64)
	return (SMB_OFF_T)ftell64(fp);
#elif defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(LARGE_SMB_OFF_T) && defined(HAVE_FTELLO64)
	return (SMB_OFF_T)ftello64(fp);
#else
	return (SMB_OFF_T)ftell(fp);
#endif
}

/*******************************************************************
 A creat() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_creat(const char *path, mode_t mode)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_CREAT64)
	return creat64(path, mode);
#else
	/*
	 * If creat64 isn't defined then ensure we call a potential open64.
	 * JRA.
	 */
	return sys_open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
#endif
}

/*******************************************************************
 An open() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_open(const char *path, int oflag, mode_t mode)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OPEN64)
	return open64(path, oflag, mode);
#else
	return open(path, oflag, mode);
#endif
}

/*******************************************************************
 An fopen() wrapper that will deal with 64 bit filesizes.
********************************************************************/

FILE *sys_fopen(const char *path, const char *type)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_FOPEN64)
	return fopen64(path, type);
#else
	return fopen(path, type);
#endif
}


#if HAVE_KERNEL_SHARE_MODES
#ifndef LOCK_MAND
#define LOCK_MAND	32	/* This is a mandatory flock */
#define LOCK_READ	64	/* ... Which allows concurrent read operations */
#define LOCK_WRITE	128	/* ... Which allows concurrent write operations */
#define LOCK_RW		192	/* ... Which allows concurrent read & write ops */
#endif
#endif

/*******************************************************************
 A flock() wrapper that will perform the kernel flock.
********************************************************************/

void kernel_flock(int fd, uint32 share_mode, uint32 access_mask)
{
#if HAVE_KERNEL_SHARE_MODES
	int kernel_mode = 0;
	if (share_mode == FILE_SHARE_WRITE) {
		kernel_mode = LOCK_MAND|LOCK_WRITE;
	} else if (share_mode == FILE_SHARE_READ) {
		kernel_mode = LOCK_MAND|LOCK_READ;
	} else if (share_mode == FILE_SHARE_NONE) {
		kernel_mode = LOCK_MAND;
	}
	if (kernel_mode) {
		flock(fd, kernel_mode);
	}
#endif
	;
}



/*******************************************************************
 An opendir wrapper that will deal with 64 bit filesizes.
********************************************************************/

SMB_STRUCT_DIR *sys_opendir(const char *name)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_OPENDIR64)
	return opendir64(name);
#else
	return opendir(name);
#endif
}

/*******************************************************************
 An fdopendir wrapper.
********************************************************************/

SMB_STRUCT_DIR *sys_fdopendir(int fd)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_FDOPENDIR64)
	return fdopendir64(fd);
#elif defined(HAVE_FDOPENDIR)
	return fdopendir(fd);
#else
	errno = ENOSYS;
	return NULL;
#endif
}

/*******************************************************************
 A readdir wrapper that will deal with 64 bit filesizes.
********************************************************************/

SMB_STRUCT_DIRENT *sys_readdir(SMB_STRUCT_DIR *dirp)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_READDIR64)
	return readdir64(dirp);
#else
	return readdir(dirp);
#endif
}

/*******************************************************************
 A seekdir wrapper that will deal with 64 bit filesizes.
********************************************************************/

void sys_seekdir(SMB_STRUCT_DIR *dirp, long offset)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_SEEKDIR64)
	seekdir64(dirp, offset);
#else
	seekdir(dirp, offset);
#endif
}

/*******************************************************************
 A telldir wrapper that will deal with 64 bit filesizes.
********************************************************************/

long sys_telldir(SMB_STRUCT_DIR *dirp)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_TELLDIR64)
	return (long)telldir64(dirp);
#else
	return (long)telldir(dirp);
#endif
}

/*******************************************************************
 A rewinddir wrapper that will deal with 64 bit filesizes.
********************************************************************/

void sys_rewinddir(SMB_STRUCT_DIR *dirp)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_REWINDDIR64)
	rewinddir64(dirp);
#else
	rewinddir(dirp);
#endif
}

/*******************************************************************
 A close wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_closedir(SMB_STRUCT_DIR *dirp)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_CLOSEDIR64)
	return closedir64(dirp);
#else
	return closedir(dirp);
#endif
}

/*******************************************************************
 An mknod() wrapper that will deal with 64 bit filesizes.
********************************************************************/

int sys_mknod(const char *path, mode_t mode, SMB_DEV_T dev)
{
#if defined(HAVE_MKNOD) || defined(HAVE_MKNOD64)
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_MKNOD64) && defined(HAVE_DEV64_T)
	return mknod64(path, mode, dev);
#else
	return mknod(path, mode, dev);
#endif
#else
	/* No mknod system call. */
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
The wait() calls vary between systems
********************************************************************/

int sys_waitpid(pid_t pid,int *status,int options)
{
#ifdef HAVE_WAITPID
	return waitpid(pid,status,options);
#else /* HAVE_WAITPID */
	return wait4(pid, status, options, NULL);
#endif /* HAVE_WAITPID */
}

/*******************************************************************
 System wrapper for getwd
********************************************************************/

char *sys_getwd(char *s)
{
	char *wd;
#ifdef HAVE_GETCWD
	wd = (char *)getcwd(s, PATH_MAX);
#else
	wd = (char *)getwd(s);
#endif
	return wd;
}

#if defined(HAVE_POSIX_CAPABILITIES)

/**************************************************************************
 Try and abstract process capabilities (for systems that have them).
****************************************************************************/

/* Set the POSIX capabilities needed for the given purpose into the effective
 * capability set of the current process. Make sure they are always removed
 * from the inheritable set, because there is no circumstance in which our
 * children should inherit our elevated privileges.
 */
static bool set_process_capability(enum smbd_capability capability,
				   bool enable)
{
	cap_value_t cap_vals[2] = {0};
	int num_cap_vals = 0;

	cap_t cap;

#if defined(HAVE_PRCTL) && defined(PR_GET_KEEPCAPS) && defined(PR_SET_KEEPCAPS)
	/* On Linux, make sure that any capabilities we grab are sticky
	 * across UID changes. We expect that this would allow us to keep both
	 * the effective and permitted capability sets, but as of circa 2.6.16,
	 * only the permitted set is kept. It is a bug (which we work around)
	 * that the effective set is lost, but we still require the effective
	 * set to be kept.
	 */
	if (!prctl(PR_GET_KEEPCAPS)) {
		prctl(PR_SET_KEEPCAPS, 1);
	}
#endif

	cap = cap_get_proc();
	if (cap == NULL) {
		DEBUG(0,("set_process_capability: cap_get_proc failed: %s\n",
			strerror(errno)));
		return False;
	}

	switch (capability) {
		case KERNEL_OPLOCK_CAPABILITY:
#ifdef CAP_NETWORK_MGT
			/* IRIX has CAP_NETWORK_MGT for oplocks. */
			cap_vals[num_cap_vals++] = CAP_NETWORK_MGT;
#endif
			break;
		case DMAPI_ACCESS_CAPABILITY:
#ifdef CAP_DEVICE_MGT
			/* IRIX has CAP_DEVICE_MGT for DMAPI access. */
			cap_vals[num_cap_vals++] = CAP_DEVICE_MGT;
#elif CAP_MKNOD
			/* Linux has CAP_MKNOD for DMAPI access. */
			cap_vals[num_cap_vals++] = CAP_MKNOD;
#endif
			break;
		case LEASE_CAPABILITY:
#ifdef CAP_LEASE
			cap_vals[num_cap_vals++] = CAP_LEASE;
#endif
			break;
	}

	SMB_ASSERT(num_cap_vals <= ARRAY_SIZE(cap_vals));

	if (num_cap_vals == 0) {
		cap_free(cap);
		return True;
	}

	cap_set_flag(cap, CAP_EFFECTIVE, num_cap_vals, cap_vals,
		enable ? CAP_SET : CAP_CLEAR);

	/* We never want to pass capabilities down to our children, so make
	 * sure they are not inherited.
	 */
	cap_set_flag(cap, CAP_INHERITABLE, num_cap_vals, cap_vals, CAP_CLEAR);

	if (cap_set_proc(cap) == -1) {
		DEBUG(0, ("set_process_capability: cap_set_proc failed: %s\n",
			strerror(errno)));
		cap_free(cap);
		return False;
	}

	cap_free(cap);
	return True;
}

#endif /* HAVE_POSIX_CAPABILITIES */

/****************************************************************************
 Gain the oplock capability from the kernel if possible.
****************************************************************************/

void set_effective_capability(enum smbd_capability capability)
{
#if defined(HAVE_POSIX_CAPABILITIES)
	set_process_capability(capability, True);
#endif /* HAVE_POSIX_CAPABILITIES */
}

void drop_effective_capability(enum smbd_capability capability)
{
#if defined(HAVE_POSIX_CAPABILITIES)
	set_process_capability(capability, False);
#endif /* HAVE_POSIX_CAPABILITIES */
}

/**************************************************************************
 Wrapper for random().
****************************************************************************/

long sys_random(void)
{
#if defined(HAVE_RANDOM)
	return (long)random();
#elif defined(HAVE_RAND)
	return (long)rand();
#else
	DEBUG(0,("Error - no random function available !\n"));
	exit(1);
#endif
}

/**************************************************************************
 Wrapper for srandom().
****************************************************************************/

void sys_srandom(unsigned int seed)
{
#if defined(HAVE_SRANDOM)
	srandom(seed);
#elif defined(HAVE_SRAND)
	srand(seed);
#else
	DEBUG(0,("Error - no srandom function available !\n"));
	exit(1);
#endif
}

#ifndef NGROUPS_MAX
#define NGROUPS_MAX 32 /* Guess... */
#endif

/**************************************************************************
 Returns equivalent to NGROUPS_MAX - using sysconf if needed.
****************************************************************************/

int groups_max(void)
{
#if defined(SYSCONF_SC_NGROUPS_MAX)
	int ret = sysconf(_SC_NGROUPS_MAX);
	return (ret == -1) ? NGROUPS_MAX : ret;
#else
	return NGROUPS_MAX;
#endif
}

/**************************************************************************
 Wrap setgroups and getgroups for systems that declare getgroups() as
 returning an array of gid_t, but actuall return an array of int.
****************************************************************************/

#if defined(HAVE_BROKEN_GETGROUPS)

#ifdef HAVE_BROKEN_GETGROUPS
#define GID_T int
#else
#define GID_T gid_t
#endif

static int sys_broken_getgroups(int setlen, gid_t *gidset)
{
	GID_T gid;
	GID_T *group_list;
	int i, ngroups;

	if(setlen == 0) {
		return getgroups(setlen, &gid);
	}

	/*
	 * Broken case. We need to allocate a
	 * GID_T array of size setlen.
	 */

	if(setlen < 0) {
		errno = EINVAL; 
		return -1;
	} 

	if (setlen == 0)
		setlen = groups_max();

	if((group_list = SMB_MALLOC_ARRAY(GID_T, setlen)) == NULL) {
		DEBUG(0,("sys_getgroups: Malloc fail.\n"));
		return -1;
	}

	if((ngroups = getgroups(setlen, group_list)) < 0) {
		int saved_errno = errno;
		SAFE_FREE(group_list);
		errno = saved_errno;
		return -1;
	}

	for(i = 0; i < ngroups; i++)
		gidset[i] = (gid_t)group_list[i];

	SAFE_FREE(group_list);
	return ngroups;
}

static int sys_broken_setgroups(int setlen, gid_t *gidset)
{
	GID_T *group_list;
	int i ; 

	if (setlen == 0)
		return 0 ;

	if (setlen < 0 || setlen > groups_max()) {
		errno = EINVAL; 
		return -1;   
	}

	/*
	 * Broken case. We need to allocate a
	 * GID_T array of size setlen.
	 */

	if((group_list = SMB_MALLOC_ARRAY(GID_T, setlen)) == NULL) {
		DEBUG(0,("sys_setgroups: Malloc fail.\n"));
		return -1;    
	}

	for(i = 0; i < setlen; i++) 
		group_list[i] = (GID_T) gidset[i]; 

	if(setgroups(setlen, group_list) != 0) {
		int saved_errno = errno;
		SAFE_FREE(group_list);
		errno = saved_errno;
		return -1;
	}

	SAFE_FREE(group_list);
	return 0 ;
}

#endif /* HAVE_BROKEN_GETGROUPS */

/* This is a list of systems that require the first GID passed to setgroups(2)
 * to be the effective GID. If your system is one of these, add it here.
 */
#if defined (FREEBSD) || defined (DARWINOS)
#define USE_BSD_SETGROUPS
#endif

#if defined(USE_BSD_SETGROUPS)
/* Depending on the particular BSD implementation, the first GID that is
 * passed to setgroups(2) will either be ignored or will set the credential's
 * effective GID. In either case, the right thing to do is to guarantee that
 * gidset[0] is the effective GID.
 */
static int sys_bsd_setgroups(gid_t primary_gid, int setlen, const gid_t *gidset)
{
	gid_t *new_gidset = NULL;
	int max;
	int ret;

	/* setgroups(2) will fail with EINVAL if we pass too many groups. */
	max = groups_max();

	/* No group list, just make sure we are setting the efective GID. */
	if (setlen == 0) {
		return setgroups(1, &primary_gid);
	}

	/* If the primary gid is not the first array element, grow the array
	 * and insert it at the front.
	 */
	if (gidset[0] != primary_gid) {
	        new_gidset = SMB_MALLOC_ARRAY(gid_t, setlen + 1);
	        if (new_gidset == NULL) {
			return -1;
	        }

		memcpy(new_gidset + 1, gidset, (setlen * sizeof(gid_t)));
		new_gidset[0] = primary_gid;
		setlen++;
	}

	if (setlen > max) {
		DEBUG(3, ("forced to truncate group list from %d to %d\n",
			setlen, max));
		setlen = max;
	}

#if defined(HAVE_BROKEN_GETGROUPS)
	ret = sys_broken_setgroups(setlen, new_gidset ? new_gidset : gidset);
#else
	ret = setgroups(setlen, new_gidset ? new_gidset : gidset);
#endif

	if (new_gidset) {
		int errsav = errno;
		SAFE_FREE(new_gidset);
		errno = errsav;
	}

	return ret;
}

#endif /* USE_BSD_SETGROUPS */

/**************************************************************************
 Wrapper for getgroups. Deals with broken (int) case.
****************************************************************************/

int sys_getgroups(int setlen, gid_t *gidset)
{
#if defined(HAVE_BROKEN_GETGROUPS)
	return sys_broken_getgroups(setlen, gidset);
#else
	return getgroups(setlen, gidset);
#endif
}

/**************************************************************************
 Wrapper for setgroups. Deals with broken (int) case and BSD case.
****************************************************************************/

int sys_setgroups(gid_t UNUSED(primary_gid), int setlen, gid_t *gidset)
{
#if !defined(HAVE_SETGROUPS)
	errno = ENOSYS;
	return -1;
#endif /* HAVE_SETGROUPS */

#if defined(USE_BSD_SETGROUPS)
	return sys_bsd_setgroups(primary_gid, setlen, gidset);
#elif defined(HAVE_BROKEN_GETGROUPS)
	return sys_broken_setgroups(setlen, gidset);
#else
	return setgroups(setlen, gidset);
#endif
}

/**************************************************************************
 Extract a command into an arg list.
****************************************************************************/

static char **extract_args(TALLOC_CTX *mem_ctx, const char *command)
{
	char *trunc_cmd;
	char *saveptr;
	char *ptr;
	int argcl;
	char **argl = NULL;
	int i;

	if (!(trunc_cmd = talloc_strdup(mem_ctx, command))) {
		DEBUG(0, ("talloc failed\n"));
		goto nomem;
	}

	if(!(ptr = strtok_r(trunc_cmd, " \t", &saveptr))) {
		TALLOC_FREE(trunc_cmd);
		errno = EINVAL;
		return NULL;
	}

	/*
	 * Count the args.
	 */

	for( argcl = 1; ptr; ptr = strtok_r(NULL, " \t", &saveptr))
		argcl++;

	TALLOC_FREE(trunc_cmd);

	if (!(argl = TALLOC_ARRAY(mem_ctx, char *, argcl + 1))) {
		goto nomem;
	}

	/*
	 * Now do the extraction.
	 */

	if (!(trunc_cmd = talloc_strdup(mem_ctx, command))) {
		goto nomem;
	}

	ptr = strtok_r(trunc_cmd, " \t", &saveptr);
	i = 0;

	if (!(argl[i++] = talloc_strdup(argl, ptr))) {
		goto nomem;
	}

	while((ptr = strtok_r(NULL, " \t", &saveptr)) != NULL) {

		if (!(argl[i++] = talloc_strdup(argl, ptr))) {
			goto nomem;
		}
	}

	argl[i++] = NULL;
	TALLOC_FREE(trunc_cmd);
	return argl;

 nomem:
	DEBUG(0, ("talloc failed\n"));
	TALLOC_FREE(trunc_cmd);
	TALLOC_FREE(argl);
	errno = ENOMEM;
	return NULL;
}

/**************************************************************************
 Wrapper for popen. Safer as it doesn't search a path.
 Modified from the glibc sources.
 modified by tridge to return a file descriptor. We must kick our FILE* habit
****************************************************************************/

typedef struct _popen_list
{
	int fd;
	pid_t child_pid;
	struct _popen_list *next;
} popen_list;

static popen_list *popen_chain;

int sys_popen(const char *command)
{
	int parent_end, child_end;
	int pipe_fds[2];
	popen_list *entry = NULL;
	char **argl = NULL;

	if (pipe(pipe_fds) < 0)
		return -1;

	parent_end = pipe_fds[0];
	child_end = pipe_fds[1];

	if (!*command) {
		errno = EINVAL;
		goto err_exit;
	}

	if((entry = SMB_MALLOC_P(popen_list)) == NULL)
		goto err_exit;

	ZERO_STRUCTP(entry);

	/*
	 * Extract the command and args into a NULL terminated array.
	 */

	if(!(argl = extract_args(NULL, command)))
		goto err_exit;

	entry->child_pid = sys_fork();

	if (entry->child_pid == -1) {
		goto err_exit;
	}

	if (entry->child_pid == 0) {

		/*
		 * Child !
		 */

		int child_std_end = STDOUT_FILENO;
		popen_list *p;

		close(parent_end);
		if (child_end != child_std_end) {
			dup2 (child_end, child_std_end);
			close (child_end);
		}

		/*
		 * POSIX.2:  "popen() shall ensure that any streams from previous
		 * popen() calls that remain open in the parent process are closed
		 * in the new child process."
		 */

		for (p = popen_chain; p; p = p->next)
			close(p->fd);

		execv(argl[0], argl);
		_exit (127);
	}

	/*
	 * Parent.
	 */

	close (child_end);
	TALLOC_FREE(argl);

	/* Link into popen_chain. */
	entry->next = popen_chain;
	popen_chain = entry;
	entry->fd = parent_end;

	return entry->fd;

err_exit:

	SAFE_FREE(entry);
	TALLOC_FREE(argl);
	close(pipe_fds[0]);
	close(pipe_fds[1]);
	return -1;
}

/**************************************************************************
 Wrapper for pclose. Modified from the glibc sources.
****************************************************************************/

int sys_pclose(int fd)
{
	int wstatus;
	popen_list **ptr = &popen_chain;
	popen_list *entry = NULL;
	pid_t wait_pid;
	int status = -1;

	/* Unlink from popen_chain. */
	for ( ; *ptr != NULL; ptr = &(*ptr)->next) {
		if ((*ptr)->fd == fd) {
			entry = *ptr;
			*ptr = (*ptr)->next;
			status = 0;
			break;
		}
	}

	if (status < 0 || close(entry->fd) < 0)
		return -1;

	/*
	 * As Samba is catching and eating child process
	 * exits we don't really care about the child exit
	 * code, a -1 with errno = ECHILD will do fine for us.
	 */

	do {
		wait_pid = sys_waitpid (entry->child_pid, &wstatus, 0);
	} while (wait_pid == -1 && errno == EINTR);

	SAFE_FREE(entry);

	if (wait_pid == -1)
		return -1;
	return wstatus;
}

/**************************************************************************
 Wrapper for Admin Logs.
****************************************************************************/

 void sys_adminlog(int priority, const char *format_str, ...) 
{
	va_list ap;
	int ret;
	char *msgbuf = NULL;

	va_start( ap, format_str );
	ret = vasprintf( &msgbuf, format_str, ap );
	va_end( ap );

	if (ret == -1)
		return;

#if defined(HAVE_SYSLOG)
	syslog( priority, "%s", msgbuf );
#else
	DEBUG(0,("%s", msgbuf ));
#endif
	SAFE_FREE(msgbuf);
}

/******** Solaris EA helper function prototypes ********/
#ifdef HAVE_ATTROPEN
#define SOLARIS_ATTRMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP
static int solaris_write_xattr(int attrfd, const char *value, size_t size);
static ssize_t solaris_read_xattr(int attrfd, void *value, size_t size);
static ssize_t solaris_list_xattr(int attrdirfd, char *list, size_t size);
static int solaris_unlinkat(int attrdirfd, const char *name);
static int solaris_attropen(const char *path, const char *attrpath, int oflag, mode_t mode);
static int solaris_openat(int fildes, const char *path, int oflag, mode_t mode);
#endif

/**************************************************************************
 Wrappers for extented attribute calls. Based on the Linux package with
 support for IRIX and (Net|Free)BSD also. Expand as other systems have them.
****************************************************************************/

ssize_t sys_getxattr (const char *path, const char *name, void *value, size_t size)
{
#if defined(HAVE_GETXATTR)
#ifndef XATTR_ADD_OPT
	return getxattr(path, name, value, size);
#else
	int options = 0;
	return getxattr(path, name, value, size, 0, options);
#endif
#elif defined(HAVE_GETEA)
	return getea(path, name, value, size);
#elif defined(HAVE_EXTATTR_GET_FILE)
	char *s;
	ssize_t retval;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;
	/*
	 * The BSD implementation has a nasty habit of silently truncating
	 * the returned value to the size of the buffer, so we have to check
	 * that the buffer is large enough to fit the returned value.
	 */
	if((retval=extattr_get_file(path, attrnamespace, attrname, NULL, 0)) >= 0) {
		if(retval > size) {
			errno = ERANGE;
			return -1;
		}
		if((retval=extattr_get_file(path, attrnamespace, attrname, value, size)) >= 0)
			return retval;
	}

	DEBUG(10,("sys_getxattr: extattr_get_file() failed with: %s\n", strerror(errno)));
	return -1;
#elif defined(HAVE_ATTR_GET)
	int retval, flags = 0;
	int valuelength = (int)size;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	retval = attr_get(path, attrname, (char *)value, &valuelength, flags);

	return retval ? retval : valuelength;
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrfd = solaris_attropen(path, name, O_RDONLY, 0);
	if (attrfd >= 0) {
		ret = solaris_read_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

ssize_t sys_lgetxattr (const char *path, const char *name, void *value, size_t size)
{
#if defined(HAVE_LGETXATTR)
	return lgetxattr(path, name, value, size);
#elif defined(HAVE_GETXATTR) && defined(XATTR_ADD_OPT)
	int options = XATTR_NOFOLLOW;
	return getxattr(path, name, value, size, 0, options);
#elif defined(HAVE_LGETEA)
	return lgetea(path, name, value, size);
#elif defined(HAVE_EXTATTR_GET_LINK)
	char *s;
	ssize_t retval;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;

	if((retval=extattr_get_link(path, attrnamespace, attrname, NULL, 0)) >= 0) {
		if(retval > size) {
			errno = ERANGE;
			return -1;
		}
		if((retval=extattr_get_link(path, attrnamespace, attrname, value, size)) >= 0)
			return retval;
	}

	DEBUG(10,("sys_lgetxattr: extattr_get_link() failed with: %s\n", strerror(errno)));
	return -1;
#elif defined(HAVE_ATTR_GET)
	int retval, flags = ATTR_DONTFOLLOW;
	int valuelength = (int)size;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	retval = attr_get(path, attrname, (char *)value, &valuelength, flags);

	return retval ? retval : valuelength;
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrfd = solaris_attropen(path, name, O_RDONLY|AT_SYMLINK_NOFOLLOW, 0);
	if (attrfd >= 0) {
		ret = solaris_read_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

ssize_t sys_fgetxattr (int filedes, const char *name, void *value, size_t size)
{
#if defined(HAVE_FGETXATTR)
#ifndef XATTR_ADD_OPT
	return fgetxattr(filedes, name, value, size);
#else
	int options = 0;
	return fgetxattr(filedes, name, value, size, 0, options);
#endif
#elif defined(HAVE_FGETEA)
	return fgetea(filedes, name, value, size);
#elif defined(HAVE_EXTATTR_GET_FD)
	char *s;
	ssize_t retval;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;

	if((retval=extattr_get_fd(filedes, attrnamespace, attrname, NULL, 0)) >= 0) {
		if(retval > size) {
			errno = ERANGE;
			return -1;
		}
		if((retval=extattr_get_fd(filedes, attrnamespace, attrname, value, size)) >= 0)
			return retval;
	}

	DEBUG(10,("sys_fgetxattr: extattr_get_fd() failed with: %s\n", strerror(errno)));
	return -1;
#elif defined(HAVE_ATTR_GETF)
	int retval, flags = 0;
	int valuelength = (int)size;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	retval = attr_getf(filedes, attrname, (char *)value, &valuelength, flags);

	return retval ? retval : valuelength;
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrfd = solaris_openat(filedes, name, O_RDONLY|O_XATTR, 0);
	if (attrfd >= 0) {
		ret = solaris_read_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

#if defined(HAVE_EXTATTR_LIST_FILE)

#define EXTATTR_PREFIX(s)	(s), (sizeof((s))-1)

static struct {
        int space;
	const char *name;
	size_t len;
} 
extattr[] = {
	{ EXTATTR_NAMESPACE_SYSTEM, EXTATTR_PREFIX("system.") },
        { EXTATTR_NAMESPACE_USER, EXTATTR_PREFIX("user.") },
};

typedef union {
	const char *path;
	int filedes;
} extattr_arg;

static ssize_t bsd_attr_list (int type, extattr_arg arg, char *list, size_t size)
{
	ssize_t list_size, total_size = 0;
	int i, t, len;
	char *buf;
	/* Iterate through extattr(2) namespaces */
	for(t = 0; t < (sizeof(extattr)/sizeof(extattr[0])); t++) {
		if (t != EXTATTR_NAMESPACE_USER && geteuid() != 0) {
			/* ignore all but user namespace when we are not root, see bug 10247 */
			continue;
		}
		switch(type) {
#if defined(HAVE_EXTATTR_LIST_FILE)
			case 0:
				list_size = extattr_list_file(arg.path, extattr[t].space, list, size);
				break;
#endif
#if defined(HAVE_EXTATTR_LIST_LINK)
			case 1:
				list_size = extattr_list_link(arg.path, extattr[t].space, list, size);
				break;
#endif
#if defined(HAVE_EXTATTR_LIST_FD)
			case 2:
				list_size = extattr_list_fd(arg.filedes, extattr[t].space, list, size);
				break;
#endif
			default:
				errno = ENOSYS;
				return -1;
		}
		/* Some error happend. Errno should be set by the previous call */
		if(list_size < 0)
			return -1;
		/* No attributes */
		if(list_size == 0)
			continue;
		/* XXX: Call with an empty buffer may be used to calculate
		   necessary buffer size. Unfortunately, we can't say, how
		   many attributes were returned, so here is the potential
		   problem with the emulation.
		*/
		if(list == NULL) {
			/* Take the worse case of one char attribute names - 
			   two bytes per name plus one more for sanity.
			*/
			total_size += list_size + (list_size/2 + 1)*extattr[t].len;
			continue;
		}
		/* Count necessary offset to fit namespace prefixes */
		len = 0;
		for(i = 0; i < list_size; i += list[i] + 1)
			len += extattr[t].len;

		total_size += list_size + len;
		/* Buffer is too small to fit the results */
		if(total_size > size) {
			errno = ERANGE;
			return -1;
		}
		/* Shift results back, so we can prepend prefixes */
		buf = (char *)memmove(list + len, list, list_size);

		for(i = 0; i < list_size; i += len + 1) {
			len = buf[i];
			strncpy(list, extattr[t].name, extattr[t].len + 1);
			list += extattr[t].len;
			strncpy(list, buf + i + 1, len);
			list[len] = '\0';
			list += len + 1;
		}
		size -= total_size;
	}
	return total_size;
}

#endif

#if defined(HAVE_ATTR_LIST) && defined(HAVE_SYS_ATTRIBUTES_H)
static char attr_buffer[ATTR_MAX_VALUELEN];

static ssize_t irix_attr_list(const char *path, int filedes, char *list, size_t size, int flags)
{
	int retval = 0, index;
	attrlist_cursor_t *cursor = 0;
	int total_size = 0;
	attrlist_t * al = (attrlist_t *)attr_buffer;
	attrlist_ent_t *ae;
	size_t ent_size, left = size;
	char *bp = list;

	while (True) {
	    if (filedes)
		retval = attr_listf(filedes, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
	    else
		retval = attr_list(path, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
	    if (retval) break;
	    for (index = 0; index < al->al_count; index++) {
		ae = ATTR_ENTRY(attr_buffer, index);
		ent_size = strlen(ae->a_name) + sizeof("user.");
		if (left >= ent_size) {
		    strncpy(bp, "user.", sizeof("user."));
		    strncat(bp, ae->a_name, ent_size - sizeof("user."));
		    bp += ent_size;
		    left -= ent_size;
		} else if (size) {
		    errno = ERANGE;
		    retval = -1;
		    break;
		}
		total_size += ent_size;
	    }
	    if (al->al_more == 0) break;
	}
	if (retval == 0) {
	    flags |= ATTR_ROOT;
	    cursor = 0;
	    while (True) {
		if (filedes)
		    retval = attr_listf(filedes, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
		else
		    retval = attr_list(path, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
		if (retval) break;
		for (index = 0; index < al->al_count; index++) {
		    ae = ATTR_ENTRY(attr_buffer, index);
		    ent_size = strlen(ae->a_name) + sizeof("system.");
		    if (left >= ent_size) {
			strncpy(bp, "system.", sizeof("system."));
			strncat(bp, ae->a_name, ent_size - sizeof("system."));
			bp += ent_size;
			left -= ent_size;
		    } else if (size) {
			errno = ERANGE;
			retval = -1;
			break;
		    }
		    total_size += ent_size;
		}
		if (al->al_more == 0) break;
	    }
	}
	return (ssize_t)(retval ? retval : total_size);
}

#endif

ssize_t sys_listxattr (const char *path, char *list, size_t size)
{
#if defined(HAVE_LISTXATTR)
#ifndef XATTR_ADD_OPT
	return listxattr(path, list, size);
#else
	int options = 0;
	return listxattr(path, list, size, options);
#endif
#elif defined(HAVE_LISTEA)
	return listea(path, list, size);
#elif defined(HAVE_EXTATTR_LIST_FILE)
	extattr_arg arg;
	arg.path = path;
	return bsd_attr_list(0, arg, list, size);
#elif defined(HAVE_ATTR_LIST) && defined(HAVE_SYS_ATTRIBUTES_H)
	return irix_attr_list(path, 0, list, size, 0);
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY, 0);
	if (attrdirfd >= 0) {
		ret = solaris_list_xattr(attrdirfd, list, size);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

ssize_t sys_llistxattr (const char *path, char *list, size_t size)
{
#if defined(HAVE_LLISTXATTR)
	return llistxattr(path, list, size);
#elif defined(HAVE_LISTXATTR) && defined(XATTR_ADD_OPT)
	int options = XATTR_NOFOLLOW;
	return listxattr(path, list, size, options);
#elif defined(HAVE_LLISTEA)
	return llistea(path, list, size);
#elif defined(HAVE_EXTATTR_LIST_LINK)
	extattr_arg arg;
	arg.path = path;
	return bsd_attr_list(1, arg, list, size);
#elif defined(HAVE_ATTR_LIST) && defined(HAVE_SYS_ATTRIBUTES_H)
	return irix_attr_list(path, 0, list, size, ATTR_DONTFOLLOW);
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY|AT_SYMLINK_NOFOLLOW, 0);
	if (attrdirfd >= 0) {
		ret = solaris_list_xattr(attrdirfd, list, size);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

ssize_t sys_flistxattr (int filedes, char *list, size_t size)
{
#if defined(HAVE_FLISTXATTR)
#ifndef XATTR_ADD_OPT
	return flistxattr(filedes, list, size);
#else
	int options = 0;
	return flistxattr(filedes, list, size, options);
#endif
#elif defined(HAVE_FLISTEA)
	return flistea(filedes, list, size);
#elif defined(HAVE_EXTATTR_LIST_FD)
	extattr_arg arg;
	arg.filedes = filedes;
	return bsd_attr_list(2, arg, list, size);
#elif defined(HAVE_ATTR_LISTF)
	return irix_attr_list(NULL, filedes, list, size, 0);
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrdirfd = solaris_openat(filedes, ".", O_RDONLY|O_XATTR, 0);
	if (attrdirfd >= 0) {
		ret = solaris_list_xattr(attrdirfd, list, size);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_removexattr (const char *path, const char *name)
{
#if defined(HAVE_REMOVEXATTR)
#ifndef XATTR_ADD_OPT
	return removexattr(path, name);
#else
	int options = 0;
	return removexattr(path, name, options);
#endif
#elif defined(HAVE_REMOVEEA)
	return removeea(path, name);
#elif defined(HAVE_EXTATTR_DELETE_FILE)
	char *s;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;

	return extattr_delete_file(path, attrnamespace, attrname);
#elif defined(HAVE_ATTR_REMOVE)
	int flags = 0;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	return attr_remove(path, attrname, flags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY, 0);
	if (attrdirfd >= 0) {
		ret = solaris_unlinkat(attrdirfd, name);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_lremovexattr (const char *path, const char *name)
{
#if defined(HAVE_LREMOVEXATTR)
	return lremovexattr(path, name);
#elif defined(HAVE_REMOVEXATTR) && defined(XATTR_ADD_OPT)
	int options = XATTR_NOFOLLOW;
	return removexattr(path, name, options);
#elif defined(HAVE_LREMOVEEA)
	return lremoveea(path, name);
#elif defined(HAVE_EXTATTR_DELETE_LINK)
	char *s;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;

	return extattr_delete_link(path, attrnamespace, attrname);
#elif defined(HAVE_ATTR_REMOVE)
	int flags = ATTR_DONTFOLLOW;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	return attr_remove(path, attrname, flags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY|AT_SYMLINK_NOFOLLOW, 0);
	if (attrdirfd >= 0) {
		ret = solaris_unlinkat(attrdirfd, name);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_fremovexattr (int filedes, const char *name)
{
#if defined(HAVE_FREMOVEXATTR)
#ifndef XATTR_ADD_OPT
	return fremovexattr(filedes, name);
#else
	int options = 0;
	return fremovexattr(filedes, name, options);
#endif
#elif defined(HAVE_FREMOVEEA)
	return fremoveea(filedes, name);
#elif defined(HAVE_EXTATTR_DELETE_FD)
	char *s;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;

	return extattr_delete_fd(filedes, attrnamespace, attrname);
#elif defined(HAVE_ATTR_REMOVEF)
	int flags = 0;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	return attr_removef(filedes, attrname, flags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int attrdirfd = solaris_openat(filedes, ".", O_RDONLY|O_XATTR, 0);
	if (attrdirfd >= 0) {
		ret = solaris_unlinkat(attrdirfd, name);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_setxattr (const char *path, const char *name, const void *value, size_t size, int flags)
{
#if defined(HAVE_SETXATTR)
#ifndef XATTR_ADD_OPT
	return setxattr(path, name, value, size, flags);
#else
	int options = 0;
	return setxattr(path, name, value, size, 0, options);
#endif
#elif defined(HAVE_SETEA)
	return setea(path, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_FILE)
	char *s;
	int retval = 0;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;
	if (flags) {
		/* Check attribute existence */
		retval = extattr_get_file(path, attrnamespace, attrname, NULL, 0);
		if (retval < 0) {
			/* REPLACE attribute, that doesn't exist */
			if (flags & XATTR_REPLACE && errno == ENOATTR) {
				errno = ENOATTR;
				return -1;
			}
			/* Ignore other errors */
		}
		else {
			/* CREATE attribute, that already exists */
			if (flags & XATTR_CREATE) {
				errno = EEXIST;
				return -1;
			}
		}
	}
	retval = extattr_set_file(path, attrnamespace, attrname, value, size);
	return (retval < 0) ? -1 : 0;
#elif defined(HAVE_ATTR_SET)
	int myflags = 0;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) myflags |= ATTR_ROOT;
	if (flags & XATTR_CREATE) myflags |= ATTR_CREATE;
	if (flags & XATTR_REPLACE) myflags |= ATTR_REPLACE;

	return attr_set(path, attrname, (const char *)value, size, myflags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int myflags = O_RDWR;
	int attrfd;
	if (flags & XATTR_CREATE) myflags |= O_EXCL;
	if (!(flags & XATTR_REPLACE)) myflags |= O_CREAT;
	attrfd = solaris_attropen(path, name, myflags, (mode_t) SOLARIS_ATTRMODE);
	if (attrfd >= 0) {
		ret = solaris_write_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_lsetxattr (const char *path, const char *name, const void *value, size_t size, int flags)
{
#if defined(HAVE_LSETXATTR)
	return lsetxattr(path, name, value, size, flags);
#elif defined(HAVE_SETXATTR) && defined(XATTR_ADD_OPT)
	int options = XATTR_NOFOLLOW;
	return setxattr(path, name, value, size, 0, options);
#elif defined(LSETEA)
	return lsetea(path, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_LINK)
	char *s;
	int retval = 0;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;
	if (flags) {
		/* Check attribute existence */
		retval = extattr_get_link(path, attrnamespace, attrname, NULL, 0);
		if (retval < 0) {
			/* REPLACE attribute, that doesn't exist */
			if (flags & XATTR_REPLACE && errno == ENOATTR) {
				errno = ENOATTR;
				return -1;
			}
			/* Ignore other errors */
		}
		else {
			/* CREATE attribute, that already exists */
			if (flags & XATTR_CREATE) {
				errno = EEXIST;
				return -1;
			}
		}
	}

	retval = extattr_set_link(path, attrnamespace, attrname, value, size);
	return (retval < 0) ? -1 : 0;
#elif defined(HAVE_ATTR_SET)
	int myflags = ATTR_DONTFOLLOW;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) myflags |= ATTR_ROOT;
	if (flags & XATTR_CREATE) myflags |= ATTR_CREATE;
	if (flags & XATTR_REPLACE) myflags |= ATTR_REPLACE;

	return attr_set(path, attrname, (const char *)value, size, myflags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int myflags = O_RDWR | AT_SYMLINK_NOFOLLOW;
	int attrfd;
	if (flags & XATTR_CREATE) myflags |= O_EXCL;
	if (!(flags & XATTR_REPLACE)) myflags |= O_CREAT;
	attrfd = solaris_attropen(path, name, myflags, (mode_t) SOLARIS_ATTRMODE);
	if (attrfd >= 0) {
		ret = solaris_write_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_fsetxattr (int filedes, const char *name, const void *value, size_t size, int flags)
{
#if defined(HAVE_FSETXATTR)
#ifndef XATTR_ADD_OPT
	return fsetxattr(filedes, name, value, size, flags);
#else
	int options = 0;
	return fsetxattr(filedes, name, value, size, 0, options);
#endif
#elif defined(HAVE_FSETEA)
	return fsetea(filedes, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_FD)
	char *s;
	int retval = 0;
	int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
		EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
	const char *attrname = ((s=strchr_m(name, '.')) == NULL) ? name : s + 1;
	if (flags) {
		/* Check attribute existence */
		retval = extattr_get_fd(filedes, attrnamespace, attrname, NULL, 0);
		if (retval < 0) {
			/* REPLACE attribute, that doesn't exist */
			if (flags & XATTR_REPLACE && errno == ENOATTR) {
				errno = ENOATTR;
				return -1;
			}
			/* Ignore other errors */
		}
		else {
			/* CREATE attribute, that already exists */
			if (flags & XATTR_CREATE) {
				errno = EEXIST;
				return -1;
			}
		}
	}
	retval = extattr_set_fd(filedes, attrnamespace, attrname, value, size);
	return (retval < 0) ? -1 : 0;
#elif defined(HAVE_ATTR_SETF)
	int myflags = 0;
	char *attrname = strchr(name,'.') + 1;

	if (strncmp(name, "system", 6) == 0) myflags |= ATTR_ROOT;
	if (flags & XATTR_CREATE) myflags |= ATTR_CREATE;
	if (flags & XATTR_REPLACE) myflags |= ATTR_REPLACE;

	return attr_setf(filedes, attrname, (const char *)value, size, myflags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int myflags = O_RDWR | O_XATTR;
	int attrfd;
	if (flags & XATTR_CREATE) myflags |= O_EXCL;
	if (!(flags & XATTR_REPLACE)) myflags |= O_CREAT;
	attrfd = solaris_openat(filedes, name, myflags, (mode_t) SOLARIS_ATTRMODE);
	if (attrfd >= 0) {
		ret = solaris_write_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

/**************************************************************************
 helper functions for Solaris' EA support
****************************************************************************/
#ifdef HAVE_ATTROPEN
static ssize_t solaris_read_xattr(int attrfd, void *value, size_t size)
{
	struct stat sbuf;

	if (fstat(attrfd, &sbuf) == -1) {
		errno = ENOATTR;
		return -1;
	}

	/* This is to return the current size of the named extended attribute */
	if (size == 0) {
		return sbuf.st_size;
	}

	/* check size and read xattr */
	if (sbuf.st_size > size) {
		errno = ERANGE;
		return -1;
	}

	return read(attrfd, value, sbuf.st_size);
}

static ssize_t solaris_list_xattr(int attrdirfd, char *list, size_t size)
{
	ssize_t len = 0;
	DIR *dirp;
	struct dirent *de;
	int newfd = dup(attrdirfd);
	/* CAUTION: The originating file descriptor should not be
	            used again following the call to fdopendir().
	            For that reason we dup() the file descriptor
		    here to make things more clear. */
	dirp = fdopendir(newfd);

	while ((de = readdir(dirp))) {
		size_t listlen = strlen(de->d_name);
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
			/* we don't want "." and ".." here: */
			DEBUG(10,("skipped EA %s\n",de->d_name));
			continue;
		}

		if (size == 0) {
			/* return the current size of the list of extended attribute names*/
			len += listlen + 1;
		} else {
			/* check size and copy entrieѕ + nul into list. */
			if ((len + listlen + 1) > size) {
				errno = ERANGE;
				len = -1;
				break;
			} else {
				safe_strcpy(list + len, de->d_name, listlen);
				len += listlen;
				list[len] = '\0';
				++len;
			}
		}
	}

	if (closedir(dirp) == -1) {
		DEBUG(0,("closedir dirp failed: %s\n",strerror(errno)));
		return -1;
	}
	return len;
}

static int solaris_unlinkat(int attrdirfd, const char *name)
{
	if (unlinkat(attrdirfd, name, 0) == -1) {
		if (errno == ENOENT) {
			errno = ENOATTR;
		}
		return -1;
	}
	return 0;
}

static int solaris_attropen(const char *path, const char *attrpath, int oflag, mode_t mode)
{
	int filedes = attropen(path, attrpath, oflag, mode);
	if (filedes == -1) {
		DEBUG(10,("attropen FAILED: path: %s, name: %s, errno: %s\n",path,attrpath,strerror(errno)));
		if (errno == EINVAL) {
			errno = ENOTSUP;
		} else {
			errno = ENOATTR;
		}
	}
	return filedes;
}

static int solaris_openat(int fildes, const char *path, int oflag, mode_t mode)
{
	int filedes = openat(fildes, path, oflag, mode);
	if (filedes == -1) {
		DEBUG(10,("openat FAILED: fd: %d, path: %s, errno: %s\n",filedes,path,strerror(errno)));
		if (errno == EINVAL) {
			errno = ENOTSUP;
		} else {
			errno = ENOATTR;
		}
	}
	return filedes;
}

static int solaris_write_xattr(int attrfd, const char *value, size_t size)
{
	if ((ftruncate(attrfd, 0) == 0) && (write(attrfd, value, size) == size)) {
		return 0;
	} else {
		DEBUG(10,("solaris_write_xattr FAILED!\n"));
		return -1;
	}
}
#endif /*HAVE_ATTROPEN*/


/****************************************************************************
 Return the major devicenumber for UNIX extensions.
****************************************************************************/

uint32 unix_dev_major(SMB_DEV_T dev)
{
#if defined(HAVE_DEVICE_MAJOR_FN)
        return (uint32)major(dev);
#else
        return (uint32)(dev >> 8);
#endif
}

/****************************************************************************
 Return the minor devicenumber for UNIX extensions.
****************************************************************************/

uint32 unix_dev_minor(SMB_DEV_T dev)
{
#if defined(HAVE_DEVICE_MINOR_FN)
        return (uint32)minor(dev);
#else
        return (uint32)(dev & 0xff);
#endif
}

#if defined(WITH_AIO)

/*******************************************************************
 An aio_read wrapper that will deal with 64-bit sizes.
********************************************************************/

int sys_aio_read(SMB_STRUCT_AIOCB *aiocb)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64) && defined(HAVE_AIO_READ64)
        return aio_read64(aiocb);
#elif defined(HAVE_AIO_READ)
        return aio_read(aiocb);
#else
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
 An aio_write wrapper that will deal with 64-bit sizes.
********************************************************************/

int sys_aio_write(SMB_STRUCT_AIOCB *aiocb)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64) && defined(HAVE_AIO_WRITE64)
        return aio_write64(aiocb);
#elif defined(HAVE_AIO_WRITE)
        return aio_write(aiocb);
#else
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
 An aio_return wrapper that will deal with 64-bit sizes.
********************************************************************/

ssize_t sys_aio_return(SMB_STRUCT_AIOCB *aiocb)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64) && defined(HAVE_AIO_RETURN64)
        return aio_return64(aiocb);
#elif defined(HAVE_AIO_RETURN)
        return aio_return(aiocb);
#else
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
 An aio_cancel wrapper that will deal with 64-bit sizes.
********************************************************************/

int sys_aio_cancel(int fd, SMB_STRUCT_AIOCB *aiocb)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64) && defined(HAVE_AIO_CANCEL64)
        return aio_cancel64(fd, aiocb);
#elif defined(HAVE_AIO_CANCEL)
        return aio_cancel(fd, aiocb);
#else
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
 An aio_error wrapper that will deal with 64-bit sizes.
********************************************************************/

int sys_aio_error(const SMB_STRUCT_AIOCB *aiocb)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64) && defined(HAVE_AIO_ERROR64)
        return aio_error64(aiocb);
#elif defined(HAVE_AIO_ERROR)
        return aio_error(aiocb);
#else
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
 An aio_fsync wrapper that will deal with 64-bit sizes.
********************************************************************/

int sys_aio_fsync(int op, SMB_STRUCT_AIOCB *aiocb)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64) && defined(HAVE_AIO_FSYNC64)
        return aio_fsync64(op, aiocb);
#elif defined(HAVE_AIO_FSYNC)
        return aio_fsync(op, aiocb);
#else
	errno = ENOSYS;
	return -1;
#endif
}

/*******************************************************************
 An aio_fsync wrapper that will deal with 64-bit sizes.
********************************************************************/

int sys_aio_suspend(const SMB_STRUCT_AIOCB * const cblist[], int n, const struct timespec *timeout)
{
#if defined(HAVE_EXPLICIT_LARGEFILE_SUPPORT) && defined(HAVE_AIOCB64) && defined(HAVE_AIO_SUSPEND64)
        return aio_suspend64(cblist, n, timeout);
#elif defined(HAVE_AIO_FSYNC)
        return aio_suspend(cblist, n, timeout);
#else
	errno = ENOSYS;
	return -1;
#endif
}
#else /* !WITH_AIO */

int sys_aio_read(SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

int sys_aio_write(SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

ssize_t sys_aio_return(SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

int sys_aio_cancel(int fd, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

int sys_aio_error(const SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

int sys_aio_fsync(int op, SMB_STRUCT_AIOCB *aiocb)
{
	errno = ENOSYS;
	return -1;
}

int sys_aio_suspend(const SMB_STRUCT_AIOCB * const cblist[], int n, const struct timespec *timeout)
{
	errno = ENOSYS;
	return -1;
}
#endif /* WITH_AIO */
