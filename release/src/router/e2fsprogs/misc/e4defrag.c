/*
 * e4defrag.c - ext4 filesystem defragmenter
 *
 * Copyright (C) 2009 NEC Software Tohoku, Ltd.
 *
 * Author: Akira Fujita	<a-fujita@rs.jp.nec.com>
 *         Takashi Sato	<t-sato@yk.jp.nec.com>
 */

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ext2fs/ext2_types.h>
#include <ext2fs/ext2fs.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <ext2fs/fiemap.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/vfs.h>

/* A relatively new ioctl interface ... */
#ifndef EXT4_IOC_MOVE_EXT
#define EXT4_IOC_MOVE_EXT      _IOWR('f', 15, struct move_extent)
#endif

/* Macro functions */
#define PRINT_ERR_MSG(msg)	fprintf(stderr, "%s\n", (msg))
#define IN_FTW_PRINT_ERR_MSG(msg)	\
	fprintf(stderr, "\t%s\t\t[ NG ]\n", (msg))
#define PRINT_FILE_NAME(file)	fprintf(stderr, " \"%s\"\n", (file))
#define PRINT_ERR_MSG_WITH_ERRNO(msg)	\
	fprintf(stderr, "\t%s:%s\t[ NG ]\n", (msg), strerror(errno))
#define STATISTIC_ERR_MSG(msg)	\
	fprintf(stderr, "\t%s\n", (msg))
#define STATISTIC_ERR_MSG_WITH_ERRNO(msg)	\
	fprintf(stderr, "\t%s:%s\n", (msg), strerror(errno))
#define min(x, y) (((x) > (y)) ? (y) : (x))
#define CALC_SCORE(ratio) \
	((ratio) > 10 ? (80 + 20 * (ratio) / 100) : (8 * (ratio)))
/* Wrap up the free function */
#define FREE(tmp)				\
	do {					\
		if ((tmp) != NULL)		\
			free(tmp);		\
	} while (0)				\
/* Insert list2 after list1 */
#define insert(list1, list2)			\
	do {					\
		list2->next = list1->next;	\
		list1->next->prev = list2;	\
		list2->prev = list1;		\
		list1->next = list2;		\
	} while (0)

/* To delete unused warning */
#ifdef __GNUC__
#define EXT2FS_ATTR(x) __attribute__(x)
#else
#define EXT2FS_ATTR(x)
#endif

/* The mode of defrag */
#define DETAIL			0x01
#define STATISTIC		0x02

#define DEVNAME			0
#define DIRNAME			1
#define FILENAME		2

#define FTW_OPEN_FD		2000

#define FS_EXT4			"ext4"
#define ROOT_UID		0

#define BOUND_SCORE		55
#define SHOW_FRAG_FILES	5

/* Magic number for ext4 */
#define EXT4_SUPER_MAGIC	0xEF53

/* Definition of flex_bg */
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200

/* The following macro is used for ioctl FS_IOC_FIEMAP
 * EXTENT_MAX_COUNT:	the maximum number of extents for exchanging between
 *			kernel-space and user-space per ioctl
 */
#define EXTENT_MAX_COUNT	512

/* The following macros are error message */
#define MSG_USAGE		\
"Usage	: e4defrag [-v] file...| directory...| device...\n\
	: e4defrag  -c  file...| directory...| device...\n"

#define NGMSG_EXT4		"Filesystem is not ext4 filesystem"
#define NGMSG_FILE_EXTENT	"Failed to get file extents"
#define NGMSG_FILE_INFO		"Failed to get file information"
#define NGMSG_FILE_OPEN		"Failed to open"
#define NGMSG_FILE_UNREG	"File is not regular file"
#define NGMSG_LOST_FOUND	"Can not process \"lost+found\""

/* Data type for filesystem-wide blocks number */
typedef unsigned long long ext4_fsblk_t;

struct fiemap_extent_data {
	__u64 len;			/* blocks count */
	__u64 logical;		/* start logical block number */
	ext4_fsblk_t physical;		/* start physical block number */
};

struct fiemap_extent_list {
	struct fiemap_extent_list *prev;
	struct fiemap_extent_list *next;
	struct fiemap_extent_data data;	/* extent belong to file */
};

struct fiemap_extent_group {
	struct fiemap_extent_group *prev;
	struct fiemap_extent_group *next;
	__u64 len;	/* length of this continuous region */
	struct fiemap_extent_list *start;	/* start ext */
	struct fiemap_extent_list *end;		/* end ext */
};

struct move_extent {
	__s32 reserved;	/* original file descriptor */
	__u32 donor_fd;	/* donor file descriptor */
	__u64 orig_start;	/* logical start offset in block for orig */
	__u64 donor_start;	/* logical start offset in block for donor */
	__u64 len;	/* block length to be moved */
	__u64 moved_len;	/* moved block length */
};

struct frag_statistic_ino {
	int now_count;	/* the file's extents count of before defrag */
	int best_count; /* the best file's extents count */
	__u64 size_per_ext;	/* size(KB) per extent */
	float ratio;	/* the ratio of fragmentation */
	char msg_buffer[PATH_MAX + 1];	/* pathname of the file */
};

char	lost_found_dir[PATH_MAX + 1];
int	block_size;
int	extents_before_defrag;
int	extents_after_defrag;
int	mode_flag;
unsigned int	current_uid;
unsigned int	defraged_file_count;
unsigned int	frag_files_before_defrag;
unsigned int	frag_files_after_defrag;
unsigned int	regular_count;
unsigned int	succeed_cnt;
unsigned int	total_count;
__u8 log_groups_per_flex;
__u32 blocks_per_group;
__u32 feature_incompat;
ext4_fsblk_t	files_block_count;
struct frag_statistic_ino	frag_rank[SHOW_FRAG_FILES];


/* Local definitions of some syscalls glibc may not yet have */

#ifndef HAVE_POSIX_FADVISE
#warning Using locally defined posix_fadvise interface.

#ifndef __NR_fadvise64_64
#error Your kernel headers dont define __NR_fadvise64_64
#endif

/*
 * fadvise() -		Give advice about file access.
 *
 * @fd:			defrag target file's descriptor.
 * @offset:		file offset.
 * @len:		area length.
 * @advise:		process flag.
 */
static int posix_fadvise(int fd, loff_t offset, size_t len, int advise)
{
	return syscall(__NR_fadvise64_64, fd, offset, len, advise);
}
#endif /* ! HAVE_FADVISE64_64 */

#ifndef HAVE_SYNC_FILE_RANGE
#warning Using locally defined sync_file_range interface.

#ifndef __NR_sync_file_range
#ifndef __NR_sync_file_range2 /* ppc */
#error Your kernel headers dont define __NR_sync_file_range
#endif
#endif

/*
 * sync_file_range() -	Sync file region.
 *
 * @fd:			defrag target file's descriptor.
 * @offset:		file offset.
 * @length:		area length.
 * @flag:		process flag.
 */
int sync_file_range(int fd, loff_t offset, loff_t length, unsigned int flag)
{
#ifdef __NR_sync_file_range
	return syscall(__NR_sync_file_range, fd, offset, length, flag);
#else
	return syscall(__NR_sync_file_range2, fd, flag, offset, length);
#endif
}
#endif /* ! HAVE_SYNC_FILE_RANGE */

#ifndef HAVE_FALLOCATE64
#warning Using locally defined fallocate syscall interface.

#ifndef __NR_fallocate
#error Your kernel headers dont define __NR_fallocate
#endif

/*
 * fallocate64() -	Manipulate file space.
 *
 * @fd:			defrag target file's descriptor.
 * @mode:		process flag.
 * @offset:		file offset.
 * @len:		file size.
 */
