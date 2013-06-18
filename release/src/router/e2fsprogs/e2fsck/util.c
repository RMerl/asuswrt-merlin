/*
 * util.c --- miscellaneous utilities
 *
 * Copyright (C) 1993, 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#ifdef __linux__
#include <sys/utsname.h>
#endif

#ifdef HAVE_CONIO_H
#undef HAVE_TERMIOS_H
#include <conio.h>
#define read_a_char()	getch()
#else
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "e2fsck.h"

extern e2fsck_t e2fsck_global_ctx;   /* Try your very best not to use this! */

#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

void fatal_error(e2fsck_t ctx, const char *msg)
{
	ext2_filsys fs = ctx->fs;
	int exit_value = FSCK_ERROR;

	if (msg)
		fprintf (stderr, "e2fsck: %s\n", msg);
	if (!fs)
		goto out;
	if (fs->io) {
		ext2fs_mmp_stop(ctx->fs);
		if (ctx->fs->io->magic == EXT2_ET_MAGIC_IO_CHANNEL)
			io_channel_flush(ctx->fs->io);
		else
			log_err(ctx, "e2fsck: io manager magic bad!\n");
	}
	if (ext2fs_test_changed(fs)) {
		exit_value |= FSCK_NONDESTRUCT;
		log_out(ctx, _("\n%s: ***** FILE SYSTEM WAS MODIFIED *****\n"),
			ctx->device_name);
		if (ctx->mount_flags & EXT2_MF_ISROOT)
			exit_value |= FSCK_REBOOT;
	}
	if (!ext2fs_test_valid(fs)) {
		log_out(ctx, _("\n%s: ********** WARNING: Filesystem still has "
			       "errors **********\n\n"), ctx->device_name);
		exit_value |= FSCK_UNCORRECTED;
		exit_value &= ~FSCK_NONDESTRUCT;
	}
out:
	ctx->flags |= E2F_FLAG_ABORT;
	if (ctx->flags & E2F_FLAG_SETJMP_OK)
		longjmp(ctx->abort_loc, 1);
	exit(exit_value);
}

void log_out(e2fsck_t ctx, const char *fmt, ...)
{
	va_list pvar;

	va_start(pvar, fmt);
	vprintf(fmt, pvar);
	va_end(pvar);
	if (ctx->logf) {
		va_start(pvar, fmt);
		vfprintf(ctx->logf, fmt, pvar);
		va_end(pvar);
	}
}

void log_err(e2fsck_t ctx, const char *fmt, ...)
{
	va_list pvar;

	va_start(pvar, fmt);
	vfprintf(stderr, fmt, pvar);
	va_end(pvar);
	if (ctx->logf) {
		va_start(pvar, fmt);
		vfprintf(ctx->logf, fmt, pvar);
		va_end(pvar);
	}
}

void *e2fsck_allocate_memory(e2fsck_t ctx, unsigned int size,
			     const char *description)
{
	void *ret;
	char buf[256];

#ifdef DEBUG_ALLOCATE_MEMORY
	printf("Allocating %u bytes for %s...\n", size, description);
#endif
	ret = malloc(size);
	if (!ret) {
		sprintf(buf, "Can't allocate %u bytes for %s\n",
			size, description);
		fatal_error(ctx, buf);
	}
	memset(ret, 0, size);
	return ret;
}

char *string_copy(e2fsck_t ctx EXT2FS_ATTR((unused)),
		  const char *str, int len)
{
	char	*ret;

	if (!str)
		return NULL;
	if (!len)
		len = strlen(str);
	ret = malloc(len+1);
	if (ret) {
		strncpy(ret, str, len);
		ret[len] = 0;
	}
	return ret;
}

#ifndef HAVE_STRNLEN
/*
 * Incredibly, libc5 doesn't appear to have strnlen.  So we have to
 * provide our own.
 */
int e2fsck_strnlen(const char * s, int count)
{
	const char *cp = s;

	while (count-- && *cp)
		cp++;
	return cp - s;
}
#endif

