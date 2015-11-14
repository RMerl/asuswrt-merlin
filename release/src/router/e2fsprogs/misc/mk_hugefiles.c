/*
 * mk_hugefiles.c -- create huge files
 */

#define _XOPEN_SOURCE 600 /* for inclusion of PATH_MAX in Solaris */
#define _BSD_SOURCE	  /* for makedev() and major() */

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#ifdef __linux__
#include <sys/utsname.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>
#include <limits.h>
#include <blkid/blkid.h>

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fsP.h"
#include "et/com_err.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"
#include "ext2fs/ext2fs.h"
#include "util.h"
#include "profile.h"
#include "prof_err.h"
#include "nls-enable.h"
#include "mke2fs.h"

static int uid;
static int gid;
static blk64_t num_blocks;
static blk64_t num_slack;
static unsigned long num_files;
static blk64_t goal;
static char *fn_prefix;
static int idx_digits;
static char *fn_buf;
static char *fn_numbuf;
int zero_hugefile = 1;

#define SYSFS_PATH_LEN 256
typedef char sysfs_path_t[SYSFS_PATH_LEN];

#ifndef HAVE_SNPRINTF
/*
 * We are very careful to avoid needing to worry about buffer
 * overflows, so we don't really need to use snprintf() except as an
 * additional safety check.  So if snprintf() is not present, it's
 * safe to fall back to vsprintf().  This provides portability since
 * vsprintf() is guaranteed by C89, while snprintf() is only
 * guaranteed by C99 --- which for example, Microsoft Visual Studio
 * has *still* not bothered to implement.  :-/  (Not that I expect
 * mke2fs to be ported to MS Visual Studio any time soon, but
 * libext2fs *does* get built on Microsoft platforms, and we might
 * want to move this into libext2fs some day.)
 */
static int my_snprintf(char *str, size_t size, const char *format, ...)
{
	va_list	ap;
	int ret;

	va_start(ap, format);
	ret = vsprintf(str, format, ap);
	va_end(ap);
	return ret;
}

#define snprintf my_snprintf
#endif

/*
 * Fall back to Linux's definitions of makedev and major are needed.
 * The search_sysfs_block() function is highly unlikely to work on
 * non-Linux systems anyway.
 */
#ifndef makedev
#define makedev(maj, min) (((maj) << 8) + (min))
#endif

static char *search_sysfs_block(dev_t devno, sysfs_path_t ret_path)
{
	struct dirent	*de, *p_de;
	DIR		*dir = NULL, *p_dir = NULL;
	FILE		*f;
	sysfs_path_t	path, p_path;
	unsigned int	major, minor;
	char		*ret = ret_path;

	ret_path[0] = 0;
	if ((dir = opendir("/sys/block")) == NULL)
		return NULL;
	while ((de = readdir(dir)) != NULL) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") ||
		    strlen(de->d_name) > sizeof(path)-32)
			continue;
		snprintf(path, SYSFS_PATH_LEN,
			 "/sys/block/%s/dev", de->d_name);
		f = fopen(path, "r");
		if (f &&
		    (fscanf(f, "%u:%u", &major, &minor) == 2)) {
			fclose(f); f = NULL;
			if (makedev(major, minor) == devno) {
				snprintf(ret_path, SYSFS_PATH_LEN,
					 "/sys/block/%s", de->d_name);
				goto success;
			}
#ifdef major
			if (major(devno) != major)
				continue;
#endif
		}
		if (f)
			fclose(f);

		snprintf(path, SYSFS_PATH_LEN, "/sys/block/%s", de->d_name);

		if (p_dir)
			closedir(p_dir);
		if ((p_dir = opendir(path)) == NULL)
			continue;
		while ((p_de = readdir(p_dir)) != NULL) {
			if (!strcmp(p_de->d_name, ".") ||
			    !strcmp(p_de->d_name, "..") ||
			    (strlen(p_de->d_name) >
			     SYSFS_PATH_LEN - strlen(path) - 32))
				continue;
			snprintf(p_path, SYSFS_PATH_LEN, "%s/%s/dev",
				 path, p_de->d_name);

			f = fopen(p_path, "r");
			if (f &&
			    (fscanf(f, "%u:%u", &major, &minor) == 2) &&
			    (((major << 8) + minor) == devno)) {
				fclose(f);
				snprintf(ret_path, SYSFS_PATH_LEN, "%s/%s",
					 path, p_de->d_name);
				goto success;
			}
			if (f)
				fclose(f);
		}
	}
	ret = NULL;
success:
	if (dir)
		closedir(dir);
	if (p_dir)
		closedir(p_dir);
	return ret;
}