static int fallocate64(int fd, int mode, loff_t offset, loff_t len)
{
	return syscall(__NR_fallocate, fd, mode, offset, len);
}
#endif /* ! HAVE_FALLOCATE */

/*
 * get_mount_point() -	Get device's mount point.
 *
 * @devname:		the device's name.
 * @mount_point:	the mount point.
 * @dir_path_len:	the length of directory.
 */
static int get_mount_point(const char *devname, char *mount_point,
							int dir_path_len)
{
	/* Refer to /etc/mtab */
	const char	*mtab = MOUNTED;
	FILE		*fp = NULL;
	struct mntent	*mnt = NULL;
	struct stat64	sb;

	if (stat64(devname, &sb) < 0) {
		perror(NGMSG_FILE_INFO);
		PRINT_FILE_NAME(devname);
		return -1;
	}

	fp = setmntent(mtab, "r");
	if (fp == NULL) {
		perror("Couldn't access /etc/mtab");
		return -1;
	}

	while ((mnt = getmntent(fp)) != NULL) {
		struct stat64 ms;

		/*
		 * To handle device symlinks, we see if the
		 * device number matches, not the name
		 */
		if (stat64(mnt->mnt_fsname, &ms) < 0)
			continue;
		if (sb.st_rdev != ms.st_rdev)
			continue;

		endmntent(fp);
		if (strcmp(mnt->mnt_type, FS_EXT4) == 0) {
			strncpy(mount_point, mnt->mnt_dir,
				dir_path_len);
			return 0;
		}
		PRINT_ERR_MSG(NGMSG_EXT4);
		return -1;
	}
	endmntent(fp);
	PRINT_ERR_MSG("Filesystem is not mounted");
	return -1;
}

/*
 * is_ext4() -		Whether on an ext4 filesystem.
 *
 * @file:		the file's name.
 */
static int is_ext4(const char *file, char *devname)
{
	int 	maxlen = 0;
	int	len, ret;
	FILE	*fp = NULL;
	char	*mnt_type = NULL;
	/* Refer to /etc/mtab */
	const char	*mtab = MOUNTED;
	char	file_path[PATH_MAX + 1];
	struct mntent	*mnt = NULL;
	struct statfs64	fsbuf;

	/* Get full path */
	if (realpath(file, file_path) == NULL) {
		perror("Couldn't get full path");
		PRINT_FILE_NAME(file);
		return -1;
	}

	if (statfs64(file_path, &fsbuf) < 0) {
		perror("Failed to get filesystem information");
		PRINT_FILE_NAME(file);
		return -1;
	}

	if (fsbuf.f_type != EXT4_SUPER_MAGIC) {
		PRINT_ERR_MSG(NGMSG_EXT4);
		return -1;
	}

	fp = setmntent(mtab, "r");
	if (fp == NULL) {
		perror("Couldn't access /etc/mtab");
		return -1;
	}

	while ((mnt = getmntent(fp)) != NULL) {
		if (mnt->mnt_fsname[0] != '/')
			continue;
		len = strlen(mnt->mnt_dir);
		ret = memcmp(file_path, mnt->mnt_dir, len);
		if (ret != 0)
			continue;

		if (maxlen >= len)
			continue;

		maxlen = len;

		mnt_type = realloc(mnt_type, strlen(mnt->mnt_type) + 1);
		if (mnt_type == NULL) {
			endmntent(fp);
			return -1;
		}
		memset(mnt_type, 0, strlen(mnt->mnt_type) + 1);
		strncpy(mnt_type, mnt->mnt_type, strlen(mnt->mnt_type));
		strncpy(lost_found_dir, mnt->mnt_dir, PATH_MAX);
		strncpy(devname, mnt->mnt_fsname, strlen(mnt->mnt_fsname) + 1);
	}

	endmntent(fp);
	if (strcmp(mnt_type, FS_EXT4) == 0) {
		FREE(mnt_type);
		return 0;
	} else {
		FREE(mnt_type);
		PRINT_ERR_MSG(NGMSG_EXT4);
		return -1;
	}
}

/*
 * calc_entry_counts() -	Calculate file counts.
 *
 * @file:		file name.
 * @buf:		file info.
 * @flag:		file type.
 * @ftwbuf:		the pointer of a struct FTW.
 */
static int calc_entry_counts(const char *file EXT2FS_ATTR((unused)),
		const struct stat64 *buf, int flag EXT2FS_ATTR((unused)),
		struct FTW *ftwbuf EXT2FS_ATTR((unused)))
{
	if (S_ISREG(buf->st_mode))
		regular_count++;

	total_count++;

	return 0;
}

/*
 * page_in_core() -	Get information on whether pages are in core.
 *
 * @fd:			defrag target file's descriptor.
 * @defrag_data:	data used for defrag.
 * @vec:		page state array.
 * @page_num:		page number.
 */
static int page_in_core(int fd, struct move_extent defrag_data,
			unsigned char **vec, unsigned int *page_num)
{
	long	pagesize;
	void	*page = NULL;
	loff_t	offset, end_offset, length;

	if (vec == NULL || *vec != NULL)
		return -1;

	pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 0)
		return -1;
	/* In mmap, offset should be a multiple of the page size */
	offset = (loff_t)defrag_data.orig_start * block_size;
	length = (loff_t)defrag_data.len * block_size;
	end_offset = offset + length;
	/* Round the offset down to the nearest multiple of pagesize */
	offset = (offset / pagesize) * pagesize;
	length = end_offset - offset;

	page = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, offset);
	if (page == MAP_FAILED)
		return -1;

	*page_num = 0;
	*page_num = (length + pagesize - 1) / pagesize;
	*vec = (unsigned char *)calloc(*page_num, 1);
	if (*vec == NULL)
		return -1;

	/* Get information on whether pages are in core */
	if (mincore(page, (size_t)length, *vec) == -1 ||
		munmap(page, length) == -1) {
		FREE(*vec);
		return -1;
	}

	return 0;
}

/*
 * defrag_fadvise() -	Predeclare an access pattern for file data.
 *
 * @fd:			defrag target file's descriptor.
 * @defrag_data:	data used for defrag.
 * @vec:		page state array.
 * @page_num:		page number.
 */
static int defrag_fadvise(int fd, struct move_extent defrag_data,
		   unsigned char *vec, unsigned int page_num)
{
	int	flag = 1;
	long	pagesize = sysconf(_SC_PAGESIZE);
	int	fadvise_flag = POSIX_FADV_DONTNEED;
	int	sync_flag = SYNC_FILE_RANGE_WAIT_BEFORE |
			    SYNC_FILE_RANGE_WRITE |
			    SYNC_FILE_RANGE_WAIT_AFTER;
	unsigned int	i;
	loff_t	offset;

	offset = (loff_t)defrag_data.orig_start * block_size;
	offset = (offset / pagesize) * pagesize;

	/* Sync file for fadvise process */
	if (sync_file_range(fd, offset,
		(loff_t)pagesize * page_num, sync_flag) < 0)
		return -1;

	/* Try to release buffer cache which this process used,
	 * then other process can use the released buffer
	 */
	for (i = 0; i < page_num; i++) {
		if ((vec[i] & 0x1) == 0) {
			offset += pagesize;
			continue;
		}
		if (posix_fadvise(fd, offset, pagesize, fadvise_flag) < 0) {
			if ((mode_flag & DETAIL) && flag) {
				perror("\tFailed to fadvise");
				flag = 0;
			}
		}
		offset += pagesize;
	}

	return 0;
}

/*
 * check_free_size() -	Check if there's enough disk space.
 *
 * @fd:			defrag target file's descriptor.
 * @file:		file name.
 * @blk_count:		file blocks.
 */
