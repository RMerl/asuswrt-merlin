/*
 * cramfsck - check a cramfs file system
 *
 * Copyright (C) 2000-2002 Transmeta Corporation
 *               2005 Adrian Bunk
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 1999/12/03: Linus Torvalds (cramfs tester and unarchive program)
 * 2000/06/03: Daniel Quinlan (CRC and length checking program)
 * 2000/06/04: Daniel Quinlan (merged programs, added options, support
 *                            for special files, preserve permissions and
 *                            ownership, cramfs superblock v2, bogus mode
 *                            test, pathname length test, etc.)
 * 2000/06/06: Daniel Quinlan (support for holes, pretty-printing,
 *                            symlink size test)
 * 2000/07/11: Daniel Quinlan (file length tests, start at offset 0 or 512,
 *                            fsck-compatible exit codes)
 * 2000/07/15: Daniel Quinlan (initial support for block devices)
 * 2002/01/10: Daniel Quinlan (additional checks, test more return codes,
 *                            use read if mmap fails, standardize messages)
 */

/* compile-time options */
//#define INCLUDE_FS_TESTS	/* include cramfs checking and extraction */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <utime.h>
#include <fcntl.h>
#include <zlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>	/* for major, minor */

#include "cramfs.h"
#include "nls.h"
#include "blkdev.h"
#include "c.h"
#include "exitcodes.h"

#define XALLOC_EXIT_CODE FSCK_ERROR
#include "xalloc.h"

static const char *progname = "cramfsck";

static int fd;			/* ROM image file descriptor */
static char *filename;		/* ROM image filename */
struct cramfs_super super;	/* just find the cramfs superblock once */
static int cramfs_is_big_endian = 0;	/* source is big endian */
static int opt_verbose = 0;	/* 1 = verbose (-v), 2+ = very verbose (-vv) */

char *extract_dir = "";		/* extraction directory (-x) */

#define PAD_SIZE 512

#ifdef INCLUDE_FS_TESTS

static int opt_extract = 0;	/* extract cramfs (-x) */

static uid_t euid;		/* effective UID */

/* (cramfs_super + start) <= start_dir < end_dir <= start_data <= end_data */
static unsigned long start_dir = ~0UL;	/* start of first non-root inode */
static unsigned long end_dir = 0;	/* end of the directory structure */
static unsigned long start_data = ~0UL;	/* start of the data (256 MB = max) */
static unsigned long end_data = 0;	/* end of the data */


/* Guarantee access to at least 8kB at a time */
#define ROMBUFFER_BITS	13
#define ROMBUFFERSIZE	(1 << ROMBUFFER_BITS)
#define ROMBUFFERMASK	(ROMBUFFERSIZE - 1)
static char read_buffer[ROMBUFFERSIZE * 2];
static unsigned long read_buffer_block = ~0UL;

static z_stream stream;

/* Prototypes */
static void expand_fs(char *, struct cramfs_inode *);
#endif /* INCLUDE_FS_TESTS */

static char *outbuffer;

static size_t page_size;

/* Input status of 0 to print help and exit without an error. */
static void __attribute__((__noreturn__)) usage(int status)
{
	FILE *stream = status ? stderr : stdout;

	fprintf(stream, _("usage: %s [-hv] [-x dir] file\n"
		" -h         print this help\n"
		" -x dir     extract into dir\n"
		" -v         be more verbose\n"
		" file       file to test\n"), progname);

	exit(status);
}

int get_superblock_endianness(uint32_t magic)
{
	if (magic == CRAMFS_MAGIC) {
		cramfs_is_big_endian = HOST_IS_BIG_ENDIAN;
		return 0;
	} else if (magic ==
		   u32_toggle_endianness(!HOST_IS_BIG_ENDIAN, CRAMFS_MAGIC)) {
		cramfs_is_big_endian = !HOST_IS_BIG_ENDIAN;
		return 0;
	} else
		return -1;
}