#ifndef HAVE_CONIO_H
static int read_a_char(void)
{
	char	c;
	int	r;
	int	fail = 0;

	while(1) {
		if (e2fsck_global_ctx &&
		    (e2fsck_global_ctx->flags & E2F_FLAG_CANCEL)) {
			return 3;
		}
		r = read(0, &c, 1);
		if (r == 1)
			return c;
		if (fail++ > 100)
			break;
	}
	return EOF;
}
#endif

int ask_yn(e2fsck_t ctx, const char * string, int def)
{
	int		c;
	const char	*defstr;
	const char	*short_yes = _("yY");
	const char	*short_no = _("nN");

#ifdef HAVE_TERMIOS_H
	struct termios	termios, tmp;

	tcgetattr (0, &termios);
	tmp = termios;
	tmp.c_lflag &= ~(ICANON | ECHO);
	tmp.c_cc[VMIN] = 1;
	tmp.c_cc[VTIME] = 0;
	tcsetattr (0, TCSANOW, &tmp);
#endif

	if (def == 1)
		defstr = _(_("<y>"));
	else if (def == 0)
		defstr = _(_("<n>"));
	else
		defstr = _(" (y/n)");
	log_out(ctx, "%s%s? ", string, defstr);
	while (1) {
		fflush (stdout);
		if ((c = read_a_char()) == EOF)
			break;
		if (c == 3) {
#ifdef HAVE_TERMIOS_H
			tcsetattr (0, TCSANOW, &termios);
#endif
			if (ctx->flags & E2F_FLAG_SETJMP_OK) {
				log_out(ctx, "\n");
				longjmp(e2fsck_global_ctx->abort_loc, 1);
			}
			log_out(ctx, _("cancelled!\n"));
			return 0;
		}
		if (strchr(short_yes, (char) c)) {
			def = 1;
			break;
		}
		else if (strchr(short_no, (char) c)) {
			def = 0;
			break;
		}
		else if ((c == 27 || c == ' ' || c == '\n') && (def != -1))
			break;
	}
	if (def)
		log_out(ctx, _("yes\n"));
	else
		log_out(ctx, _("no\n"));
#ifdef HAVE_TERMIOS_H
	tcsetattr (0, TCSANOW, &termios);
#endif
	return def;
}

int ask (e2fsck_t ctx, const char * string, int def)
{
	if (ctx->options & E2F_OPT_NO) {
		log_out(ctx, _("%s? no\n\n"), string);
		return 0;
	}
	if (ctx->options & E2F_OPT_YES) {
		log_out(ctx, _("%s? yes\n\n"), string);
		return 1;
	}
	if (ctx->options & E2F_OPT_PREEN) {
		log_out(ctx, "%s? %s\n\n", string, def ? _("yes") : _("no"));
		return def;
	}
	return ask_yn(ctx, string, def);
}

void e2fsck_read_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	errcode_t	retval;
	const char	*old_op;
	unsigned int	save_type;

	if (ctx->invalid_bitmaps) {
		com_err(ctx->program_name, 0,
		    _("e2fsck_read_bitmaps: illegal bitmap block(s) for %s"),
			ctx->device_name);
		fatal_error(ctx, 0);
	}

	old_op = ehandler_operation(_("reading inode and block bitmaps"));
	e2fsck_set_bitmap_type(fs, EXT2FS_BMAP64_RBTREE, "fs_bitmaps",
			       &save_type);
	retval = ext2fs_read_bitmaps(fs);
	fs->default_bitmap_type = save_type;
	ehandler_operation(old_op);
	if (retval) {
		com_err(ctx->program_name, retval,
			_("while retrying to read bitmaps for %s"),
			ctx->device_name);
		fatal_error(ctx, 0);
	}
}

void e2fsck_write_bitmaps(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;
	errcode_t	retval;
	const char	*old_op;

	old_op = ehandler_operation(_("writing block and inode bitmaps"));
	retval = ext2fs_write_bitmaps(fs);
	ehandler_operation(old_op);
	if (retval) {
		com_err(ctx->program_name, retval,
			_("while rewriting block and inode bitmaps for %s"),
			ctx->device_name);
		fatal_error(ctx, 0);
	}
}