static int check_free_size(int fd, const char *file, ext4_fsblk_t blk_count)
{
	ext4_fsblk_t	free_blk_count;
	struct statfs64	fsbuf;

	if (fstatfs64(fd, &fsbuf) < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(
				"Failed to get filesystem information");
		}
		return -1;
	}

	/* Compute free space for root and normal user separately */
	if (current_uid == ROOT_UID)
		free_blk_count = fsbuf.f_bfree;
	else
		free_blk_count = fsbuf.f_bavail;

	if (free_blk_count >= blk_count)
		return 0;

	return -ENOSPC;
}

/*
 * file_frag_count() -	Get file fragment count.
 *
 * @fd:			defrag target file's descriptor.
 */
static int file_frag_count(int fd)
{
	int	ret;
	struct fiemap	fiemap_buf;

	/* When fm_extent_count is 0,
	 * ioctl just get file fragment count.
	 */
	memset(&fiemap_buf, 0, sizeof(struct fiemap));
	fiemap_buf.fm_start = 0;
	fiemap_buf.fm_length = FIEMAP_MAX_OFFSET;
	fiemap_buf.fm_flags |= FIEMAP_FLAG_SYNC;

	ret = ioctl(fd, FS_IOC_FIEMAP, &fiemap_buf);
	if (ret < 0)
		return ret;

	return fiemap_buf.fm_mapped_extents;
}

/*
 * file_check() -	Check file's attributes.
 *
 * @fd:			defrag target file's descriptor.
 * @buf:		a pointer of the struct stat64.
 * @file:		file name.
 * @extents:		file extents.
 * @blk_count:		file blocks.
 */
static int file_check(int fd, const struct stat64 *buf, const char *file,
		int extents, ext4_fsblk_t blk_count)
{
	int	ret;
	struct flock	lock;

	/* Write-lock check is more reliable */
	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	/* Free space */
	ret = check_free_size(fd, file, blk_count);
	if (ret < 0) {
		if ((mode_flag & DETAIL) && ret == -ENOSPC) {
			printf("\033[79;0H\033[K[%u/%u] \"%s\"\t\t"
				"  extents: %d -> %d\n", defraged_file_count,
				total_count, file, extents, extents);
			IN_FTW_PRINT_ERR_MSG(
			"Defrag size is larger than filesystem's free space");
		}
		return -1;
	}

	/* Access authority */
	if (current_uid != ROOT_UID &&
		buf->st_uid != current_uid) {
		if (mode_flag & DETAIL) {
			printf("\033[79;0H\033[K[%u/%u] \"%s\"\t\t"
				"  extents: %d -> %d\n", defraged_file_count,
				total_count, file, extents, extents);
			IN_FTW_PRINT_ERR_MSG(
				"File is not current user's file"
				" or current user is not root");
		}
		return -1;
	}

	/* Lock status */
	if (fcntl(fd, F_GETLK, &lock) < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(
				"Failed to get lock information");
		}
		return -1;
	} else if (lock.l_type != F_UNLCK) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			IN_FTW_PRINT_ERR_MSG("File has been locked");
		}
		return -1;
	}

	return 0;
}

/*
 * insert_extent_by_logical() -	Sequentially insert extent by logical.
 *
 * @ext_list_head:	the head of logical extent list.
 * @ext:		the extent element which will be inserted.
 */
static int insert_extent_by_logical(struct fiemap_extent_list **ext_list_head,
			struct fiemap_extent_list *ext)
{
	struct fiemap_extent_list	*ext_list_tmp = *ext_list_head;

	if (ext == NULL)
		goto out;

	/* First element */
	if (*ext_list_head == NULL) {
		(*ext_list_head) = ext;
		(*ext_list_head)->prev = *ext_list_head;
		(*ext_list_head)->next = *ext_list_head;
		return 0;
	}

	if (ext->data.logical <= ext_list_tmp->data.logical) {
		/* Insert before head */
		if (ext_list_tmp->data.logical <
			ext->data.logical + ext->data.len)
			/* Overlap */
			goto out;
		/* Adjust head */
		*ext_list_head = ext;
	} else {
		/* Insert into the middle or last of the list */
		do {
			if (ext->data.logical < ext_list_tmp->data.logical)
				break;
			ext_list_tmp = ext_list_tmp->next;
		} while (ext_list_tmp != (*ext_list_head));
		if (ext->data.logical <
		    ext_list_tmp->prev->data.logical +
			ext_list_tmp->prev->data.len)
			/* Overlap */
			goto out;

		if (ext_list_tmp != *ext_list_head &&
		    ext_list_tmp->data.logical <
		    ext->data.logical + ext->data.len)
			/* Overlap */
			goto out;
	}
	ext_list_tmp = ext_list_tmp->prev;
	/* Insert "ext" after "ext_list_tmp" */
	insert(ext_list_tmp, ext);
	return 0;
out:
	errno = EINVAL;
	return -1;
}

/*
 * insert_extent_by_physical() -	Sequentially insert extent by physical.
 *
 * @ext_list_head:	the head of physical extent list.
 * @ext:		the extent element which will be inserted.
 */
static int insert_extent_by_physical(struct fiemap_extent_list **ext_list_head,
			struct fiemap_extent_list *ext)
{
	struct fiemap_extent_list	*ext_list_tmp = *ext_list_head;

	if (ext == NULL)
		goto out;

	/* First element */
	if (*ext_list_head == NULL) {
		(*ext_list_head) = ext;
		(*ext_list_head)->prev = *ext_list_head;
		(*ext_list_head)->next = *ext_list_head;
		return 0;
	}

	if (ext->data.physical <= ext_list_tmp->data.physical) {
		/* Insert before head */
		if (ext_list_tmp->data.physical <
					ext->data.physical + ext->data.len)
			/* Overlap */
			goto out;
		/* Adjust head */
		*ext_list_head = ext;
	} else {
		/* Insert into the middle or last of the list */
		do {
			if (ext->data.physical < ext_list_tmp->data.physical)
				break;
			ext_list_tmp = ext_list_tmp->next;
		} while (ext_list_tmp != (*ext_list_head));
		if (ext->data.physical <
		    ext_list_tmp->prev->data.physical +
				ext_list_tmp->prev->data.len)
			/* Overlap */
			goto out;

		if (ext_list_tmp != *ext_list_head &&
		    ext_list_tmp->data.physical <
				ext->data.physical + ext->data.len)
			/* Overlap */
			goto out;
	}
	ext_list_tmp = ext_list_tmp->prev;
	/* Insert "ext" after "ext_list_tmp" */
	insert(ext_list_tmp, ext);
	return 0;
out:
	errno = EINVAL;
	return -1;
}

/*
 * insert_exts_group() -	Insert a exts_group.
 *
 * @ext_group_head:		the head of a exts_group list.
 * @exts_group:			the exts_group element which will be inserted.
 */
static int insert_exts_group(struct fiemap_extent_group **ext_group_head,
				struct fiemap_extent_group *exts_group)
{
	struct fiemap_extent_group	*ext_group_tmp = NULL;

	if (exts_group == NULL) {
		errno = EINVAL;
		return -1;
	}

	/* Initialize list */
	if (*ext_group_head == NULL) {
		(*ext_group_head) = exts_group;
		(*ext_group_head)->prev = *ext_group_head;
		(*ext_group_head)->next = *ext_group_head;
		return 0;
	}

	ext_group_tmp = (*ext_group_head)->prev;
	insert(ext_group_tmp, exts_group);

	return 0;
}

/*
 * join_extents() -		Find continuous region(exts_group).
 *
 * @ext_list_head:		the head of the extent list.
 * @ext_group_head:		the head of the target exts_group list.
 */
