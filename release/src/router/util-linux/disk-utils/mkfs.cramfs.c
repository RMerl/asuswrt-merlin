/*
 * mkcramfs - make a cramfs file system
 *
 * Copyright (C) 1999-2002 Transmeta Corporation
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
 */

/*
 * Old version would die on largish filesystems. Change to mmap the
 * files one by one instaed of all simultaneously. - aeb, 2002-11-01
 */

#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <zconf.h>
#include <zlib.h>

#include "c.h"
#include "cramfs.h"
#include "md5.h"
#include "nls.h"
#include "exitcodes.h"
#include "strutils.h"
#define XALLOC_EXIT_CODE MKFS_ERROR
#include "xalloc.h"

/* The kernel only supports PAD_SIZE of 0 and 512. */
#define PAD_SIZE 512

static int verbose = 0;

static unsigned int blksize; /* settable via -b option */
static long total_blocks = 0, total_nodes = 1; /* pre-count the root node */
static int image_length = 0;
static int cramfs_is_big_endian = 0; /* target is big endian */

/*
 * If opt_holes is set, then mkcramfs can create explicit holes in the
 * data, which saves 26 bytes per hole (which is a lot smaller a
 * saving than for most filesystems).
 *
 * Note that kernels up to at least 2.3.39 don't support cramfs holes,
 * which is why this is turned off by default.
 */
static int opt_edition = 0;
static int opt_errors = 0;
static int opt_holes = 0;
static int opt_pad = 0;
static char *opt_image = NULL;
static char *opt_name = NULL;

static int warn_dev = 0;
static int warn_gid = 0;
static int warn_namelen = 0;
static int warn_skip = 0;
static int warn_size = 0;
static int warn_uid = 0;

#ifndef MIN
# define MIN(_a,_b) ((_a) < (_b) ? (_a) : (_b))
#endif

/* entry.flags */
#define CRAMFS_EFLAG_MD5	1
#define CRAMFS_EFLAG_INVALID	2

/* In-core version of inode / directory entry. */
struct entry {
	/* stats */
	unsigned char *name;
	unsigned int mode, size, uid, gid;
	unsigned char md5sum[MD5LENGTH];
	unsigned char flags;	   /* CRAMFS_EFLAG_* */

	/* FS data */
	char *path;
	int fd;			    /* temporarily open files while mmapped */
	struct entry *same;	    /* points to other identical file */
	unsigned int offset;        /* pointer to compressed data in archive */
	unsigned int dir_offset;    /* offset of directory entry in archive */

	/* organization */
	struct entry *child;	    /* NULL for non-directory and empty dir */
	struct entry *next;
};

/*
 * Width of various bitfields in struct cramfs_inode.
 * Used only to generate warnings.
 */
#define CRAMFS_SIZE_WIDTH 24
#define CRAMFS_UID_WIDTH 16
#define CRAMFS_GID_WIDTH 8
#define CRAMFS_OFFSET_WIDTH 26

/* Input status of 0 to print help and exit without an error. */
static void
usage(int status) {
	FILE *stream = status ? stderr : stdout;

	fprintf(stream,
		_("usage: %s [-h] [-v] [-b blksize] [-e edition] [-N endian] [-i file] "
		  "[-n name] dirname outfile\n"
		  " -h         print this help\n"
		  " -v         be verbose\n"
		  " -E         make all warnings errors "
		    "(non-zero exit status)\n"
		  " -b blksize use this blocksize, must equal page size\n"
		  " -e edition set edition number (part of fsid)\n"
		  " -N endian  set cramfs endianness (big|little|host), default host\n"
		  " -i file    insert a file image into the filesystem "
		    "(requires >= 2.4.0)\n"
		  " -n name    set name of cramfs filesystem\n"
		  " -p         pad by %d bytes for boot code\n"
		  " -s         sort directory entries (old option, ignored)\n"
		  " -z         make explicit holes (requires >= 2.3.39)\n"
		  " dirname    root of the filesystem to be compressed\n"
		  " outfile    output file\n"),
		program_invocation_short_name, PAD_SIZE);

	exit(status);
}