void preenhalt(e2fsck_t ctx)
{
	ext2_filsys fs = ctx->fs;

	if (!(ctx->options & E2F_OPT_PREEN))
		return;
	log_err(ctx, _("\n\n%s: UNEXPECTED INCONSISTENCY; "
		"RUN fsck MANUALLY.\n\t(i.e., without -a or -p options)\n"),
	       ctx->device_name);
	ctx->flags |= E2F_FLAG_EXITING;
	if (fs != NULL) {
		fs->super->s_state |= EXT2_ERROR_FS;
		ext2fs_mark_super_dirty(fs);
		ext2fs_close(fs);
	}
	exit(FSCK_UNCORRECTED);
}

#ifdef RESOURCE_TRACK
void init_resource_track(struct resource_track *track, io_channel channel)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
	io_stats io_start = 0;

	track->brk_start = sbrk(0);
	gettimeofday(&track->time_start, 0);
#ifdef HAVE_GETRUSAGE
#ifdef sun
	memset(&r, 0, sizeof(struct rusage));
#endif
	getrusage(RUSAGE_SELF, &r);
	track->user_start = r.ru_utime;
	track->system_start = r.ru_stime;
#else
	track->user_start.tv_sec = track->user_start.tv_usec = 0;
	track->system_start.tv_sec = track->system_start.tv_usec = 0;
#endif
	track->bytes_read = 0;
	track->bytes_written = 0;
	if (channel && channel->manager && channel->manager->get_stats)
		channel->manager->get_stats(channel, &io_start);
	if (io_start) {
		track->bytes_read = io_start->bytes_read;
		track->bytes_written = io_start->bytes_written;
	}
}

#ifdef __GNUC__
#define _INLINE_ __inline__
#else
#define _INLINE_
#endif

static _INLINE_ float timeval_subtract(struct timeval *tv1,
				       struct timeval *tv2)
{
	return ((tv1->tv_sec - tv2->tv_sec) +
		((float) (tv1->tv_usec - tv2->tv_usec)) / 1000000);
}

void print_resource_track(e2fsck_t ctx, const char *desc,
			  struct resource_track *track, io_channel channel)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
#ifdef HAVE_MALLINFO
	struct mallinfo	malloc_info;
#endif
	struct timeval time_end;

	if ((desc && !(ctx->options & E2F_OPT_TIME2)) ||
	    (!desc && !(ctx->options & E2F_OPT_TIME)))
		return;

	e2fsck_clear_progbar(ctx);
	gettimeofday(&time_end, 0);

	if (desc)
		log_out(ctx, "%s: ", desc);

#ifdef HAVE_MALLINFO
#define kbytes(x)	(((unsigned long)(x) + 1023) / 1024)

	malloc_info = mallinfo();
	log_out(ctx, _("Memory used: %luk/%luk (%luk/%luk), "),
		kbytes(malloc_info.arena), kbytes(malloc_info.hblkhd),
		kbytes(malloc_info.uordblks), kbytes(malloc_info.fordblks));
#else
	log_out(ctx, _("Memory used: %lu, "),
		(unsigned long) (((char *) sbrk(0)) -
				 ((char *) track->brk_start)));
#endif
#ifdef HAVE_GETRUSAGE
	getrusage(RUSAGE_SELF, &r);

	log_out(ctx, _("time: %5.2f/%5.2f/%5.2f\n"),
		timeval_subtract(&time_end, &track->time_start),
		timeval_subtract(&r.ru_utime, &track->user_start),
		timeval_subtract(&r.ru_stime, &track->system_start));
#else
	log_out(ctx, _("elapsed time: %6.3f\n"),
		timeval_subtract(&time_end, &track->time_start));