static int join_extents(struct fiemap_extent_list *ext_list_head,
		struct fiemap_extent_group **ext_group_head)
{
	__u64	len = ext_list_head->data.len;
	struct fiemap_extent_list *ext_list_start = ext_list_head;
	struct fiemap_extent_list *ext_list_tmp = ext_list_head->next;

	do {
		struct fiemap_extent_group	*ext_group_tmp = NULL;

		/* This extent and previous extent are not continuous,
		 * so, all previous extents are treated as an extent group.
		 */
		if ((ext_list_tmp->prev->data.logical +
			ext_list_tmp->prev->data.len)
				!= ext_list_tmp->data.logical) {
			ext_group_tmp =
				malloc(sizeof(struct fiemap_extent_group));
			if (ext_group_tmp == NULL)
				return -1;

			memset(ext_group_tmp, 0,
				sizeof(struct fiemap_extent_group));
			ext_group_tmp->len = len;
			ext_group_tmp->start = ext_list_start;
			ext_group_tmp->end = ext_list_tmp->prev;

			if (insert_exts_group(ext_group_head,
				ext_group_tmp) < 0) {
				FREE(ext_group_tmp);
				return -1;
			}
			ext_list_start = ext_list_tmp;
			len = ext_list_tmp->data.len;
			ext_list_tmp = ext_list_tmp->next;
			continue;
		}

		/* This extent and previous extent are continuous,
		 * so, they belong to the same extent group, and we check
		 * if the next extent belongs to the same extent group.
		 */
		len += ext_list_tmp->data.len;
		ext_list_tmp = ext_list_tmp->next;
	} while (ext_list_tmp != ext_list_head->next);

	return 0;
}

/*
 * get_file_extents() -	Get file's extent list.
 *
 * @fd:			defrag target file's descriptor.
 * @ext_list_head:	the head of the extent list.
 */
static int get_file_extents(int fd, struct fiemap_extent_list **ext_list_head)
{
	__u32	i;
	int	ret;
	int	ext_buf_size, fie_buf_size;
	__u64	pos = 0;
	struct fiemap	*fiemap_buf = NULL;
	struct fiemap_extent	*ext_buf = NULL;
	struct fiemap_extent_list	*ext_list = NULL;

	/* Convert units, in bytes.
	 * Be careful : now, physical block number in extent is 48bit,
	 * and the maximum blocksize for ext4 is 4K(12bit),
	 * so there is no overflow, but in future it may be changed.
	 */

	/* Alloc space for fiemap */
	ext_buf_size = EXTENT_MAX_COUNT * sizeof(struct fiemap_extent);
	fie_buf_size = sizeof(struct fiemap) + ext_buf_size;

	fiemap_buf = malloc(fie_buf_size);
	if (fiemap_buf == NULL)
		return -1;

	ext_buf = fiemap_buf->fm_extents;
	memset(fiemap_buf, 0, fie_buf_size);
	fiemap_buf->fm_length = FIEMAP_MAX_OFFSET;
	fiemap_buf->fm_flags |= FIEMAP_FLAG_SYNC;
	fiemap_buf->fm_extent_count = EXTENT_MAX_COUNT;

	do {
		fiemap_buf->fm_start = pos;
		memset(ext_buf, 0, ext_buf_size);
		ret = ioctl(fd, FS_IOC_FIEMAP, fiemap_buf);
		if (ret < 0 || fiemap_buf->fm_mapped_extents == 0)
			goto out;
		for (i = 0; i < fiemap_buf->fm_mapped_extents; i++) {
			ext_list = NULL;
			ext_list = malloc(sizeof(struct fiemap_extent_list));
			if (ext_list == NULL)
				goto out;

			ext_list->data.physical = ext_buf[i].fe_physical
						/ block_size;
			ext_list->data.logical = ext_buf[i].fe_logical
						/ block_size;
			ext_list->data.len = ext_buf[i].fe_length
						/ block_size;

			ret = insert_extent_by_physical(
					ext_list_head, ext_list);
			if (ret < 0) {
				FREE(ext_list);
				goto out;
			}
		}
		/* Record file's logical offset this time */
		pos = ext_buf[EXTENT_MAX_COUNT-1].fe_logical +
			ext_buf[EXTENT_MAX_COUNT-1].fe_length;
		/*
		 * If fm_extents array has been filled and
		 * there are extents left, continue to cycle.
		 */
	} while (fiemap_buf->fm_mapped_extents
					== EXTENT_MAX_COUNT &&
		!(ext_buf[EXTENT_MAX_COUNT-1].fe_flags
					& FIEMAP_EXTENT_LAST));

	FREE(fiemap_buf);
	return 0;
out:
	FREE(fiemap_buf);
	return -1;
}

/*
 * get_logical_count() -	Get the file logical extents count.
 *
 * @logical_list_head:	the head of the logical extent list.
 */
static int get_logical_count(struct fiemap_extent_list *logical_list_head)
{
	int ret = 0;
	struct fiemap_extent_list *ext_list_tmp  = logical_list_head;

	do {
		ret++;
		ext_list_tmp = ext_list_tmp->next;
	} while (ext_list_tmp != logical_list_head);

	return ret;
}

/*
 * get_physical_count() -	Get the file physical extents count.
 *
 * @physical_list_head:	the head of the physical extent list.
 */
static int get_physical_count(struct fiemap_extent_list *physical_list_head)
{
	int ret = 0;
	struct fiemap_extent_list *ext_list_tmp = physical_list_head;

	do {
		if ((ext_list_tmp->data.physical + ext_list_tmp->data.len)
				!= ext_list_tmp->next->data.physical) {
			/* This extent and next extent are not continuous. */
			ret++;
		}

		ext_list_tmp = ext_list_tmp->next;
	} while (ext_list_tmp != physical_list_head);

	return ret;
}

/*
 * change_physical_to_logical() -	Change list from physical to logical.
 *
 * @physical_list_head:	the head of physical extent list.
 * @logical_list_head:	the head of logical extent list.
 */
static int change_physical_to_logical(
			struct fiemap_extent_list **physical_list_head,
			struct fiemap_extent_list **logical_list_head)
{
	int ret;
	struct fiemap_extent_list *ext_list_tmp = *physical_list_head;
	struct fiemap_extent_list *ext_list_next = ext_list_tmp->next;

	while (1) {
		if (ext_list_tmp == ext_list_next) {
			ret = insert_extent_by_logical(
				logical_list_head, ext_list_tmp);
			if (ret < 0)
				return -1;

			*physical_list_head = NULL;
			break;
		}

		ext_list_tmp->prev->next = ext_list_tmp->next;
		ext_list_tmp->next->prev = ext_list_tmp->prev;
		*physical_list_head = ext_list_next;

		ret = insert_extent_by_logical(
			logical_list_head, ext_list_tmp);
		if (ret < 0) {
			FREE(ext_list_tmp);
			return -1;
		}
		ext_list_tmp = ext_list_next;
		ext_list_next = ext_list_next->next;
	}

	return 0;
}

/* get_file_blocks() -  Get total file blocks.
 *
 * @ext_list_head:	the extent list head of the target file
 */
static ext4_fsblk_t get_file_blocks(struct fiemap_extent_list *ext_list_head)
{
	ext4_fsblk_t blk_count = 0;
	struct fiemap_extent_list *ext_list_tmp = ext_list_head;

	do {
		blk_count += ext_list_tmp->data.len;
		ext_list_tmp = ext_list_tmp->next;
	} while (ext_list_tmp != ext_list_head);

	return blk_count;
}

/*
 * free_ext() -		Free the extent list.
 *
 * @ext_list_head:	the extent list head of which will be free.
 */
