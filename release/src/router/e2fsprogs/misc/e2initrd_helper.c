/*
 * e2initrd_helper.c - Get the filesystem table
 *
 * Copyright 2004 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "blkid/blkid.h"

#include "../version.h"
#include "nls-enable.h"

const char * program_name = "get_fstab";
char * device_name;
static int open_flag;
static int root_type;
static blkid_cache cache = NULL;

struct mem_file {
	char	*buf;
	int	size;
	int	ptr;
};

struct fs_info {
	char  *device;
	char  *mountpt;
	char  *type;
	char  *opts;
	int   freq;
	int   passno;
	int   flags;
	struct fs_info *next;
};

static void usage(void)
{
	fprintf(stderr,
		_("Usage: %s -r device\n"), program_name);
	exit (1);
}

static errcode_t get_file(ext2_filsys fs, const char * filename,
		   struct mem_file *ret_file)
{
	errcode_t	retval;
	char 		*buf;
	ext2_file_t	e2_file;
	unsigned int	got;
	struct ext2_inode inode;
	ext2_ino_t	ino;

	ret_file->buf = 0;
	ret_file->size = 0;
	ret_file->ptr = 0;

	retval = ext2fs_namei(fs, EXT2_ROOT_INO, EXT2_ROOT_INO,
			      filename, &ino);
	if (retval)
		return retval;

	retval = ext2fs_read_inode(fs, ino, &inode);
	if (retval)
		return retval;

	if (inode.i_size_high || (inode.i_size > 65536))
		return EFBIG;

	buf = malloc(inode.i_size + 1);
	if (!buf)
		return ENOMEM;
	memset(buf, 0, inode.i_size+1);

	retval = ext2fs_file_open(fs, ino, 0, &e2_file);
	if (retval)
		return retval;

	retval = ext2fs_file_read(e2_file, buf, inode.i_size, &got);
	if (retval)
		goto errout;

	retval = ext2fs_file_close(e2_file);
	if (retval)
		return retval;

	ret_file->buf = buf;
	ret_file->size = (int) got;

errout:
	ext2fs_file_close(e2_file);
	return retval;
}

static char *get_line(struct mem_file *file)
{
	char	*cp, *ret;
	int	s = 0;

	cp = file->buf + file->ptr;
	while (*cp && *cp != '\n') {
		cp++;
		s++;
	}
	ret = malloc(s+1);
	if (!ret)
		return 0;
	ret[s]=0;
	memcpy(ret, file->buf + file->ptr, s);
	while (*cp && (*cp == '\n' || *cp == '\r')) {
		cp++;
		s++;
	}
	file->ptr += s;
	return ret;
}

static int mem_file_eof(struct mem_file *file)
{
	return (file->ptr >= file->size);
}

/*
 * fstab parsing code
 */
static char *string_copy(const char *s)
{
	char	*ret;

	if (!s)
		return 0;
	ret = malloc(strlen(s)+1);
	if (ret)
		strcpy(ret, s);
	return ret;
}

static char *skip_over_blank(char *cp)
{
	while (*cp && isspace(*cp))
		cp++;
	return cp;
}

static char *skip_over_word(char *cp)
{
	while (*cp && !isspace(*cp))
		cp++;
	return cp;
}

static char *parse_word(char **buf)
{
	char *word, *next;

	word = *buf;
	if (*word == 0)
		return 0;

	word = skip_over_blank(word);
	next = skip_over_word(word);
	if (*next)
		*next++ = 0;
	*buf = next;
	return word;
}

static void parse_escape(char *word)
{
	char	*p, *q;
	int	ac, i;

	if (!word)
		return;

	for (p = word, q = word; *p; p++, q++) {
		*q = *p;
		if (*p != '\\')
			continue;
		if (*++p == 0)
			break;
		if (*p == 't') {
			*q = '\t';
			continue;
		}
		if (*p == 'n') {
			*q = '\n';
			continue;
		}
		if (!isdigit(*p)) {
			*q = *p;
			continue;
		}
		ac = 0;
		for (i = 0; i < 3; i++, p++) {
			if (!isdigit(*p))
				break;
			ac = (ac * 8) + (*p - '0');
		}
		*q = ac;
		p--;
	}
	*q = 0;
}