static blk64_t get_partition_start(const char *device_name)
{
	unsigned long long start;
	sysfs_path_t	path;
	struct stat	st;
	FILE		*f;
	char		*cp;
	int		n;

	if ((stat(device_name, &st) < 0) || !S_ISBLK(st.st_mode))
		return 0;

	cp = search_sysfs_block(st.st_rdev, path);
	if (!cp)
		return 0;
	if (strlen(path) > SYSFS_PATH_LEN - sizeof("/start"))
		return 0;
	strcat(path, "/start");
	f = fopen(path, "r");
	if (!f)
		return 0;
	n = fscanf(f, "%llu", &start);
	fclose(f);
	return (n == 1) ? start : 0;
}

static errcode_t create_directory(ext2_filsys fs, char *dir,
				  ext2_ino_t *ret_ino)

{
	struct ext2_inode	inode;
	ext2_ino_t		ino = EXT2_ROOT_INO;
	ext2_ino_t		newdir;
	errcode_t		retval = 0;
	char			*fn, *cp, *next;

	fn = malloc(strlen(dir) + 1);
	if (fn == NULL)
		return ENOMEM;

	strcpy(fn, dir);
	cp = fn;
	while(1) {
		next = strchr(cp, '/');
		if (next)
			*next++ = 0;
		if (*cp) {
			retval = ext2fs_new_inode(fs, ino, LINUX_S_IFDIR,
						  NULL, &newdir);
			if (retval)
				goto errout;

			retval = ext2fs_mkdir(fs, ino, newdir, cp);
			if (retval)
				goto errout;

			ino = newdir;
			retval = ext2fs_read_inode(fs, ino, &inode);
			if (retval)
				goto errout;

			inode.i_uid = uid & 0xFFFF;
			ext2fs_set_i_uid_high(inode, (uid >> 16) & 0xffff);
			inode.i_gid = gid & 0xFFFF;
			ext2fs_set_i_gid_high(inode, (gid >> 16) & 0xffff);
			retval = ext2fs_write_inode(fs, ino, &inode);
			if (retval)
				goto errout;
		}
		if (next == NULL || *next == '\0')
			break;
		cp = next;
	}
errout:
	free(fn);
	if (retval == 0)
		*ret_ino = ino;
	return retval;
}

static errcode_t mk_hugefile(ext2_filsys fs, blk64_t num,
			     ext2_ino_t dir, unsigned long idx, ext2_ino_t *ino)

{
	errcode_t		retval;
	blk64_t			lblk, bend = 0;
	__u64			size;
	blk64_t			left;
	blk64_t			count = 0;
	struct ext2_inode	inode;
	ext2_extent_handle_t	handle;

	retval = ext2fs_new_inode(fs, 0, LINUX_S_IFREG, NULL, ino);
	if (retval)
		return retval;

	memset(&inode, 0, sizeof(struct ext2_inode));
	inode.i_mode = LINUX_S_IFREG | (0666 & ~fs->umask);
	inode.i_links_count = 1;
	inode.i_uid = uid & 0xFFFF;
	ext2fs_set_i_uid_high(inode, (uid >> 16) & 0xffff);
	inode.i_gid = gid & 0xFFFF;
	ext2fs_set_i_gid_high(inode, (gid >> 16) & 0xffff);

	retval = ext2fs_write_new_inode(fs, *ino, &inode);
	if (retval)
		return retval;

	ext2fs_inode_alloc_stats2(fs, *ino, +1, 0);

	retval = ext2fs_extent_open2(fs, *ino, &inode, &handle);
	if (retval)
		return retval;

	lblk = 0;
	left = num ? num : 1;
	while (left) {
		blk64_t pblk, end;
		blk64_t n = left;

		retval =  ext2fs_find_first_zero_block_bitmap2(fs->block_map,
			goal, ext2fs_blocks_count(fs->super) - 1, &end);
		if (retval)
			goto errout;
		goal = end;

		retval =  ext2fs_find_first_set_block_bitmap2(fs->block_map, goal,
			       ext2fs_blocks_count(fs->super) - 1, &bend);
		if (retval == ENOENT) {
			bend = ext2fs_blocks_count(fs->super);
			if (num == 0)
				left = 0;
		}
		if (!num || bend - goal < left)
			n = bend - goal;
		pblk = goal;
		if (num)
			left -= n;
		goal += n;
		count += n;
		ext2fs_block_alloc_stats_range(fs, pblk, n, +1);

		if (zero_hugefile) {
			blk64_t ret_blk;
			retval = ext2fs_zero_blocks2(fs, pblk, n,
						     &ret_blk, NULL);

			if (retval)
				com_err(program_name, retval,
					_("while zeroing block %llu "
					  "for hugefile"), ret_blk);
		}

		while (n) {
			blk64_t l = n;
			struct ext2fs_extent newextent;

			if (l > EXT_INIT_MAX_LEN)
				l = EXT_INIT_MAX_LEN;

			newextent.e_len = l;
			newextent.e_pblk = pblk;
			newextent.e_lblk = lblk;
			newextent.e_flags = 0;

			retval = ext2fs_extent_insert(handle,
					EXT2_EXTENT_INSERT_AFTER, &newextent);
			if (retval)
				return retval;
			pblk += l;
			lblk += l;
			n -= l;
		}
	}

	retval = ext2fs_read_inode(fs, *ino, &inode);
	if (retval)
		goto errout;

	retval = ext2fs_iblk_add_blocks(fs, &inode,
					count / EXT2FS_CLUSTER_RATIO(fs));
	if (retval)
		goto errout;
	size = (__u64) count * fs->blocksize;
	retval = ext2fs_inode_size_set(fs, &inode, size);
	if (retval)
		goto errout;

	retval = ext2fs_write_new_inode(fs, *ino, &inode);
	if (retval)
		goto errout;

	if (idx_digits)
		sprintf(fn_numbuf, "%0*lu", idx_digits, idx);
	else if (num_files > 1)
		sprintf(fn_numbuf, "%lu", idx);

retry:
	retval = ext2fs_link(fs, dir, fn_buf, *ino, EXT2_FT_REG_FILE);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		retval = ext2fs_expand_dir(fs, dir);
		if (retval)
			goto errout;
		goto retry;
	}

	if (retval)
		goto errout;