static void test_super(int *start, size_t * length)
{
	struct stat st;

	/* find the physical size of the file or block device */
	if (stat(filename, &st) < 0)
		err(FSCK_ERROR, _("stat failed: %s"), filename);

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		err(FSCK_ERROR, _("open failed: %s"), filename);

	if (S_ISBLK(st.st_mode)) {
		unsigned long long bytes;
		if (blkdev_get_size(fd, &bytes))
			err(FSCK_ERROR,
			    _("ioctl failed: unable to determine device size: %s"),
			    filename);
		*length = bytes;
	} else if (S_ISREG(st.st_mode))
		*length = st.st_size;
	else
		errx(FSCK_ERROR, _("not a block device or file: %s"), filename);

	if (*length < sizeof(struct cramfs_super))
		errx(FSCK_UNCORRECTED, _("file length too short"));

	/* find superblock */
	if (read(fd, &super, sizeof(super)) != sizeof(super))
		err(FSCK_ERROR, _("read failed: %s"), filename);
	if (get_superblock_endianness(super.magic) != -1)
		*start = 0;
	else if (*length >= (PAD_SIZE + sizeof(super))) {
		lseek(fd, PAD_SIZE, SEEK_SET);
		if (read(fd, &super, sizeof(super)) != sizeof(super))
			err(FSCK_ERROR, _("read failed: %s"), filename);
		if (get_superblock_endianness(super.magic) != -1)
			*start = PAD_SIZE;
		else
			errx(FSCK_UNCORRECTED, _("superblock magic not found"));
	} else
		errx(FSCK_UNCORRECTED, _("superblock magic not found"));

	if (opt_verbose)
		printf(_("cramfs endianness is %s\n"),
		       cramfs_is_big_endian ? _("big") : _("little"));

	super_toggle_endianness(cramfs_is_big_endian, &super);
	if (super.flags & ~CRAMFS_SUPPORTED_FLAGS)
		errx(FSCK_ERROR, _("unsupported filesystem features"));

	if (super.size < page_size)
		errx(FSCK_UNCORRECTED, _("superblock size (%d) too small"),
		     super.size);

	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2) {
		if (super.fsid.files == 0)
			errx(FSCK_UNCORRECTED, _("zero file count"));
		if (*length < super.size)
			errx(FSCK_UNCORRECTED, _("file length too short"));
		else if (*length > super.size)
			fprintf(stderr,
				_("warning: file extends past end of filesystem\n"));
	} else
		fprintf(stderr, _("warning: old cramfs format\n"));
}