static int parse_fstab_line(char *line, struct fs_info *fs)
{
	char	*dev, *device, *mntpnt, *type, *opts, *freq, *passno, *cp;

	if ((cp = strchr(line, '#')))
		*cp = 0;	/* Ignore everything after the comment char */
	cp = line;

	device = parse_word(&cp);
	mntpnt = parse_word(&cp);
	type = parse_word(&cp);
	opts = parse_word(&cp);
	freq = parse_word(&cp);
	passno = parse_word(&cp);

	if (!device)
		return -1;	/* Allow blank lines */

	if (!mntpnt || !type)
		return -1;

	parse_escape(device);
	parse_escape(mntpnt);
	parse_escape(type);
	parse_escape(opts);
	parse_escape(freq);
	parse_escape(passno);

	dev = blkid_get_devname(cache, device, NULL);
	if (dev)
		device = dev;

	if (strchr(type, ','))
		type = 0;

	fs->device = string_copy(device);
	fs->mountpt = string_copy(mntpnt);
	fs->type = string_copy(type);
	fs->opts = string_copy(opts ? opts : "");
	fs->freq = freq ? atoi(freq) : -1;
	fs->passno = passno ? atoi(passno) : -1;
	fs->flags = 0;
	fs->next = NULL;

	free(dev);

	return 0;
}

static void free_fstab_line(struct fs_info *fs)
{
	if (fs->device)
		fs->device = 0;
	if (fs->mountpt)
		fs->mountpt = 0;
	if (fs->type)
		fs->type = 0;
	if (fs->opts)
		fs->opts = 0;
	memset(fs, 0, sizeof(struct fs_info));
}


static void PRS(int argc, char **argv)
{
	int c;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif

	while ((c = getopt(argc, argv, "rv")) != EOF) {
		switch (c) {
		case 'r':
			root_type++;
			break;

		case 'v':
			printf("%s %s (%s)\n", program_name,
			       E2FSPROGS_VERSION, E2FSPROGS_DATE);
			break;
		default:
			usage();
		}
	}
	if (optind < argc - 1 || optind == argc)
		usage();
	device_name = blkid_get_devname(NULL, argv[optind], NULL);
	if (!device_name) {
		com_err("tune2fs", 0, _("Unable to resolve '%s'"),
			argv[optind]);
		exit(1);
	}
}

static void get_root_type(ext2_filsys fs)
{
	errcode_t retval;
	struct mem_file file;
	char 		*buf;
	struct fs_info fs_info;
	int		ret;

	retval = get_file(fs, "/etc/fstab", &file);

	while (!mem_file_eof(&file)) {
		buf = get_line(&file);
		if (!buf)
			continue;

		ret = parse_fstab_line(buf, &fs_info);
		if (ret < 0)
			goto next_line;

		if (!strcmp(fs_info.mountpt, "/"))
			printf("%s\n", fs_info.type);

		free_fstab_line(&fs_info);

	next_line:
		free(buf);
	}
}


int main (int argc, char ** argv)
{
	errcode_t retval;
	ext2_filsys fs;
	io_manager io_ptr;

	add_error_table(&et_ext2_error_table);

	blkid_get_cache(&cache, NULL);
	PRS(argc, argv);

#ifdef CONFIG_TESTIO_DEBUG
	if (getenv("TEST_IO_FLAGS") || getenv("TEST_IO_BLOCK")) {
		io_ptr = test_io_manager;
		test_io_backing_manager = unix_io_manager;
	} else
#endif
		io_ptr = unix_io_manager;
	retval = ext2fs_open (device_name, open_flag, 0, 0, io_ptr, &fs);
        if (retval)
		exit(1);

	if (root_type)
		get_root_type(fs);

	remove_error_table(&et_ext2_error_table);
	return (ext2fs_close (fs) ? 1 : 0);
}