static char *
do_mmap(char *path, unsigned int size, unsigned int mode){
	int fd;
	char *start;

	if (!size)
		return NULL;

	if (S_ISLNK(mode)) {
		start = xmalloc(size);
		if (readlink(path, start, size) < 0) {
			perror(path);
			warn_skip = 1;
			start = NULL;
		}
		return start;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		warn_skip = 1;
		return NULL;
	}

	start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (-1 == (int) (long) start)
		err(MKFS_ERROR, "mmap");
	close(fd);

	return start;
}

static void
do_munmap(char *start, unsigned int size, unsigned int mode){
	if (S_ISLNK(mode))
		free(start);
	else
		munmap(start, size);
}

/* compute md5sums, so that we do not have to compare every pair of files */
static void
mdfile(struct entry *e) {
	MD5_CTX ctx;
	char *start;

	start = do_mmap(e->path, e->size, e->mode);
	if (start == NULL) {
		e->flags |= CRAMFS_EFLAG_INVALID;
	} else {
		MD5Init(&ctx);
		MD5Update(&ctx, (unsigned char *) start, e->size);
		MD5Final(e->md5sum, &ctx);

		do_munmap(start, e->size, e->mode);

		e->flags |= CRAMFS_EFLAG_MD5;
	}
}

/* md5 digests are equal; files are almost certainly the same,
   but just to be sure, do the comparison */
static int
identical_file(struct entry *e1, struct entry *e2){
	char *start1, *start2;
	int equal;

	start1 = do_mmap(e1->path, e1->size, e1->mode);
	if (!start1)
		return 0;
	start2 = do_mmap(e2->path, e2->size, e2->mode);
	if (!start2)
		return 0;
	equal = !memcmp(start1, start2, e1->size);
	do_munmap(start1, e1->size, e1->mode);
	do_munmap(start2, e2->size, e2->mode);
	return equal;
}

/*
 * The longest file name component to allow for in the input directory tree.
 * Ext2fs (and many others) allow up to 255 bytes.  A couple of filesystems
 * allow longer (e.g. smbfs 1024), but there isn't much use in supporting
 * >255-byte names in the input directory tree given that such names get
 * truncated to 255 bytes when written to cramfs.
 */
#define MAX_INPUT_NAMELEN 255

static int find_identical_file(struct entry *orig, struct entry *new, loff_t *fslen_ub)
{
	if (orig == new)
		return 1;
	if (!orig)
		return 0;
	if (orig->size == new->size && orig->path) {
		if (!orig->flags)
			mdfile(orig);
		if (!new->flags)
			mdfile(new);

		if ((orig->flags & CRAMFS_EFLAG_MD5) &&
		    (new->flags & CRAMFS_EFLAG_MD5) &&
		    !memcmp(orig->md5sum, new->md5sum, MD5LENGTH) &&
		    identical_file(orig, new)) {
			new->same = orig;
			*fslen_ub -= new->size;
			return 1;
		}
	}
	return find_identical_file(orig->child, new, fslen_ub) ||
		   find_identical_file(orig->next, new, fslen_ub);
}

static void eliminate_doubles(struct entry *root, struct entry *orig, loff_t *fslen_ub) {
	if (orig) {
		if (orig->size && orig->path)
			find_identical_file(root,orig, fslen_ub);
		eliminate_doubles(root,orig->child, fslen_ub);
		eliminate_doubles(root,orig->next, fslen_ub);
	}
}

/*
 * We define our own sorting function instead of using alphasort which
 * uses strcoll and changes ordering based on locale information.
 */