errout:
	if (handle)
		ext2fs_extent_free(handle);

	return retval;
}

static blk64_t calc_overhead(ext2_filsys fs, blk64_t num)
{
	blk64_t e_blocks, e_blocks2, e_blocks3, e_blocks4;
	int extents_per_block;
	int extents = (num + EXT_INIT_MAX_LEN - 1) / EXT_INIT_MAX_LEN;

	if (extents <= 4)
		return 0;

	/*
	 * This calculation is due to the fact that we are inefficient
	 * in how handle extent splits when appending to the end of
	 * the extent tree.  Sigh.  We should fix this so that we can
	 * actually store 340 extents per 4k block, instead of only 170.
	 */
	extents_per_block = ((fs->blocksize -
			      sizeof(struct ext3_extent_header)) /
			     sizeof(struct ext3_extent));
	extents_per_block = (extents_per_block/ 2) - 1;

	e_blocks = (extents + extents_per_block - 1) / extents_per_block;
	e_blocks2 = (e_blocks + extents_per_block - 1) / extents_per_block;
	e_blocks3 = (e_blocks2 + extents_per_block - 1) / extents_per_block;
	e_blocks4 = (e_blocks3 + extents_per_block - 1) / extents_per_block;
	return e_blocks + e_blocks2 + e_blocks3 + e_blocks4;
}

/*
 * Find the place where we should start allocating blocks for the huge
 * files.  Leave <slack> free blocks at the beginning of the file
 * system for things like metadata blocks.
 */
static blk64_t get_start_block(ext2_filsys fs, blk64_t slack)
{
	errcode_t retval;
	blk64_t blk = fs->super->s_first_data_block, next;
	blk64_t last_blk = ext2fs_blocks_count(fs->super) - 1;

	while (slack) {
		retval = ext2fs_find_first_zero_block_bitmap2(fs->block_map,
						blk, last_blk, &blk);
		if (retval)
			break;

		retval = ext2fs_find_first_set_block_bitmap2(fs->block_map,
						blk, last_blk, &next);
		if (retval)
			next = last_blk;

		if (next - blk > slack) {
			blk += slack;
			break;
		}

		slack -= (next - blk);
		blk = next;
	}
	return blk;
}

static blk64_t round_up_align(blk64_t b, unsigned long align,
			      blk64_t part_offset)
{
	unsigned long m;

	if (align == 0)
		return b;
	part_offset = part_offset % align;
	m = (b + part_offset) % align;
	if (m)
		b += align - m;
	return b;
}