#endif
#define mbytes(x)	(((x) + 1048575) / 1048576)
	if (channel && channel->manager && channel->manager->get_stats) {
		io_stats delta = 0;
		unsigned long long bytes_read = 0;
		unsigned long long bytes_written = 0;

		if (desc)
			log_out(ctx, "%s: ", desc);

		channel->manager->get_stats(channel, &delta);
		if (delta) {
			bytes_read = delta->bytes_read - track->bytes_read;
			bytes_written = delta->bytes_written -
				track->bytes_written;
		}
		log_out(ctx, "I/O read: %lluMB, write: %lluMB, "
			"rate: %.2fMB/s\n",
			mbytes(bytes_read), mbytes(bytes_written),
			(double)mbytes(bytes_read + bytes_written) /
			timeval_subtract(&time_end, &track->time_start));
	}
}
#endif /* RESOURCE_TRACK */

void e2fsck_read_inode(e2fsck_t ctx, unsigned long ino,
			      struct ext2_inode * inode, const char *proc)
{
	int retval;

	retval = ext2fs_read_inode(ctx->fs, ino, inode);
	if (retval) {
		com_err("ext2fs_read_inode", retval,
			_("while reading inode %lu in %s"), ino, proc);
		fatal_error(ctx, 0);
	}
}

void e2fsck_read_inode_full(e2fsck_t ctx, unsigned long ino,
			    struct ext2_inode *inode, int bufsize,
			    const char *proc)
{
	int retval;

	retval = ext2fs_read_inode_full(ctx->fs, ino, inode, bufsize);
	if (retval) {
		com_err("ext2fs_read_inode_full", retval,
			_("while reading inode %lu in %s"), ino, proc);
		fatal_error(ctx, 0);
	}
}

extern void e2fsck_write_inode_full(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, int bufsize,
			       const char *proc)
{
	int retval;

	retval = ext2fs_write_inode_full(ctx->fs, ino, inode, bufsize);
	if (retval) {
		com_err("ext2fs_write_inode", retval,
			_("while writing inode %lu in %s"), ino, proc);
		fatal_error(ctx, 0);
	}
}

extern void e2fsck_write_inode(e2fsck_t ctx, unsigned long ino,
			       struct ext2_inode * inode, const char *proc)
{
	int retval;

	retval = ext2fs_write_inode(ctx->fs, ino, inode);
	if (retval) {
		com_err("ext2fs_write_inode", retval,
			_("while writing inode %lu in %s"), ino, proc);
		fatal_error(ctx, 0);
	}
}

#ifdef MTRACE
void mtrace_print(char *mesg)
{
	FILE	*malloc_get_mallstream();
	FILE	*f = malloc_get_mallstream();

	if (f)
		fprintf(f, "============= %s\n", mesg);
}
#endif

blk_t get_backup_sb(e2fsck_t ctx, ext2_filsys fs, const char *name,
		   io_manager manager)
{
	struct ext2_super_block *sb;
	io_channel		io = NULL;
	void			*buf = NULL;
	int			blocksize;
	blk_t			superblock, ret_sb = 8193;

	if (fs && fs->super) {
		ret_sb = (fs->super->s_blocks_per_group +
			  fs->super->s_first_data_block);
		if (ctx) {
			ctx->superblock = ret_sb;
			ctx->blocksize = fs->blocksize;
		}
		return ret_sb;
	}

	if (ctx) {
		if (ctx->blocksize) {
			ret_sb = ctx->blocksize * 8;
			if (ctx->blocksize == 1024)
				ret_sb++;
			ctx->superblock = ret_sb;
			return ret_sb;
		}
		ctx->superblock = ret_sb;
		ctx->blocksize = 1024;
	}

	if (!name || !manager)
		goto cleanup;

	if (manager->open(name, 0, &io) != 0)
		goto cleanup;

	if (ext2fs_get_mem(SUPERBLOCK_SIZE, &buf))
		goto cleanup;
	sb = (struct ext2_super_block *) buf;

	for (blocksize = EXT2_MIN_BLOCK_SIZE;
	     blocksize <= EXT2_MAX_BLOCK_SIZE ; blocksize *= 2) {
		superblock = blocksize*8;
		if (blocksize == 1024)
			superblock++;
		io_channel_set_blksize(io, blocksize);
		if (io_channel_read_blk64(io, superblock,
					-SUPERBLOCK_SIZE, buf))
			continue;
#ifdef WORDS_BIGENDIAN
		if (sb->s_magic == ext2fs_swab16(EXT2_SUPER_MAGIC))
			ext2fs_swap_super(sb);
#endif
		if ((sb->s_magic == EXT2_SUPER_MAGIC) &&
		    (EXT2_BLOCK_SIZE(sb) == blocksize)) {
			ret_sb = superblock;
			if (ctx) {
				ctx->superblock = superblock;
				ctx->blocksize = blocksize;
			}
			break;
		}
	}

cleanup:
	if (io)
		io_channel_close(io);
	if (buf)
		ext2fs_free_mem(&buf);
	return (ret_sb);
}