static int cramsort (const struct dirent **a, const struct dirent **b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

static unsigned int parse_directory(struct entry *root_entry, const char *name, struct entry **prev, loff_t *fslen_ub)
{
	struct dirent **dirlist;
	int totalsize = 0, dircount, dirindex;
	char *path, *endpath;
	size_t len = strlen(name);

	/* Set up the path. */
	/* TODO: Reuse the parent's buffer to save memcpy'ing and duplication. */
	path = xmalloc(len + 1 + MAX_INPUT_NAMELEN + 1);
	memcpy(path, name, len);
	endpath = path + len;
	*endpath = '/';
	endpath++;

	/* read in the directory and sort */
	dircount = scandir(name, &dirlist, 0, cramsort);

	if (dircount < 0)
		err(MKFS_ERROR, _("could not read directory %s"), name);

	/* process directory */
	for (dirindex = 0; dirindex < dircount; dirindex++) {
		struct dirent *dirent;
		struct entry *entry;
		struct stat st;
		int size;
		size_t namelen;

		dirent = dirlist[dirindex];

		/* Ignore "." and ".." - we won't be adding them
		   to the archive */
		if (dirent->d_name[0] == '.') {
			if (dirent->d_name[1] == '\0')
				continue;
			if (dirent->d_name[1] == '.') {
				if (dirent->d_name[2] == '\0')
					continue;
			}
		}
		namelen = strlen(dirent->d_name);
		if (namelen > MAX_INPUT_NAMELEN)
			errx(MKFS_ERROR,
				_("Very long (%zu bytes) filename `%s' found.\n"
				  " Please increase MAX_INPUT_NAMELEN in "
				  "mkcramfs.c and recompile.  Exiting."),
				namelen, dirent->d_name);
		memcpy(endpath, dirent->d_name, namelen + 1);

		if (lstat(path, &st) < 0) {
			perror(endpath);
			warn_skip = 1;
			continue;
		}
		entry = xcalloc(1, sizeof(struct entry));
		entry->name = (unsigned char *)xstrdup(dirent->d_name);
		if (namelen > 255) {
			/* Can't happen when reading from ext2fs. */

			/* TODO: we ought to avoid chopping in half
			   multi-byte UTF8 characters. */
			entry->name[namelen = 255] = '\0';
			warn_namelen = 1;
		}
		entry->mode = st.st_mode;
		entry->size = st.st_size;
		entry->uid = st.st_uid;
		if (entry->uid >= 1 << CRAMFS_UID_WIDTH)
			warn_uid = 1;
		entry->gid = st.st_gid;
		if (entry->gid >= 1 << CRAMFS_GID_WIDTH)
			/* TODO: We ought to replace with a default
			   gid instead of truncating; otherwise there
			   are security problems.  Maybe mode should
			   be &= ~070.  Same goes for uid once Linux
			   supports >16-bit uids. */
			warn_gid = 1;
		size = sizeof(struct cramfs_inode) + ((namelen + 3) & ~3);
		*fslen_ub += size;
		if (S_ISDIR(st.st_mode)) {
			entry->size = parse_directory(root_entry, path, &entry->child, fslen_ub);
		} else if (S_ISREG(st.st_mode)) {
			entry->path = xstrdup(path);
			if (entry->size) {
				if (entry->size >= (1 << CRAMFS_SIZE_WIDTH)) {
					warn_size = 1;
					entry->size = (1 << CRAMFS_SIZE_WIDTH) - 1;
				}
			}
		} else if (S_ISLNK(st.st_mode)) {
			entry->path = xstrdup(path);
		} else if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) {
			/* maybe we should skip sockets */
			entry->size = 0;
		} else {
			entry->size = st.st_rdev;
			if (entry->size & -(1<<CRAMFS_SIZE_WIDTH))
				warn_dev = 1;
		}

		if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
			int blocks = ((entry->size - 1) / blksize + 1);

			/* block pointers & data expansion allowance + data */
			if (entry->size)
				*fslen_ub += (4+26)*blocks + entry->size + 3;
		}

		/* Link it into the list */
		*prev = entry;
		prev = &entry->next;
		totalsize += size;
	}
	free(path);
	free(dirlist);		/* allocated by scandir() with malloc() */
	return totalsize;
}