errcode_t mk_hugefiles(ext2_filsys fs, const char *device_name)
{
	unsigned long	i;
	ext2_ino_t	dir;
	errcode_t	retval;
	blk64_t		fs_blocks, part_offset = 0;
	unsigned long	align;
	int		d, dsize;
	char		*t;

	if (!get_bool_from_profile(fs_types, "make_hugefiles", 0))
		return 0;

	uid = get_int_from_profile(fs_types, "hugefiles_uid", 0);
	gid = get_int_from_profile(fs_types, "hugefiles_gid", 0);
	fs->umask = get_int_from_profile(fs_types, "hugefiles_umask", 077);
	num_files = get_int_from_profile(fs_types, "num_hugefiles", 0);
	t = get_string_from_profile(fs_types, "hugefiles_slack", "1M");
	num_slack = parse_num_blocks2(t, fs->super->s_log_block_size);
	free(t);
	t = get_string_from_profile(fs_types, "hugefiles_size", "0");
	num_blocks = parse_num_blocks2(t, fs->super->s_log_block_size);
	free(t);
	t = get_string_from_profile(fs_types, "hugefiles_align", "0");
	align = parse_num_blocks2(t, fs->super->s_log_block_size);
	free(t);
	if (get_bool_from_profile(fs_types, "hugefiles_align_disk", 0)) {
		part_offset = get_partition_start(device_name) /
			(fs->blocksize / 512);
		if (part_offset % EXT2FS_CLUSTER_RATIO(fs)) {
			fprintf(stderr,
				_("Partition offset of %llu (%uk) blocks "
				  "not compatible with cluster size %u.\n"),
				part_offset, fs->blocksize,
				EXT2_CLUSTER_SIZE(fs->super));
			exit(1);
		}
	}
	num_blocks = round_up_align(num_blocks, align, 0);
	zero_hugefile = get_bool_from_profile(fs_types, "zero_hugefiles",
					      zero_hugefile);

	t = get_string_from_profile(fs_types, "hugefiles_dir", "/");
	retval = create_directory(fs, t, &dir);
	free(t);
	if (retval)
		return retval;

	fn_prefix = get_string_from_profile(fs_types, "hugefiles_name",
					    "hugefile");
	idx_digits = get_int_from_profile(fs_types, "hugefiles_digits", 5);
	d = int_log10(num_files) + 1;
	if (idx_digits > d)
		d = idx_digits;
	dsize = strlen(fn_prefix) + d + 16;
	fn_buf = malloc(dsize);
	if (!fn_buf) {
		free(fn_prefix);
		return ENOMEM;
	}
	strcpy(fn_buf, fn_prefix);
	fn_numbuf = fn_buf + strlen(fn_prefix);
	free(fn_prefix);

	fs_blocks = ext2fs_free_blocks_count(fs->super);
	if (fs_blocks < num_slack + align)
		return ENOMEM;
	fs_blocks -= num_slack + align;
	if (num_blocks && num_blocks > fs_blocks)
		return ENOMEM;
	if (num_blocks == 0 && num_files == 0)
		num_files = 1;

	if (num_files == 0 && num_blocks) {
		num_files = fs_blocks / num_blocks;
		fs_blocks -= (num_files / 16) + 1;
		fs_blocks -= calc_overhead(fs, num_blocks) * num_files;
		num_files = fs_blocks / num_blocks;
	}

	if (num_blocks == 0 && num_files > 1) {
		num_blocks = fs_blocks / num_files;
		fs_blocks -= (num_files / 16) + 1;
		fs_blocks -= calc_overhead(fs, num_blocks) * num_files;
		num_blocks = fs_blocks / num_files;
	}

	num_slack += calc_overhead(fs, num_blocks) * num_files;
	num_slack += (num_files / 16) + 1; /* space for dir entries */
	goal = get_start_block(fs, num_slack);
	goal = round_up_align(goal, align, part_offset);

	if ((num_blocks ? num_blocks : fs_blocks) >
	    (0x80000000UL / fs->blocksize))
		fs->super->s_feature_ro_compat |=
			EXT2_FEATURE_RO_COMPAT_LARGE_FILE;

	if (!quiet) {
		if (zero_hugefile && verbose)
			printf("%s", _("Huge files will be zero'ed\n"));
		printf(_("Creating %lu huge file(s) "), num_files);
		if (num_blocks)
			printf(_("with %llu blocks each"), num_blocks);
		fputs(": ", stdout);
	}
	for (i=0; i < num_files; i++) {
		ext2_ino_t ino;

		retval = mk_hugefile(fs, num_blocks, dir, i, &ino);
		if (retval) {
			com_err(program_name, retval,
				_("while creating huge file %lu"), i);
			goto errout;
		}
	}
	if (!quiet)
		fputs(_("done\n"), stdout);

errout:
	free(fn_buf);
	return retval;
}