static void free_ext(struct fiemap_extent_list *ext_list_head)
{
	struct fiemap_extent_list	*ext_list_tmp = NULL;

	if (ext_list_head == NULL)
		return;

	while (ext_list_head->next != ext_list_head) {
		ext_list_tmp = ext_list_head;
		ext_list_head->prev->next = ext_list_head->next;
		ext_list_head->next->prev = ext_list_head->prev;
		ext_list_head = ext_list_head->next;
		free(ext_list_tmp);
	}
	free(ext_list_head);
}

/*
 * free_exts_group() -		Free the exts_group.
 *
 * @*ext_group_head:	the exts_group list head which will be free.
 */
static void free_exts_group(struct fiemap_extent_group *ext_group_head)
{
	struct fiemap_extent_group	*ext_group_tmp = NULL;

	if (ext_group_head == NULL)
		return;

	while (ext_group_head->next != ext_group_head) {
		ext_group_tmp = ext_group_head;
		ext_group_head->prev->next = ext_group_head->next;
		ext_group_head->next->prev = ext_group_head->prev;
		ext_group_head = ext_group_head->next;
		free(ext_group_tmp);
	}
	free(ext_group_head);
}

/*
 * get_best_count() -	Get the file best extents count.
 *
 * @block_count:		the file's physical block count.
 */
static int get_best_count(ext4_fsblk_t block_count)
{
	int ret;
	unsigned int flex_bg_num;

	/* Calcuate best extents count */
	if (feature_incompat & EXT4_FEATURE_INCOMPAT_FLEX_BG) {
		flex_bg_num = 1 << log_groups_per_flex;
		ret = ((block_count - 1) /
			((ext4_fsblk_t)blocks_per_group *
				flex_bg_num)) + 1;
	} else
		ret = ((block_count - 1) / blocks_per_group) + 1;

	return ret;
}


/*
 * file_statistic() -	Get statistic info of the file's fragments.
 *
 * @file:		the file's name.
 * @buf:		the pointer of the struct stat64.
 * @flag:		file type.
 * @ftwbuf:		the pointer of a struct FTW.
 */
static int file_statistic(const char *file, const struct stat64 *buf,
			int flag EXT2FS_ATTR((unused)),
			struct FTW *ftwbuf EXT2FS_ATTR((unused)))
{
	int	fd;
	int	ret;
	int	now_ext_count, best_ext_count = 0, physical_ext_count;
	int	i, j;
	__u64	size_per_ext = 0;
	float	ratio = 0.0;
	ext4_fsblk_t	blk_count = 0;
	char	msg_buffer[PATH_MAX + 24];
	struct fiemap_extent_list *physical_list_head = NULL;
	struct fiemap_extent_list *logical_list_head = NULL;

	defraged_file_count++;

	if (mode_flag & DETAIL) {
		if (total_count == 1 && regular_count == 1)
			printf("<File>\n");
		else {
			printf("[%u/%u]", defraged_file_count, total_count);
			fflush(stdout);
		}
	}
	if (lost_found_dir[0] != '\0' &&
	    !memcmp(file, lost_found_dir, strnlen(lost_found_dir, PATH_MAX))) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG(NGMSG_LOST_FOUND);
		}
			return 0;
	}

	if (!S_ISREG(buf->st_mode)) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG(NGMSG_FILE_UNREG);
		}
		return 0;
	}

	/* Access authority */
	if (current_uid != ROOT_UID &&
		buf->st_uid != current_uid) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG(
				"File is not current user's file"
				" or current user is not root");
		}
		return 0;
	}

	/* Empty file */
	if (buf->st_size == 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG("File size is 0");
		}
		return 0;
	}

	/* Has no blocks */
	if (buf->st_blocks == 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG("File has no blocks");
		}
		return 0;
	}

	fd = open64(file, O_RDONLY);
	if (fd < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG_WITH_ERRNO(NGMSG_FILE_OPEN);
		}
		return 0;
	}

	/* Get file's physical extents  */
	ret = get_file_extents(fd, &physical_list_head);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG_WITH_ERRNO(NGMSG_FILE_EXTENT);
		}
		goto out;
	}

	/* Get the count of file's continuous physical region */
	physical_ext_count = get_physical_count(physical_list_head);

	/* Change list from physical to logical */
	ret = change_physical_to_logical(&physical_list_head,
							&logical_list_head);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG_WITH_ERRNO(NGMSG_FILE_EXTENT);
		}
		goto out;
	}

	/* Count file fragments before defrag */
	now_ext_count = get_logical_count(logical_list_head);

	if (current_uid == ROOT_UID) {
		/* Calculate the size per extent */
		blk_count = get_file_blocks(logical_list_head);

		best_ext_count = get_best_count(blk_count);

		/* e4defrag rounds size_per_ext up to a block size boundary */
		size_per_ext = blk_count * (buf->st_blksize / 1024) /
							now_ext_count;

		ratio = (float)(physical_ext_count - best_ext_count) * 100 /
							blk_count;

		extents_before_defrag += now_ext_count;
		extents_after_defrag += best_ext_count;
		files_block_count += blk_count;
	}

	if (total_count == 1 && regular_count == 1) {
		/* File only */
		if (mode_flag & DETAIL) {
			int count = 0;
			struct fiemap_extent_list *ext_list_tmp =
						logical_list_head;

			/* Print extents info */
			do {
				count++;
				printf("[ext %d]:\tstart %llu:\tlogical "
						"%llu:\tlen %llu\n", count,
						ext_list_tmp->data.physical,
						ext_list_tmp->data.logical,
						ext_list_tmp->data.len);
				ext_list_tmp = ext_list_tmp->next;
			} while (ext_list_tmp != logical_list_head);

		} else {
			printf("%-40s%10s/%-10s%9s\n",
					"<File>", "now", "best", "size/ext");
			if (current_uid == ROOT_UID) {
				if (strlen(file) > 40)
					printf("%s\n%50d/%-10d%6llu KB\n",
						file, now_ext_count,
						best_ext_count, size_per_ext);
				else
					printf("%-40s%10d/%-10d%6llu KB\n",
						file, now_ext_count,
						best_ext_count, size_per_ext);
			} else {
				if (strlen(file) > 40)
					printf("%s\n%50d/%-10s%7s\n",
							file, now_ext_count,
							"-", "-");
				else
					printf("%-40s%10d/%-10s%7s\n",
							file, now_ext_count,
							"-", "-");
			}
		}
		succeed_cnt++;
		goto out;
	}

	if (mode_flag & DETAIL) {
		/* Print statistic info */
		sprintf(msg_buffer, "[%u/%u]%s",
				defraged_file_count, total_count, file);
		if (current_uid == ROOT_UID) {
			if (strlen(msg_buffer) > 40)
				printf("\033[79;0H\033[K%s\n"
						"%50d/%-10d%6llu KB\n",
						msg_buffer, now_ext_count,
						best_ext_count, size_per_ext);
			else
				printf("\033[79;0H\033[K%-40s"
						"%10d/%-10d%6llu KB\n",
						msg_buffer, now_ext_count,
						best_ext_count, size_per_ext);
		} else {
			if (strlen(msg_buffer) > 40)
				printf("\033[79;0H\033[K%s\n%50d/%-10s%7s\n",
						msg_buffer, now_ext_count,
							"-", "-");
			else
				printf("\033[79;0H\033[K%-40s%10d/%-10s%7s\n",
						msg_buffer, now_ext_count,
							"-", "-");
		}
	}

	for (i = 0; i < SHOW_FRAG_FILES; i++) {
		if (ratio >= frag_rank[i].ratio) {
			for (j = SHOW_FRAG_FILES - 1; j > i; j--) {
				memset(&frag_rank[j], 0,
					sizeof(struct frag_statistic_ino));
				strncpy(frag_rank[j].msg_buffer,
					frag_rank[j - 1].msg_buffer,
					strnlen(frag_rank[j - 1].msg_buffer,
					PATH_MAX));
				frag_rank[j].now_count =
					frag_rank[j - 1].now_count;
				frag_rank[j].best_count =
					frag_rank[j - 1].best_count;
				frag_rank[j].size_per_ext =
					frag_rank[j - 1].size_per_ext;
				frag_rank[j].ratio =
					frag_rank[j - 1].ratio;
			}
			memset(&frag_rank[i], 0,
					sizeof(struct frag_statistic_ino));
			strncpy(frag_rank[i].msg_buffer, file,
						strnlen(file, PATH_MAX));
			frag_rank[i].now_count = now_ext_count;
			frag_rank[i].best_count = best_ext_count;
			frag_rank[i].size_per_ext = size_per_ext;
			frag_rank[i].ratio = ratio;
			break;
		}
	}

	succeed_cnt++;