/* Returns sizeof(struct cramfs_super), which includes the root inode. */
static unsigned int write_superblock(struct entry *root, char *base, int size)
{
	struct cramfs_super *super = (struct cramfs_super *) base;
	unsigned int offset = sizeof(struct cramfs_super) + image_length;

	if (opt_pad) {
		offset += opt_pad;
	}

	super->magic = CRAMFS_MAGIC;
	super->flags = CRAMFS_FLAG_FSID_VERSION_2 | CRAMFS_FLAG_SORTED_DIRS;
	if (opt_holes)
		super->flags |= CRAMFS_FLAG_HOLES;
	if (image_length > 0)
		super->flags |= CRAMFS_FLAG_SHIFTED_ROOT_OFFSET;
	super->size = size;
	memcpy(super->signature, CRAMFS_SIGNATURE, sizeof(super->signature));

	super->fsid.crc = crc32(0L, Z_NULL, 0);
	super->fsid.edition = opt_edition;
	super->fsid.blocks = total_blocks;
	super->fsid.files = total_nodes;

	memset(super->name, 0x00, sizeof(super->name));
	if (opt_name)
		strncpy((char *)super->name, opt_name, sizeof(super->name));
	else
		strncpy((char *)super->name, "Compressed", sizeof(super->name));

	super->root.mode = root->mode;
	super->root.uid = root->uid;
	super->root.gid = root->gid;
	super->root.size = root->size;
	super->root.offset = offset >> 2;

	super_toggle_endianness(cramfs_is_big_endian, super);
	inode_from_host(cramfs_is_big_endian, &super->root, &super->root);

	return offset;
}

static void set_data_offset(struct entry *entry, char *base, unsigned long offset)
{
	struct cramfs_inode *inode = (struct cramfs_inode *) (base + entry->dir_offset);
	inode_to_host(cramfs_is_big_endian, inode, inode);
	if (offset >= (1 << (2 + CRAMFS_OFFSET_WIDTH)))
		errx(MKFS_ERROR, _("filesystem too big.  Exiting."));
	inode->offset = (offset >> 2);
	inode_from_host(cramfs_is_big_endian, inode, inode);
}


/*
 * We do a width-first printout of the directory
 * entries, using a stack to remember the directories
 * we've seen.
 */
static unsigned int write_directory_structure(struct entry *entry, char *base, unsigned int offset)
{
	int stack_entries = 0;
	int stack_size = 64;
	struct entry **entry_stack;

	entry_stack = xmalloc(stack_size * sizeof(struct entry *));

	for (;;) {
		int dir_start = stack_entries;
		while (entry) {
			struct cramfs_inode *inode =
				(struct cramfs_inode *) (base + offset);
			size_t len = strlen((const char *)entry->name);

			entry->dir_offset = offset;

			inode->mode = entry->mode;
			inode->uid = entry->uid;
			inode->gid = entry->gid;
			inode->size = entry->size;
			inode->offset = 0;
			/* Non-empty directories, regfiles and symlinks will
			   write over inode->offset later. */

			offset += sizeof(struct cramfs_inode);
			total_nodes++;	/* another node */
			memcpy(base + offset, entry->name, len);
			/* Pad up the name to a 4-byte boundary */
			while (len & 3) {
				*(base + offset + len) = '\0';
				len++;
			}
			inode->namelen = len >> 2;
			offset += len;

			if (verbose)
				printf("  %s\n", entry->name);
			if (entry->child) {
				if (stack_entries >= stack_size) {
					stack_size *= 2;
					entry_stack = xrealloc(entry_stack, stack_size * sizeof(struct entry *));
				}
				entry_stack[stack_entries] = entry;
				stack_entries++;
			}
			inode_from_host(cramfs_is_big_endian, inode, inode);
			entry = entry->next;
		}

		/*
		 * Reverse the order the stack entries pushed during
		 * this directory, for a small optimization of disk
		 * access in the created fs.  This change makes things
		 * `ls -UR' order.
		 */
		{
			struct entry **lo = entry_stack + dir_start;
			struct entry **hi = entry_stack + stack_entries;
			struct entry *tmp;

			while (lo < --hi) {
				tmp = *lo;
				*lo++ = *hi;
				*hi = tmp;
			}
		}

		/* Pop a subdirectory entry from the stack, and recurse. */
		if (!stack_entries)
			break;
		stack_entries--;
		entry = entry_stack[stack_entries];

		set_data_offset(entry, base, offset);
		if (verbose)
			printf("'%s':\n", entry->name);
		entry = entry->child;
	}
	free(entry_stack);
	return offset;
}