static void test_crc(int start)
{
	void *buf;
	uint32_t crc;

	if (!(super.flags & CRAMFS_FLAG_FSID_VERSION_2)) {
#ifdef INCLUDE_FS_TESTS
		return;
#else
		errx(FSCK_USAGE, _("unable to test CRC: old cramfs format"));
#endif
	}

	crc = crc32(0L, Z_NULL, 0);

	buf =
	    mmap(NULL, super.size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (buf == MAP_FAILED) {
		buf =
		    mmap(NULL, super.size, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (buf != MAP_FAILED) {
			lseek(fd, 0, SEEK_SET);
			if (read(fd, buf, super.size) < 0)
				err(FSCK_ERROR, _("read failed: %s"), filename);
		}
	}
	if (buf != MAP_FAILED) {
		((struct cramfs_super *)(buf + start))->fsid.crc =
		    crc32(0L, Z_NULL, 0);
		crc = crc32(crc, buf + start, super.size - start);
		munmap(buf, super.size);
	} else {
		int retval;
		size_t length = 0;

		buf = xmalloc(4096);
		lseek(fd, start, SEEK_SET);
		for (;;) {
			retval = read(fd, buf, 4096);
			if (retval < 0)
				err(FSCK_ERROR, _("read failed: %s"), filename);
			else if (retval == 0)
				break;
			if (length == 0)
				((struct cramfs_super *)buf)->fsid.crc =
				    crc32(0L, Z_NULL, 0);
			length += retval;
			if (length > (super.size - start)) {
				crc = crc32(crc, buf,
					  retval - (length -
						    (super.size - start)));
				break;
			}
			crc = crc32(crc, buf, retval);
		}
		free(buf);
	}

	if (crc != super.fsid.crc)
		errx(FSCK_UNCORRECTED, _("crc error"));
}

#ifdef INCLUDE_FS_TESTS
static void print_node(char type, struct cramfs_inode *i, char *name)
{
	char info[10];

	if (S_ISCHR(i->mode) || (S_ISBLK(i->mode)))
		/* major/minor numbers can be as high as 2^12 or 4096 */
		snprintf(info, 10, "%4d,%4d", major(i->size), minor(i->size));
	else
		/* size be as high as 2^24 or 16777216 */
		snprintf(info, 10, "%9d", i->size);

	printf("%c %04o %s %5d:%-3d %s\n",
	       type, i->mode & ~S_IFMT, info, i->uid, i->gid,
	       !*name && type == 'd' ? "/" : name);
}

/*
 * Create a fake "blocked" access
 */
static void *romfs_read(unsigned long offset)
{
	unsigned int block = offset >> ROMBUFFER_BITS;
	if (block != read_buffer_block) {
		read_buffer_block = block;
		lseek(fd, block << ROMBUFFER_BITS, SEEK_SET);
		read(fd, read_buffer, ROMBUFFERSIZE * 2);
	}
	return read_buffer + (offset & ROMBUFFERMASK);
}

static struct cramfs_inode *cramfs_iget(struct cramfs_inode *i)
{
	struct cramfs_inode *inode = xmalloc(sizeof(struct cramfs_inode));

	inode_to_host(cramfs_is_big_endian, i, inode);
	return inode;
}

static struct cramfs_inode *iget(unsigned int ino)
{
	return cramfs_iget(romfs_read(ino));
}

static void iput(struct cramfs_inode *inode)
{
	free(inode);
}

/*
 * Return the offset of the root directory
 */
static struct cramfs_inode *read_super(void)
{
	struct cramfs_inode *root = cramfs_iget(&super.root);
	unsigned long offset = root->offset << 2;

	if (!S_ISDIR(root->mode))
		errx(FSCK_UNCORRECTED, _("root inode is not directory"));
	if (!(super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
	    ((offset != sizeof(struct cramfs_super)) &&
	     (offset != PAD_SIZE + sizeof(struct cramfs_super)))) {
		errx(FSCK_UNCORRECTED, _("bad root offset (%lu)"), offset);
	}
	return root;
}

static int uncompress_block(void *src, int len)
{
	int err;

	stream.next_in = src;
	stream.avail_in = len;

	stream.next_out = (unsigned char *)outbuffer;
	stream.avail_out = page_size * 2;

	inflateReset(&stream);

	if (len > page_size * 2)
		errx(FSCK_UNCORRECTED, _("data block too large"));

	err = inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END)
		errx(FSCK_UNCORRECTED, _("decompression error %p(%d): %s"),
		     zError(err), src, len);
	return stream.total_out;
}

#if !HAVE_LCHOWN
#define lchown chown
#endif

static void do_uncompress(char *path, int fd, unsigned long offset,
			  unsigned long size)
{
	unsigned long curr = offset + 4 * ((size + page_size - 1) / page_size);

	do {
		unsigned long out = page_size;
		unsigned long next = u32_toggle_endianness(cramfs_is_big_endian,
							   *(uint32_t *)
							   romfs_read(offset));

		if (next > end_data)
			end_data = next;

		offset += 4;
		if (curr == next) {
			if (opt_verbose > 1)
				printf(_("  hole at %ld (%zd)\n"), curr,
				       page_size);
			if (size < page_size)
				out = size;
			memset(outbuffer, 0x00, out);
		} else {
			if (opt_verbose > 1)
				printf(_("  uncompressing block at %ld to %ld (%ld)\n"),
				       curr, next, next - curr);
			out = uncompress_block(romfs_read(curr), next - curr);
		}
		if (size >= page_size) {
			if (out != page_size)
				errx(FSCK_UNCORRECTED,
				     _("non-block (%ld) bytes"), out);
		} else {
			if (out != size)
				errx(FSCK_UNCORRECTED,
				     _("non-size (%ld vs %ld) bytes"), out,
				     size);
		}
		size -= out;
		if (opt_extract)
			if (write(fd, outbuffer, out) < 0)
				err(FSCK_ERROR, _("write failed: %s"),
				    path);
		curr = next;
	} while (size);
}

static void change_file_status(char *path, struct cramfs_inode *i)
{
	struct utimbuf epoch = { 0, 0 };

	if (euid == 0) {
		if (lchown(path, i->uid, i->gid) < 0)
			err(FSCK_ERROR, _("lchown failed: %s"), path);
		if (S_ISLNK(i->mode))
			return;
		if (((S_ISUID | S_ISGID) & i->mode) && chmod(path, i->mode) < 0)
			err(FSCK_ERROR, _("chown failed: %s"), path);
	}
	if (S_ISLNK(i->mode))
		return;
	if (utime(path, &epoch) < 0)
		err(FSCK_ERROR, _("utime failed: %s"), path);
}

static void do_directory(char *path, struct cramfs_inode *i)
{
	int pathlen = strlen(path);
	int count = i->size;
	unsigned long offset = i->offset << 2;
	char *newpath = xmalloc(pathlen + 256);

	if (offset == 0 && count != 0)
		errx(FSCK_UNCORRECTED,
		     _("directory inode has zero offset and non-zero size: %s"),
		     path);

	if (offset != 0 && offset < start_dir)
		start_dir = offset;

	/* TODO: Do we need to check end_dir for empty case? */
	memcpy(newpath, path, pathlen);
	newpath[pathlen] = '/';
	pathlen++;
	if (opt_verbose)
		print_node('d', i, path);

	if (opt_extract) {
		if (mkdir(path, i->mode) < 0)
			err(FSCK_ERROR, _("mkdir failed: %s"), path);
		change_file_status(path, i);
	}
	while (count > 0) {
		struct cramfs_inode *child = iget(offset);
		int size;
		int newlen = child->namelen << 2;

		size = sizeof(struct cramfs_inode) + newlen;
		count -= size;

		offset += sizeof(struct cramfs_inode);

		memcpy(newpath + pathlen, romfs_read(offset), newlen);
		newpath[pathlen + newlen] = 0;
		if (newlen == 0)
			errx(FSCK_UNCORRECTED, _("filename length is zero"));
		if ((pathlen + newlen) - strlen(newpath) > 3)
			errx(FSCK_UNCORRECTED, _("bad filename length"));
		expand_fs(newpath, child);

		offset += newlen;

		if (offset <= start_dir)
			errx(FSCK_UNCORRECTED, _("bad inode offset"));
		if (offset > end_dir)
			end_dir = offset;
		iput(child);	/* free(child) */
	}
	free(newpath);
}

static void do_file(char *path, struct cramfs_inode *i)
{
	unsigned long offset = i->offset << 2;
	int fd = 0;

	if (offset == 0 && i->size != 0)
		errx(FSCK_UNCORRECTED,
		     _("file inode has zero offset and non-zero size"));
	if (i->size == 0 && offset != 0)
		errx(FSCK_UNCORRECTED,
		     _("file inode has zero size and non-zero offset"));
	if (offset != 0 && offset < start_data)
		start_data = offset;
	if (opt_verbose)
		print_node('f', i, path);
	if (opt_extract) {
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, i->mode);
		if (fd < 0)
			err(FSCK_ERROR, _("open failed: %s"), path);
	}
	if (i->size)
		do_uncompress(path, fd, offset, i->size);
	if (opt_extract) {
		close(fd);
		change_file_status(path, i);
	}
}

static void do_symlink(char *path, struct cramfs_inode *i)
{
	unsigned long offset = i->offset << 2;
	unsigned long curr = offset + 4;
	unsigned long next =
	    u32_toggle_endianness(cramfs_is_big_endian,
				  *(uint32_t *) romfs_read(offset));
	unsigned long size;

	if (offset == 0)
		errx(FSCK_UNCORRECTED, _("symbolic link has zero offset"));
	if (i->size == 0)
		errx(FSCK_UNCORRECTED, _("symbolic link has zero size"));

	if (offset < start_data)
		start_data = offset;
	if (next > end_data)
		end_data = next;

	size = uncompress_block(romfs_read(curr), next - curr);
	if (size != i->size)
		errx(FSCK_UNCORRECTED, _("size error in symlink: %s"), path);
	outbuffer[size] = 0;
	if (opt_verbose) {
		char *str;

		asprintf(&str, "%s -> %s", path, outbuffer);
		print_node('l', i, str);
		if (opt_verbose > 1)
			printf(_("  uncompressing block at %ld to %ld (%ld)\n"),
			       curr, next, next - curr);
		free(str);
	}
	if (opt_extract) {
		if (symlink(outbuffer, path) < 0)
			err(FSCK_ERROR, _("symlink failed: %s"), path);
		change_file_status(path, i);
	}
}

static void do_special_inode(char *path, struct cramfs_inode *i)
{
	dev_t devtype = 0;
	char type;

	if (i->offset)
		/* no need to shift offset */
		errx(FSCK_UNCORRECTED,
		     _("special file has non-zero offset: %s"), path);

	if (S_ISCHR(i->mode)) {
		devtype = i->size;
		type = 'c';
	} else if (S_ISBLK(i->mode)) {
		devtype = i->size;
		type = 'b';
	} else if (S_ISFIFO(i->mode)) {
		if (i->size != 0)
			errx(FSCK_UNCORRECTED, _("fifo has non-zero size: %s"),
			     path);
		type = 'p';
	} else if (S_ISSOCK(i->mode)) {
		if (i->size != 0)
			errx(FSCK_UNCORRECTED,
			     _("socket has non-zero size: %s"), path);
		type = 's';
	} else {
		errx(FSCK_UNCORRECTED, _("bogus mode: %s (%o)"), path, i->mode);
		return;		/* not reached */
	}

	if (opt_verbose)
		print_node(type, i, path);

	if (opt_extract) {
		if (mknod(path, i->mode, devtype) < 0)
			err(FSCK_ERROR, _("mknod failed: %s"), path);
		change_file_status(path, i);
	}
}

static void expand_fs(char *path, struct cramfs_inode *inode)
{
	if (S_ISDIR(inode->mode))
		do_directory(path, inode);
	else if (S_ISREG(inode->mode))
		do_file(path, inode);
	else if (S_ISLNK(inode->mode))
		do_symlink(path, inode);
	else
		do_special_inode(path, inode);
}

static void test_fs(int start)
{
	struct cramfs_inode *root;

	root = read_super();
	umask(0);
	euid = geteuid();
	stream.next_in = NULL;
	stream.avail_in = 0;
	inflateInit(&stream);
	expand_fs(extract_dir, root);
	inflateEnd(&stream);
	if (start_data != ~0UL) {
		if (start_data < (sizeof(struct cramfs_super) + start))
			errx(FSCK_UNCORRECTED,
			     _("directory data start (%ld) < sizeof(struct cramfs_super) + start (%ld)"),
			     start_data, sizeof(struct cramfs_super) + start);
		if (end_dir != start_data)
			errx(FSCK_UNCORRECTED,
			     _("directory data end (%ld) != file data start (%ld)"),
			     end_dir, start_data);
	}
	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2)
		if (end_data > super.size)
			errx(FSCK_UNCORRECTED, _("invalid file data offset"));

	iput(root);		/* free(root) */
}
#endif /* INCLUDE_FS_TESTS */

int main(int argc, char **argv)
{
	int c;			/* for getopt */
	int start = 0;
	size_t length = 0;

	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	page_size = getpagesize();

	if (argc)
		progname = argv[0];

	outbuffer = xmalloc(page_size * 2);

	/* command line options */
	while ((c = getopt(argc, argv, "hx:v")) != EOF)
		switch (c) {
		case 'h':
			usage(FSCK_OK);
			break;
		case 'x':
#ifdef INCLUDE_FS_TESTS
			opt_extract = 1;
			extract_dir = optarg;
			break;
#else
			errx(FSCK_USAGE, _("compiled without -x support"));
#endif
		case 'v':
			opt_verbose++;
			break;
		}

	if ((argc - optind) != 1)
		usage(FSCK_USAGE);
	filename = argv[optind];

	test_super(&start, &length);
	test_crc(start);
#ifdef INCLUDE_FS_TESTS
	test_fs(start);
#endif

	if (opt_verbose)
		printf(_("%s: OK\n"), filename);

	exit(FSCK_OK);
}