out:
	close(fd);
	free_ext(physical_list_head);
	free_ext(logical_list_head);
	return 0;
}

/*
 * print_progress -	Print defrag progress
 *
 * @file:		file name.
 * @start:		logical offset for defrag target file
 * @file_size:		defrag target filesize
 */
static void print_progress(const char *file, loff_t start, loff_t file_size)
{
	int percent = (start * 100) / file_size;
	printf("\033[79;0H\033[K[%u/%u]%s:\t%3d%%",
		defraged_file_count, total_count, file, min(percent, 100));
	fflush(stdout);

	return;
}

/*
 * call_defrag() -	Execute the defrag program.
 *
 * @fd:			target file descriptor.
 * @donor_fd:		donor file descriptor.
 * @file:			target file name.
 * @buf:			pointer of the struct stat64.
 * @ext_list_head:	head of the extent list.
 */
static int call_defrag(int fd, int donor_fd, const char *file,
	const struct stat64 *buf, struct fiemap_extent_list *ext_list_head)
{
	loff_t	start = 0;
	unsigned int	page_num;
	unsigned char	*vec = NULL;
	int	defraged_ret = 0;
	int	ret;
	struct move_extent	move_data;
	struct fiemap_extent_list	*ext_list_tmp = NULL;

	memset(&move_data, 0, sizeof(struct move_extent));
	move_data.donor_fd = donor_fd;

	/* Print defrag progress */
	print_progress(file, start, buf->st_size);

	ext_list_tmp = ext_list_head;
	do {
		move_data.orig_start = ext_list_tmp->data.logical;
		/* Logical offset of orig and donor should be same */
		move_data.donor_start = move_data.orig_start;
		move_data.len = ext_list_tmp->data.len;
		move_data.moved_len = 0;

		ret = page_in_core(fd, move_data, &vec, &page_num);
		if (ret < 0) {
			if (mode_flag & DETAIL) {
				printf("\n");
				PRINT_ERR_MSG_WITH_ERRNO(
						"Failed to get file map");
			} else {
				printf("\t[ NG ]\n");
			}
			return -1;
		}

		/* EXT4_IOC_MOVE_EXT */
		defraged_ret =
			ioctl(fd, EXT4_IOC_MOVE_EXT, &move_data);

		/* Free pages */
		ret = defrag_fadvise(fd, move_data, vec, page_num);
		if (vec) {
			free(vec);
			vec = NULL;
		}
		if (ret < 0) {
			if (mode_flag & DETAIL) {
				printf("\n");
				PRINT_ERR_MSG_WITH_ERRNO(
					"Failed to free page");
			} else {
				printf("\t[ NG ]\n");
			}
			return -1;
		}

		if (defraged_ret < 0) {
			if (mode_flag & DETAIL) {
				printf("\n");
				PRINT_ERR_MSG_WITH_ERRNO(
					"Failed to defrag with "
					"EXT4_IOC_MOVE_EXT ioctl");
				if (errno == ENOTTY)
					printf("\tAt least 2.6.31-rc1 of "
						"vanilla kernel is required\n");
			} else {
				printf("\t[ NG ]\n");
			}
			return -1;
		}
		/* Adjust logical offset for next ioctl */
		move_data.orig_start += move_data.moved_len;
		move_data.donor_start = move_data.orig_start;

		start = move_data.orig_start * buf->st_blksize;

		/* Print defrag progress */
		print_progress(file, start, buf->st_size);

		/* End of file */
		if (start >= buf->st_size)
			break;

		ext_list_tmp = ext_list_tmp->next;
	} while (ext_list_tmp != ext_list_head);

	return 0;
}

/*
 * file_defrag() -		Check file attributes and call ioctl to defrag.
 *
 * @file:		the file's name.
 * @buf:		the pointer of the struct stat64.
 * @flag:		file type.
 * @ftwbuf:		the pointer of a struct FTW.
 */
static int file_defrag(const char *file, const struct stat64 *buf,
			int flag EXT2FS_ATTR((unused)),
			struct FTW *ftwbuf EXT2FS_ATTR((unused)))
{
	int	fd;
	int	donor_fd = -1;
	int	ret;
	int	best;
	int	file_frags_start, file_frags_end;
	int	orig_physical_cnt, donor_physical_cnt = 0;
	char	tmp_inode_name[PATH_MAX + 8];
	ext4_fsblk_t			blk_count = 0;
	struct fiemap_extent_list	*orig_list_physical = NULL;
	struct fiemap_extent_list	*orig_list_logical = NULL;
	struct fiemap_extent_list	*donor_list_physical = NULL;
	struct fiemap_extent_list	*donor_list_logical = NULL;
	struct fiemap_extent_group	*orig_group_head = NULL;
	struct fiemap_extent_group	*orig_group_tmp = NULL;

	defraged_file_count++;

	if (mode_flag & DETAIL) {
		printf("[%u/%u]", defraged_file_count, total_count);
		fflush(stdout);
	}