/*
 * Given a mode, return the ext2 file type
 */
int ext2_file_type(unsigned int mode)
{
	if (LINUX_S_ISREG(mode))
		return EXT2_FT_REG_FILE;

	if (LINUX_S_ISDIR(mode))
		return EXT2_FT_DIR;

	if (LINUX_S_ISCHR(mode))
		return EXT2_FT_CHRDEV;

	if (LINUX_S_ISBLK(mode))
		return EXT2_FT_BLKDEV;

	if (LINUX_S_ISLNK(mode))
		return EXT2_FT_SYMLINK;

	if (LINUX_S_ISFIFO(mode))
		return EXT2_FT_FIFO;

	if (LINUX_S_ISSOCK(mode))
		return EXT2_FT_SOCK;

	return 0;
}

#define STRIDE_LENGTH 8
/*
 * Helper function which zeros out _num_ blocks starting at _blk_.  In
 * case of an error, the details of the error is returned via _ret_blk_
 * and _ret_count_ if they are non-NULL pointers.  Returns 0 on
 * success, and an error code on an error.
 *
 * As a special case, if the first argument is NULL, then it will
 * attempt to free the static zeroizing buffer.  (This is to keep
 * programs that check for memory leaks happy.)
 */
errcode_t e2fsck_zero_blocks(ext2_filsys fs, blk_t blk, int num,
			     blk_t *ret_blk, int *ret_count)
{
	int		j, count;
	static char	*buf;
	errcode_t	retval;

	/* If fs is null, clean up the static buffer and return */
	if (!fs) {
		if (buf) {
			free(buf);
			buf = 0;
		}
		return 0;
	}
	/* Allocate the zeroizing buffer if necessary */
	if (!buf) {
		buf = malloc(fs->blocksize * STRIDE_LENGTH);
		if (!buf) {
			com_err("malloc", ENOMEM,
				_("while allocating zeroizing buffer"));
			exit(1);
		}
		memset(buf, 0, fs->blocksize * STRIDE_LENGTH);
	}
	/* OK, do the write loop */
	for (j = 0; j < num; j += STRIDE_LENGTH, blk += STRIDE_LENGTH) {
		count = num - j;
		if (count > STRIDE_LENGTH)
			count = STRIDE_LENGTH;
		retval = io_channel_write_blk64(fs->io, blk, count, buf);
		if (retval) {
			if (ret_count)
				*ret_count = count;
			if (ret_blk)
				*ret_blk = blk;
			return retval;
		}
	}
	return 0;
}

/*
 * Check to see if a filesystem is in /proc/filesystems.
 * Returns 1 if found, 0 if not
 */
int fs_proc_check(const char *fs_name)
{
	FILE	*f;
	char	buf[80], *cp, *t;

	f = fopen("/proc/filesystems", "r");
	if (!f)
		return (0);
	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		cp = buf;
		if (!isspace(*cp)) {
			while (*cp && !isspace(*cp))
				cp++;
		}
		while (*cp && isspace(*cp))
			cp++;
		if ((t = strchr(cp, '\n')) != NULL)
			*t = 0;
		if ((t = strchr(cp, '\t')) != NULL)
			*t = 0;
		if ((t = strchr(cp, ' ')) != NULL)
			*t = 0;
		if (!strcmp(fs_name, cp)) {
			fclose(f);
			return (1);
		}
	}
	fclose(f);
	return (0);
}

/*
 * Check to see if a filesystem is available as a module
 * Returns 1 if found, 0 if not
 */