static int is_zero(unsigned char const *begin, unsigned len)
{
	if (opt_holes)
		/* Returns non-zero iff the first LEN bytes from BEGIN are
		   all NULs. */
		return (len-- == 0 ||
			(begin[0] == '\0' &&
			 (len-- == 0 ||
			  (begin[1] == '\0' &&
			   (len-- == 0 ||
			    (begin[2] == '\0' &&
			     (len-- == 0 ||
			      (begin[3] == '\0' &&
			       memcmp(begin, begin + 4, len) == 0))))))));
	else
		/* Never create holes. */
		return 0;
}

/*
 * One 4-byte pointer per block and then the actual blocked
 * output. The first block does not need an offset pointer,
 * as it will start immediately after the pointer block;
 * so the i'th pointer points to the end of the i'th block
 * (i.e. the start of the (i+1)'th block or past EOF).
 *
 * Note that size > 0, as a zero-sized file wouldn't ever
 * have gotten here in the first place.
 */
static unsigned int
do_compress(char *base, unsigned int offset, unsigned char const *name,
	    char *path, unsigned int size, unsigned int mode)
{
	unsigned long original_size, original_offset, new_size, blocks, curr;
	long change;
	char *start;
	Bytef *p;

	/* get uncompressed data */
	start = do_mmap(path, size, mode);
	if (start == NULL)
		return offset;
	p = (Bytef *) start;

	original_size = size;
	original_offset = offset;
	blocks = (size - 1) / blksize + 1;
	curr = offset + 4 * blocks;

	total_blocks += blocks;

	do {
		uLongf len = 2 * blksize;
		uLongf input = size;
		if (input > blksize)
			input = blksize;
		size -= input;
		if (!is_zero (p, input)) {
			compress((Bytef *)(base + curr), &len, p, input);
			curr += len;
		}
		p += input;

		if (len > blksize*2) {
			/* (I don't think this can happen with zlib.) */
			printf(_("AIEEE: block \"compressed\" to > "
				 "2*blocklength (%ld)\n"),
			       len);
			exit(MKFS_ERROR);
		}

		*(uint32_t *) (base + offset) = u32_toggle_endianness(cramfs_is_big_endian, curr);
		offset += 4;
	} while (size);

	do_munmap(start, original_size, mode);

	curr = (curr + 3) & ~3;
	new_size = curr - original_offset;
	/* TODO: Arguably, original_size in these 2 lines should be
	   st_blocks * 512.  But if you say that, then perhaps
	   administrative data should also be included in both. */
	change = new_size - original_size;
	if (verbose)
		printf(_("%6.2f%% (%+ld bytes)\t%s\n"),
		       (change * 100) / (double) original_size, change, name);

	return curr;
}


/*
 * Traverse the entry tree, writing data for every item that has
 * non-null entry->path (i.e. every symlink and non-empty
 * regfile).
 */
static unsigned int
write_data(struct entry *entry, char *base, unsigned int offset) {
	struct entry *e;

	for (e = entry; e; e = e->next) {
		if (e->path) {
			if (e->same) {
				set_data_offset(e, base, e->same->offset);
				e->offset = e->same->offset;
			} else if (e->size) {
				set_data_offset(e, base, offset);
				e->offset = offset;
				offset = do_compress(base, offset, e->name,
						     e->path, e->size,e->mode);
			}
		} else if (e->child)
			offset = write_data(e->child, base, offset);
	}
	return offset;
}

static unsigned int write_file(char *file, char *base, unsigned int offset)
{
	int fd;
	char *buf;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		err(MKFS_ERROR, _("cannot open file %s"), file);
	buf = mmap(NULL, image_length, PROT_READ, MAP_PRIVATE, fd, 0);
	memcpy(base + offset, buf, image_length);
	munmap(buf, image_length);
	if (close (fd) < 0)
		err(MKFS_ERROR, _("cannot close file %s"), file);
	/* Pad up the image_length to a 4-byte boundary */
	while (image_length & 3) {
		*(base + offset + image_length) = '\0';
		image_length++;
	}
	return (offset + image_length);
}

/*
 * Maximum size fs you can create is roughly 256MB.  (The last file's
 * data must begin within 256MB boundary but can extend beyond that.)
 *
 * Note that if you want it to fit in a ROM then you're limited to what the
 * hardware and kernel can support (64MB?).
 */