	if (lost_found_dir[0] != '\0' &&
	    !memcmp(file, lost_found_dir, strnlen(lost_found_dir, PATH_MAX))) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			IN_FTW_PRINT_ERR_MSG(NGMSG_LOST_FOUND);
		}
		return 0;
	}

	if (!S_ISREG(buf->st_mode)) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			IN_FTW_PRINT_ERR_MSG(NGMSG_FILE_UNREG);
		}
		return 0;
	}

	/* Empty file */
	if (buf->st_size == 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			IN_FTW_PRINT_ERR_MSG("File size is 0");
		}
		return 0;
	}

	/* Has no blocks */
	if (buf->st_blocks == 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			STATISTIC_ERR_MSG("File has no blocks");
		}
		return 0;
	}

	fd = open64(file, O_RDWR);
	if (fd < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_OPEN);
		}
		return 0;
	}

	/* Get file's extents */
	ret = get_file_extents(fd, &orig_list_physical);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_EXTENT);
		}
		goto out;
	}

	/* Get the count of file's continuous physical region */
	orig_physical_cnt = get_physical_count(orig_list_physical);

	/* Change list from physical to logical */
	ret = change_physical_to_logical(&orig_list_physical,
							&orig_list_logical);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_EXTENT);
		}
		goto out;
	}

	/* Count file fragments before defrag */
	file_frags_start = get_logical_count(orig_list_logical);

	blk_count = get_file_blocks(orig_list_logical);
	if (file_check(fd, buf, file, file_frags_start, blk_count) < 0)
		goto out;

	if (fsync(fd) < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO("Failed to sync(fsync)");
		}
		goto out;
	}

	if (current_uid == ROOT_UID)
		best = get_best_count(blk_count);
	else
		best = 1;

	if (file_frags_start <= best)
		goto check_improvement;

	/* Combine extents to group */
	ret = join_extents(orig_list_logical, &orig_group_head);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_EXTENT);
		}
		goto out;
	}

	/* Create donor inode */
	memset(tmp_inode_name, 0, PATH_MAX + 8);
	sprintf(tmp_inode_name, "%.*s.defrag",
				(int)strnlen(file, PATH_MAX), file);
	donor_fd = open64(tmp_inode_name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR);
	if (donor_fd < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			if (errno == EEXIST)
				PRINT_ERR_MSG_WITH_ERRNO(
				"File is being defraged by other program");
			else
				PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_OPEN);
		}
		goto out;
	}

	/* Unlink donor inode */
	ret = unlink(tmp_inode_name);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO("Failed to unlink");
		}
		goto out;
	}

	/* Allocate space for donor inode */
	orig_group_tmp = orig_group_head;
	do {
		ret = fallocate64(donor_fd, 0,
		  (loff_t)orig_group_tmp->start->data.logical * block_size,
		  (loff_t)orig_group_tmp->len * block_size);
		if (ret < 0) {
			if (mode_flag & DETAIL) {
				PRINT_FILE_NAME(file);
				PRINT_ERR_MSG_WITH_ERRNO("Failed to fallocate");
			}
			goto out;
		}

		orig_group_tmp = orig_group_tmp->next;
	} while (orig_group_tmp != orig_group_head);

	/* Get donor inode's extents */
	ret = get_file_extents(donor_fd, &donor_list_physical);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_EXTENT);
		}
		goto out;
	}

	/* Calcuate donor inode's continuous physical region */
	donor_physical_cnt = get_physical_count(donor_list_physical);

	/* Change donor extent list from physical to logical */
	ret = change_physical_to_logical(&donor_list_physical,
							&donor_list_logical);
	if (ret < 0) {
		if (mode_flag & DETAIL) {
			PRINT_FILE_NAME(file);
			PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_EXTENT);
		}
		goto out;
	}

check_improvement:
	if (mode_flag & DETAIL) {
		if (file_frags_start != 1)
			frag_files_before_defrag++;

		extents_before_defrag += file_frags_start;
	}

	if (file_frags_start <= best ||
			orig_physical_cnt <= donor_physical_cnt) {
		printf("\033[79;0H\033[K[%u/%u]%s:\t%3d%%",
			defraged_file_count, total_count, file, 100);
		if (mode_flag & DETAIL)
			printf("  extents: %d -> %d",
				file_frags_start, file_frags_start);

		printf("\t[ OK ]\n");
		succeed_cnt++;

		if (file_frags_start != 1)
			frag_files_after_defrag++;

		extents_after_defrag += file_frags_start;
		goto out;
	}

	/* Defrag the file */
	ret = call_defrag(fd, donor_fd, file, buf, donor_list_logical);

	/* Count file fragments after defrag and print extents info */
	if (mode_flag & DETAIL) {
		file_frags_end = file_frag_count(fd);
		if (file_frags_end < 0) {
			printf("\n");
			PRINT_ERR_MSG_WITH_ERRNO(NGMSG_FILE_INFO);
			goto out;
		}

		if (file_frags_end != 1)
			frag_files_after_defrag++;

		extents_after_defrag += file_frags_end;

		if (ret < 0)
			goto out;

		printf("  extents: %d -> %d",
			file_frags_start, file_frags_end);
		fflush(stdout);
	}

	if (ret < 0)
		goto out;

	printf("\t[ OK ]\n");
	fflush(stdout);
	succeed_cnt++;

out:
	close(fd);
	if (donor_fd != -1)
		close(donor_fd);
	free_ext(orig_list_physical);
	free_ext(orig_list_logical);
	free_ext(donor_list_physical);
	free_exts_group(orig_group_head);
	return 0;
}

/*
 * main() -		Ext4 online defrag.
 *
 * @argc:		the number of parameter.
 * @argv[]:		the pointer array of parameter.
 */