int check_for_modules(const char *fs_name)
{
#ifdef __linux__
	struct utsname	uts;
	FILE		*f;
	char		buf[1024], *cp, *t;
	int		i;

	if (uname(&uts))
		return (0);
	snprintf(buf, sizeof(buf), "/lib/modules/%s/modules.dep", uts.release);

	f = fopen(buf, "r");
	if (!f)
		return (0);
	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		if ((cp = strchr(buf, ':')) != NULL)
			*cp = 0;
		else
			continue;
		if ((cp = strrchr(buf, '/')) != NULL)
			cp++;
		else
			cp = buf;
		i = strlen(cp);
		if (i > 3) {
			t = cp + i - 3;
			if (!strcmp(t, ".ko"))
				*t = 0;
		}
		if (!strcmp(cp, fs_name)) {
			fclose(f);
			return (1);
		}
	}
	fclose(f);
#endif /* __linux__ */
	return (0);
}

/*
 * Helper function that does the right thing if write returns a
 * partial write, or an EGAIN/EINTR error.
 */
int write_all(int fd, char *buf, size_t count)
{
	ssize_t ret;
	int c = 0;

	while (count > 0) {
		ret = write(fd, buf, count);
		if (ret < 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			return -1;
		}
		count -= ret;
		buf += ret;
		c += ret;
	}
	return c;
}

void dump_mmp_msg(struct mmp_struct *mmp, const char *msg)
{

	if (msg)
		printf("MMP check failed: %s\n", msg);
	if (mmp) {
		time_t t = mmp->mmp_time;

		printf("MMP error info: last update: %s node: %s device: %s\n",
		       ctime(&t), mmp->mmp_nodename, mmp->mmp_bdevname);
	}
}

errcode_t e2fsck_mmp_update(ext2_filsys fs)
{
	errcode_t retval;

	retval = ext2fs_mmp_update(fs);
	if (retval == EXT2_ET_MMP_CHANGE_ABORT)
		dump_mmp_msg(fs->mmp_cmp,
			     _("UNEXPECTED INCONSISTENCY: the filesystem is "
			       "being modified while fsck is running.\n"));

	return retval;
}

void e2fsck_set_bitmap_type(ext2_filsys fs, unsigned int default_type,
			    const char *profile_name, unsigned int *old_type)
{
	unsigned type;

	if (old_type)
		*old_type = fs->default_bitmap_type;
	profile_get_uint(e2fsck_global_ctx->profile, "bitmaps",
			 profile_name, 0, default_type, &type);
	profile_get_uint(e2fsck_global_ctx->profile, "bitmaps",
			 "all", 0, type, &type);
	fs->default_bitmap_type = type ? type : default_type;
}

errcode_t e2fsck_allocate_inode_bitmap(ext2_filsys fs, const char *descr,
				       int deftype,
				       const char *name,
				       ext2fs_inode_bitmap *ret)
{
	errcode_t	retval;
	unsigned int	save_type;

	e2fsck_set_bitmap_type(fs, deftype, name, &save_type);
	retval = ext2fs_allocate_inode_bitmap(fs, descr, ret);
	fs->default_bitmap_type = save_type;
	return retval;
}

errcode_t e2fsck_allocate_block_bitmap(ext2_filsys fs, const char *descr,
				       int deftype,
				       const char *name,
				       ext2fs_block_bitmap *ret)
{
	errcode_t	retval;
	unsigned int	save_type;

	e2fsck_set_bitmap_type(fs, deftype, name, &save_type);
	retval = ext2fs_allocate_block_bitmap(fs, descr, ret);
	fs->default_bitmap_type = save_type;
	return retval;
}

errcode_t e2fsck_allocate_subcluster_bitmap(ext2_filsys fs, const char *descr,
					    int deftype,
					    const char *name,
					    ext2fs_block_bitmap *ret)
{
	errcode_t	retval;
	unsigned int	save_type;

	e2fsck_set_bitmap_type(fs, deftype, name, &save_type);
	retval = ext2fs_allocate_subcluster_bitmap(fs, descr, ret);
	fs->default_bitmap_type = save_type;
	return retval;
}