static unsigned int
maxfslen(void) {
	return (((1 << CRAMFS_OFFSET_WIDTH) - 1) << 2)    /* offset */
		+ (1 << CRAMFS_SIZE_WIDTH) - 1            /* filesize */
		+ (1 << CRAMFS_SIZE_WIDTH) * 4 / blksize; /* block pointers */
}

/*
 * Usage:
 *
 *      mkcramfs directory-name outfile
 *
 * where "directory-name" is simply the root of the directory
 * tree that we want to generate a compressed filesystem out
 * of.
 */
int main(int argc, char **argv)
{
	struct stat st;		/* used twice... */
	struct entry *root_entry;
	char *rom_image;
	ssize_t offset, written;
	int fd;
	/* initial guess (upper-bound) of required filesystem size */
	loff_t fslen_ub = sizeof(struct cramfs_super);
	unsigned int fslen_max;
	char const *dirname, *outfile;
	uint32_t crc = crc32(0L, Z_NULL, 0);
	int c;
	cramfs_is_big_endian = HOST_IS_BIG_ENDIAN; /* default is to use host order */

	blksize = getpagesize();
	total_blocks = 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* command line options */
	while ((c = getopt(argc, argv, "hb:Ee:i:n:N:psVvz")) != EOF) {
		switch (c) {
		case 'h':
			usage(MKFS_OK);
		case 'b':
			blksize = strtoll_or_err(optarg, _("failed to parse blocksize argument"));
			if (blksize <= 0)
				usage(MKFS_USAGE);
			break;
		case 'E':
			opt_errors = 1;
			break;
		case 'e':
			opt_edition = strtoll_or_err(optarg, _("edition number argument failed"));
			break;
		case 'N':
			if (strcmp(optarg, "big") == 0)
				cramfs_is_big_endian = 1;
			else if (strcmp(optarg, "little") == 0)
				cramfs_is_big_endian = 0;
			else if (strcmp(optarg, "host") == 0)
				/* default */ ;
			else
				errx(MKFS_USAGE, _("invalid endianness given."
						   " Must be 'big', 'little', or 'host'"));
			break;
		case 'i':
			opt_image = optarg;
			if (lstat(opt_image, &st) < 0)
				err(MKFS_USAGE, _("cannot stat %s"), opt_image);
			image_length = st.st_size; /* may be padded later */
			fslen_ub += (image_length + 3); /* 3 is for padding */
			break;
		case 'n':
			opt_name = optarg;
			break;
		case 'p':
			opt_pad = PAD_SIZE;
			fslen_ub += PAD_SIZE;
			break;
		case 's':
			/* old option, ignored */
			break;
		case 'V':
			printf(_("%s from %s\n"),
			       program_invocation_short_name, PACKAGE_STRING);
			exit(MKFS_OK);
		case 'v':
			verbose = 1;
			break;
		case 'z':
			opt_holes = 1;
			break;
		}
	}

	if ((argc - optind) != 2)
		usage(MKFS_USAGE);
	dirname = argv[optind];
	outfile = argv[optind + 1];

	if (stat(dirname, &st) < 0)
		err(MKFS_USAGE, _("cannot stat %s"), dirname);
	fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0)
		err(MKFS_USAGE, _("cannot open %s"), outfile);

	root_entry = xcalloc(1, sizeof(struct entry));
	root_entry->mode = st.st_mode;
	root_entry->uid = st.st_uid;
	root_entry->gid = st.st_gid;

	root_entry->size = parse_directory(root_entry, dirname, &root_entry->child, &fslen_ub);

	/* always allocate a multiple of blksize bytes because that's
	   what we're going to write later on */
	fslen_ub = ((fslen_ub - 1) | (blksize - 1)) + 1;
	fslen_max = maxfslen();

	if (fslen_ub > fslen_max) {
		warnx(	_("warning: guestimate of required size (upper bound) "
			  "is %lldMB, but maximum image size is %uMB.  "
			  "We might die prematurely."),
			(long long)fslen_ub >> 20,
			fslen_max >> 20);
		fslen_ub = fslen_max;
	}

	/* find duplicate files */
	eliminate_doubles(root_entry,root_entry, &fslen_ub);

	/* TODO: Why do we use a private/anonymous mapping here
	   followed by a write below, instead of just a shared mapping
	   and a couple of ftruncate calls?  Is it just to save us
	   having to deal with removing the file afterwards?  If we
	   really need this huge anonymous mapping, we ought to mmap
	   in smaller chunks, so that the user doesn't need nn MB of
	   RAM free.  If the reason is to be able to write to
	   un-mmappable block devices, then we could try shared mmap
	   and revert to anonymous mmap if the shared mmap fails. */
	rom_image = mmap(NULL,
			 fslen_ub?fslen_ub:1,
			 PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS,
			 -1, 0);

	if (-1 == (int) (long) rom_image)
		err(MKFS_ERROR, _("ROM image map"));

	/* Skip the first opt_pad bytes for boot loader code */
	offset = opt_pad;
	memset(rom_image, 0x00, opt_pad);

	/* Skip the superblock and come back to write it later. */
	offset += sizeof(struct cramfs_super);

	/* Insert a file image. */
	if (opt_image) {
		if (verbose)
			printf(_("Including: %s\n"), opt_image);
		offset = write_file(opt_image, rom_image, offset);
	}

	offset = write_directory_structure(root_entry->child, rom_image, offset);
	if (verbose)
		printf(_("Directory data: %zd bytes\n"), offset);

	offset = write_data(root_entry, rom_image, offset);

	/* We always write a multiple of blksize bytes, so that
	   losetup works. */
	offset = ((offset - 1) | (blksize - 1)) + 1;
	if (verbose)
		printf(_("Everything: %zd kilobytes\n"), offset >> 10);

	/* Write the superblock now that we can fill in all of the fields. */
	write_superblock(root_entry, rom_image+opt_pad, offset);
	if (verbose)
		printf(_("Super block: %zd bytes\n"),
		       sizeof(struct cramfs_super));

	/* Put the checksum in. */
	crc = crc32(crc, (unsigned char *) (rom_image+opt_pad), (offset-opt_pad));
	((struct cramfs_super *) (rom_image+opt_pad))->fsid.crc = u32_toggle_endianness(cramfs_is_big_endian, crc);
	if (verbose)
		printf(_("CRC: %x\n"), crc);

	/* Check to make sure we allocated enough space. */
	if (fslen_ub < offset)
		errx(MKFS_ERROR,
			_("not enough space allocated for ROM image "
			  "(%lld allocated, %zu used)"),
			(long long) fslen_ub, offset);

	written = write(fd, rom_image, offset);
	if (written < 0)
		err(MKFS_ERROR, _("ROM image"));
	if (offset != written)
		errx(MKFS_ERROR, _("ROM image write failed (%zd %zd)"),
			written, offset);

	/*
	 * (These warnings used to come at the start, but they scroll off
	 * the screen too quickly.)
	 */
	if (warn_namelen)
		/* Can't happen when reading from ext2fs. */
		/* Bytes, not chars: think UTF8. */
		warnx(_("warning: filenames truncated to 255 bytes."));
	if (warn_skip)
		warnx(_("warning: files were skipped due to errors."));
	if (warn_size)
		warnx(_("warning: file sizes truncated to %luMB "
			"(minus 1 byte)."), 1L << (CRAMFS_SIZE_WIDTH - 20));
	if (warn_uid)
		/* (not possible with current Linux versions) */
		warnx(_("warning: uids truncated to %u bits.  "
			"(This may be a security concern.)"), CRAMFS_UID_WIDTH);
	if (warn_gid)
		warnx(_("warning: gids truncated to %u bits.  "
			"(This may be a security concern.)"), CRAMFS_GID_WIDTH);
	if (warn_dev)
		warnx(_("WARNING: device numbers truncated to %u bits.  "
			"This almost certainly means\n"
			"that some device files will be wrong."),
		      CRAMFS_OFFSET_WIDTH);
	if (opt_errors &&
	    (warn_namelen|warn_skip|warn_size|warn_uid|warn_gid|warn_dev))
		exit(MKFS_ERROR);

	return EXIT_SUCCESS;
}