int main(int argc, char *argv[])
{
	int	opt;
	int	i, j, ret = 0;
	int	flags = FTW_PHYS | FTW_MOUNT;
	int	arg_type = -1;
	int	success_flag = 0;
	char	dir_name[PATH_MAX + 1];
	char	dev_name[PATH_MAX + 1];
	struct stat64	buf;
	ext2_filsys fs = NULL;

	/* Parse arguments */
	if (argc == 1)
		goto out;

	while ((opt = getopt(argc, argv, "vc")) != EOF) {
		switch (opt) {
		case 'v':
			mode_flag |= DETAIL;
			break;
		case 'c':
			mode_flag |= STATISTIC;
			break;
		default:
			goto out;
		}
	}

	if (argc == optind)
		goto out;

	current_uid = getuid();

	/* Main process */
	for (i = optind; i < argc; i++) {
		succeed_cnt = 0;
		regular_count = 0;
		total_count = 0;
		frag_files_before_defrag = 0;
		frag_files_after_defrag = 0;
		extents_before_defrag = 0;
		extents_after_defrag = 0;
		defraged_file_count = 0;
		files_block_count = 0;
		blocks_per_group = 0;
		feature_incompat = 0;
		log_groups_per_flex = 0;

		memset(dir_name, 0, PATH_MAX + 1);
		memset(dev_name, 0, PATH_MAX + 1);
		memset(lost_found_dir, 0, PATH_MAX + 1);
		memset(frag_rank, 0,
			sizeof(struct frag_statistic_ino) * SHOW_FRAG_FILES);

		if ((mode_flag & STATISTIC) && i > optind)
			printf("\n");

#if BYTE_ORDER != BIG_ENDIAN && BYTE_ORDER != LITTLE_ENDIAN
		PRINT_ERR_MSG("Endian's type is not big/little endian");
		PRINT_FILE_NAME(argv[i]);
		continue;
#endif

		if (lstat64(argv[i], &buf) < 0) {
			perror(NGMSG_FILE_INFO);
			PRINT_FILE_NAME(argv[i]);
			continue;
		}

		/* Handle i.e. lvm device symlinks */
		if (S_ISLNK(buf.st_mode)) {
			struct stat64	buf2;

			if (stat64(argv[i], &buf2) == 0 &&
			    S_ISBLK(buf2.st_mode))
				buf = buf2;
		}

		if (S_ISBLK(buf.st_mode)) {
			/* Block device */
			strncpy(dev_name, argv[i], strnlen(argv[i], PATH_MAX));
			if (get_mount_point(argv[i], dir_name, PATH_MAX) < 0)
				continue;
			if (lstat64(dir_name, &buf) < 0) {
				perror(NGMSG_FILE_INFO);
				PRINT_FILE_NAME(argv[i]);
				continue;
			}
			arg_type = DEVNAME;
			if (!(mode_flag & STATISTIC))
				printf("ext4 defragmentation for device(%s)\n",
					argv[i]);
		} else if (S_ISDIR(buf.st_mode)) {
			/* Directory */
			if (access(argv[i], R_OK) < 0) {
				perror(argv[i]);
				continue;
			}
			arg_type = DIRNAME;
			strncpy(dir_name, argv[i], strnlen(argv[i], PATH_MAX));
		} else if (S_ISREG(buf.st_mode)) {
			/* Regular file */
			arg_type = FILENAME;
		} else {
			/* Irregular file */
			PRINT_ERR_MSG(NGMSG_FILE_UNREG);
			PRINT_FILE_NAME(argv[i]);
			continue;
		}

		/* Set blocksize */
		block_size = buf.st_blksize;

		/* For device case,
		 * filesystem type checked in get_mount_point()
		 */
		if (arg_type == FILENAME || arg_type == DIRNAME) {
			if (is_ext4(argv[i], dev_name) < 0)
				continue;
			if (realpath(argv[i], dir_name) == NULL) {
				perror("Couldn't get full path");
				PRINT_FILE_NAME(argv[i]);
				continue;
			}
		}

		if (current_uid == ROOT_UID) {
			/* Get super block info */
			ret = ext2fs_open(dev_name, 0, 0, block_size,
					unix_io_manager, &fs);
			if (ret) {
				if (mode_flag & DETAIL) {
					perror("Can't get super block info");
					PRINT_FILE_NAME(argv[i]);
				}
				continue;
			}

			blocks_per_group = fs->super->s_blocks_per_group;
			feature_incompat = fs->super->s_feature_incompat;
			log_groups_per_flex = fs->super->s_log_groups_per_flex;

			ext2fs_close(fs);
		}

		switch (arg_type) {
		case DIRNAME:
			if (!(mode_flag & STATISTIC))
				printf("ext4 defragmentation "
					"for directory(%s)\n", argv[i]);

			int mount_dir_len = 0;
			mount_dir_len = strnlen(lost_found_dir, PATH_MAX);

			strncat(lost_found_dir, "/lost+found",
				PATH_MAX - strnlen(lost_found_dir, PATH_MAX));

			/* Not the case("e4defrag  mount_piont_dir") */
			if (dir_name[mount_dir_len] != '\0') {
				/*
				 * "e4defrag mount_piont_dir/lost+found"
				 * or "e4defrag mount_piont_dir/lost+found/"
				 */
				if (strncmp(lost_found_dir, dir_name,
					    strnlen(lost_found_dir,
						    PATH_MAX)) == 0 &&
				    (dir_name[strnlen(lost_found_dir,
						      PATH_MAX)] == '\0' ||
				     dir_name[strnlen(lost_found_dir,
						      PATH_MAX)] == '/')) {
					PRINT_ERR_MSG(NGMSG_LOST_FOUND);
					PRINT_FILE_NAME(argv[i]);
					continue;
				}

				/* "e4defrag mount_piont_dir/else_dir" */
				memset(lost_found_dir, 0, PATH_MAX + 1);
			}
		case DEVNAME:
			if (arg_type == DEVNAME) {
				strncpy(lost_found_dir, dir_name,
					strnlen(dir_name, PATH_MAX));
				strncat(lost_found_dir, "/lost+found/",
					PATH_MAX - strnlen(lost_found_dir,
							   PATH_MAX));
			}

			nftw64(dir_name, calc_entry_counts, FTW_OPEN_FD, flags);

			if (mode_flag & STATISTIC) {
				if (mode_flag & DETAIL)
					printf("%-40s%10s/%-10s%9s\n",
					"<File>", "now", "best", "size/ext");

				if (!(mode_flag & DETAIL) &&
						current_uid != ROOT_UID) {
					printf(" Done.\n");
					success_flag = 1;
					continue;
				}

				nftw64(dir_name, file_statistic,
							FTW_OPEN_FD, flags);

				if (succeed_cnt != 0 &&
					current_uid == ROOT_UID) {
					if (mode_flag & DETAIL)
						printf("\n");
					printf("%-40s%10s/%-10s%9s\n",
						"<Fragmented files>", "now",
						"best", "size/ext");
					for (j = 0; j < SHOW_FRAG_FILES; j++) {
						if (strlen(frag_rank[j].
							msg_buffer) > 37) {
							printf("%d. %s\n%50d/"
							"%-10d%6llu KB\n",
							j + 1,
							frag_rank[j].msg_buffer,
							frag_rank[j].now_count,
							frag_rank[j].best_count,
							frag_rank[j].
								size_per_ext);
						} else if (strlen(frag_rank[j].
							msg_buffer) > 0) {
							printf("%d. %-37s%10d/"
							"%-10d%6llu KB\n",
							j + 1,
							frag_rank[j].msg_buffer,
							frag_rank[j].now_count,
							frag_rank[j].best_count,
							frag_rank[j].
								size_per_ext);
						} else
							break;
					}
				}
				break;
			}
			/* File tree walk */
			nftw64(dir_name, file_defrag, FTW_OPEN_FD, flags);
			printf("\n\tSuccess:\t\t\t[ %u/%u ]\n", succeed_cnt,
				total_count);
			printf("\tFailure:\t\t\t[ %u/%u ]\n",
				total_count - succeed_cnt, total_count);
			if (mode_flag & DETAIL) {
				printf("\tTotal extents:\t\t\t%4d->%d\n",
					extents_before_defrag,
					extents_after_defrag);
				printf("\tFragmented percentage:\t\t"
					"%3llu%%->%llu%%\n",
					!regular_count ? 0 :
					((unsigned long long)
					frag_files_before_defrag * 100) /
					regular_count,
					!regular_count ? 0 :
					((unsigned long long)
					frag_files_after_defrag * 100) /
					regular_count);
			}
			break;
		case FILENAME:
			total_count = 1;
			regular_count = 1;
			strncat(lost_found_dir, "/lost+found/",
				PATH_MAX - strnlen(lost_found_dir,
						   PATH_MAX));
			if (strncmp(lost_found_dir, dir_name,
				    strnlen(lost_found_dir,
					    PATH_MAX)) == 0) {
				PRINT_ERR_MSG(NGMSG_LOST_FOUND);
				PRINT_FILE_NAME(argv[i]);
				continue;
			}

			if (mode_flag & STATISTIC) {
				file_statistic(argv[i], &buf, FTW_F, NULL);
				break;
			} else
				printf("ext4 defragmentation for %s\n",
								 argv[i]);
			/* Defrag single file process */
			file_defrag(argv[i], &buf, FTW_F, NULL);
			if (succeed_cnt != 0)
				printf(" Success:\t\t\t[1/1]\n");
			else
				printf(" Success:\t\t\t[0/1]\n");

			break;
		}

		if (succeed_cnt != 0)
			success_flag = 1;
		if (mode_flag & STATISTIC) {
			if (current_uid != ROOT_UID) {
				printf(" Done.\n");
				continue;
			}

			if (!succeed_cnt) {
				if (mode_flag & DETAIL)
					printf("\n");

				if (arg_type == DEVNAME)
					printf(" In this device(%s), "
					"none can be defragmented.\n", argv[i]);
				else if (arg_type == DIRNAME)
					printf(" In this directory(%s), "
					"none can be defragmented.\n", argv[i]);
				else
					printf(" This file(%s) "
					"can't be defragmented.\n", argv[i]);
			} else {
				float files_ratio = 0.0;
				float score = 0.0;
				__u64 size_per_ext = files_block_count *
						(buf.st_blksize / 1024) /
						extents_before_defrag;
				files_ratio = (float)(extents_before_defrag -
						extents_after_defrag) *
						100 / files_block_count;
				score = CALC_SCORE(files_ratio);
				printf("\n Total/best extents\t\t\t\t%d/%d\n"
					" Average size per extent"
					"\t\t\t%llu KB\n"
					" Fragmentation score\t\t\t\t%.0f\n",
						extents_before_defrag,
						extents_after_defrag,
						size_per_ext, score);
				printf(" [0-30 no problem:"
					" 31-55 a little bit fragmented:"
					" 56- needs defrag]\n");

				if (arg_type == DEVNAME)
					printf(" This device (%s) ", argv[i]);
				else if (arg_type == DIRNAME)
					printf(" This directory (%s) ",
								argv[i]);
				else
					printf(" This file (%s) ", argv[i]);

				if (score > BOUND_SCORE)
					printf("needs defragmentation.\n");
				else
					printf("does not need "
							"defragmentation.\n");
			}
			printf(" Done.\n");
		}

	}

	if (success_flag)
		return 0;

	exit(1);

out:
	printf(MSG_USAGE);
	exit(1);
}

