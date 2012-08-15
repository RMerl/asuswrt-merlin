/**
 * ntfscmp - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2005-2006 Szabolcs Szakacsits
 * Copyright (c) 2005      Anton Altaparmakov
 * Copyright (c) 2007      Yura Pakhuchiy
 *
 * This utility compare two NTFS volumes.
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "utils.h"
#include "mst.h"
#include "version.h"
#include "support.h"

static const char *EXEC_NAME = "ntfscmp";

static const char *invalid_ntfs_msg =
"Apparently device '%s' doesn't have a valid NTFS.\n"
"Maybe you selected the wrong partition? Or the whole disk instead of a\n"
"partition (e.g. /dev/hda, not /dev/hda1)?\n";

static const char *corrupt_volume_msg =
"Apparently you have a corrupted NTFS. Please run the filesystem checker\n"
"on Windows by invoking chkdsk /f. Don't forget the /f (force) parameter,\n"
"it's important! You probably also need to reboot Windows to take effect.\n";

static const char *hibernated_volume_msg =
"Apparently the NTFS partition is hibernated. Windows must be resumed and\n"
"turned off properly\n";


static struct {
	int debug;
	int show_progress;
	int verbose;
	char *vol1;
	char *vol2;
} opt;


#define NTFS_PROGBAR		0x0001
#define NTFS_PROGBAR_SUPPRESS	0x0002

struct progress_bar {
	u64 start;
	u64 stop;
	int resolution;
	int flags;
	float unit;
};

/* WARNING: don't modify the text, external tools grep for it */
#define ERR_PREFIX   "ERROR"
#define PERR_PREFIX  ERR_PREFIX "(%d): "
#define NERR_PREFIX  ERR_PREFIX ": "

__attribute__((format(printf, 2, 3)))
static void perr_printf(int newline, const char *fmt, ...)
{
	va_list ap;
	int eo = errno;

	fprintf(stdout, PERR_PREFIX, eo);
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fprintf(stdout, ": %s", strerror(eo));
	if (newline)
		fprintf(stdout, "\n");
	fflush(stdout);
	fflush(stderr);
}

#define perr_print(...)     perr_printf(0, __VA_ARGS__)
#define perr_println(...)   perr_printf(1, __VA_ARGS__)

__attribute__((format(printf, 1, 2)))
static void err_printf(const char *fmt, ...)
{
	va_list ap;

	fprintf(stdout, NERR_PREFIX);
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
	fflush(stderr);
}

/**
 * err_exit
 *
 * Print and error message and exit the program.
 */
__attribute__((noreturn))
__attribute__((format(printf, 1, 2)))
static int err_exit(const char *fmt, ...)
{
	va_list ap;

	fprintf(stdout, NERR_PREFIX);
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
	fflush(stderr);
	exit(1);
}

/**
 * perr_exit
 *
 * Print and error message and exit the program
 */
__attribute__((noreturn))
__attribute__((format(printf, 1, 2)))
static int perr_exit(const char *fmt, ...)
{
	va_list ap;
	int eo = errno;

	fprintf(stdout, PERR_PREFIX, eo);
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	printf(": %s\n", strerror(eo));
	fflush(stdout);
	fflush(stderr);
	exit(1);
}

/**
 * usage - Print a list of the parameters to the program
 *
 * Print a list of the parameters and options for the program.
 *
 * Return:  none
 */
__attribute__((noreturn))
static void usage(void)
{

	printf("\nUsage: %s [OPTIONS] DEVICE1 DEVICE2\n"
		"    Compare two NTFS volumes and tell the differences.\n"
		"\n"
		"    -P, --no-progress-bar  Don't show progress bar\n"
		"    -v, --verbose          More output\n"
		"    -h, --help             Display this help\n"
#ifdef DEBUG
		"    -d, --debug            Show debug information\n"
#endif
		"\n", EXEC_NAME);
	printf("%s%s", ntfs_bugs, ntfs_home);
	exit(1);
}


static void parse_options(int argc, char **argv)
{
	static const char *sopt = "-dhPv";
	static const struct option lopt[] = {
#ifdef DEBUG
		{ "debug",		no_argument,	NULL, 'd' },
#endif
		{ "help",		no_argument,	NULL, 'h' },
		{ "no-progress-bar",	no_argument,	NULL, 'P' },
		{ "verbose",		no_argument,	NULL, 'v' },
		{ NULL, 0, NULL, 0 }
	};

	int c;

	memset(&opt, 0, sizeof(opt));
	opt.show_progress = 1;

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opt.vol1) {
				opt.vol1 = argv[optind - 1];
			} else if (!opt.vol2) {
				opt.vol2 = argv[optind - 1];
			} else {
				err_printf("Too many arguments!\n");
				usage();
			}
			break;
#ifdef DEBUG
		case 'd':
			opt.debug++;
			break;
#endif
		case 'h':
		case '?':
			usage();
		case 'P':
			opt.show_progress = 0;
			break;
		case 'v':
			opt.verbose++;
			break;
		default:
			err_printf("Unknown option '%s'.\n", argv[optind - 1]);
			usage();
			break;
		}
	}

	if (opt.vol1 == NULL || opt.vol2 == NULL) {
		err_printf("You must specify exactly 2 volumes.\n");
		usage();
	}

	/* Redirect stderr to stdout, note fflush()es are essential! */
	fflush(stdout);
	fflush(stderr);
	if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
		perror("Failed to redirect stderr to stdout");
		exit(1);
	}
	fflush(stdout);
	fflush(stderr);

#ifdef DEBUG
	 if (!opt.debug)
		if (!freopen("/dev/null", "w", stderr))
			perr_exit("Failed to redirect stderr to /dev/null");
#endif
}

static ntfs_attr_search_ctx *attr_get_search_ctx(ntfs_inode *ni)
{
	ntfs_attr_search_ctx *ret;

	if ((ret = ntfs_attr_get_search_ctx(ni, NULL)) == NULL)
		perr_println("ntfs_attr_get_search_ctx");

	return ret;
}

static void progress_init(struct progress_bar *p, u64 start, u64 stop, int flags)
{
	p->start = start;
	p->stop = stop;
	p->unit = 100.0 / (stop - start);
	p->resolution = 100;
	p->flags = flags;
}

static void progress_update(struct progress_bar *p, u64 current)
{
	float percent;

	if (!(p->flags & NTFS_PROGBAR))
		return;
	if (p->flags & NTFS_PROGBAR_SUPPRESS)
		return;

	/* WARNING: don't modify the texts, external tools grep for them */
	percent = p->unit * current;
	if (current != p->stop) {
		if ((current - p->start) % p->resolution)
			return;
		printf("%6.2f percent completed\r", percent);
	} else
		printf("100.00 percent completed\n");
	fflush(stdout);
}

static u64 inumber(ntfs_inode *ni)
{
	if (ni->nr_extents >= 0)
		return ni->mft_no;

	return ni->base_ni->mft_no;
}

static int inode_close(ntfs_inode *ni)
{
	if (ni == NULL)
		return 0;

	if (ntfs_inode_close(ni)) {
		perr_println("ntfs_inode_close: inode %llu", inumber(ni));
		return -1;
	}
	return 0;
}

static inline s64 get_nr_mft_records(ntfs_volume *vol)
{
	return vol->mft_na->initialized_size >> vol->mft_record_size_bits;
}

#define  NTFSCMP_OK				0
#define  NTFSCMP_INODE_OPEN_ERROR		1
#define  NTFSCMP_INODE_OPEN_IO_ERROR		2
#define  NTFSCMP_INODE_OPEN_ENOENT_ERROR	3
#define  NTFSCMP_EXTENSION_RECORD		4
#define  NTFSCMP_INODE_CLOSE_ERROR		5

static const char *ntfscmp_errs[] = {
	"OK",
	"INODE_OPEN_ERROR",
	"INODE_OPEN_IO_ERROR",
	"INODE_OPEN_ENOENT_ERROR",
	"EXTENSION_RECORD",
	"INODE_CLOSE_ERROR",
	""
};


static const char *err2string(int err)
{
	return ntfscmp_errs[err];
}

static const char *pret2str(void *p)
{
	if (p == NULL)
		return "FAILED";
	return "OK";
}

static int inode_open(ntfs_volume *vol, MFT_REF mref, ntfs_inode **ni)
{
	*ni = ntfs_inode_open(vol, mref);
	if (*ni == NULL) {
		if (errno == EIO)
			return NTFSCMP_INODE_OPEN_IO_ERROR;
		if (errno == ENOENT)
			return NTFSCMP_INODE_OPEN_ENOENT_ERROR;

		perr_println("Reading inode %lld failed", mref);
		return NTFSCMP_INODE_OPEN_ERROR;
	}

	if ((*ni)->mrec->base_mft_record) {

		if (inode_close(*ni) != 0)
			return NTFSCMP_INODE_CLOSE_ERROR;

		return NTFSCMP_EXTENSION_RECORD;
	}

	return NTFSCMP_OK;
}

static ntfs_inode *base_inode(ntfs_attr_search_ctx *ctx)
{
	if (ctx->base_ntfs_ino)
		return ctx->base_ntfs_ino;

	return ctx->ntfs_ino;
}

static void print_inode(u64 inum)
{
	printf("Inode %llu ", inum);
}

static void print_inode_ni(ntfs_inode *ni)
{
	print_inode(inumber(ni));
}

static void print_attribute_type(ATTR_TYPES atype)
{
	printf("attribute 0x%x", atype);
}

static void print_attribute_name(char *name)
{
	if (name)
		printf(":%s", name);
}

#define	GET_ATTR_NAME(a) \
	((ntfschar *)(((u8 *)(a)) + le16_to_cpu((a)->name_offset))), \
	((a)->name_length)

static void free_name(char **name)
{
	if (*name) {
		free(*name);
		*name = NULL;
	}
}

static char *get_attr_name(u64 mft_no,
			   ATTR_TYPES atype,
			   const ntfschar *uname,
			   const int uname_len)
{
	char *name = NULL;
	int name_len;

	if (atype == AT_END)
		return NULL;

	name_len = ntfs_ucstombs(uname, uname_len, &name, 0);
	if (name_len < 0) {
		perr_print("ntfs_ucstombs");
		print_inode(mft_no);
		print_attribute_type(atype);
		puts("");
		exit(1);

	} else if (name_len > 0)
		return name;

	free_name(&name);
	return NULL;
}

static char *get_attr_name_na(ntfs_attr *na)
{
	return get_attr_name(inumber(na->ni), na->type, na->name, na->name_len);
}

static char *get_attr_name_ctx(ntfs_attr_search_ctx *ctx)
{
	u64 mft_no = inumber(ctx->ntfs_ino);
	ATTR_TYPES atype = ctx->attr->type;

	return get_attr_name(mft_no, atype, GET_ATTR_NAME(ctx->attr));
}

static void print_attribute(ATTR_TYPES atype, char *name)
{
	print_attribute_type(atype);
	print_attribute_name(name);
	printf(" ");
}

static void print_na(ntfs_attr *na)
{
	char *name = get_attr_name_na(na);
	print_inode_ni(na->ni);
	print_attribute(na->type, name);
	free_name(&name);
}

static void print_attribute_ctx(ntfs_attr_search_ctx *ctx)
{
	char *name = get_attr_name_ctx(ctx);
	print_attribute(ctx->attr->type, name);
	free_name(&name);
}

static void print_ctx(ntfs_attr_search_ctx *ctx)
{
	char *name = get_attr_name_ctx(ctx);
	print_inode_ni(base_inode(ctx));
	print_attribute(ctx->attr->type, name);
	free_name(&name);
}

static void print_differ(ntfs_attr *na)
{
	print_na(na);
	printf("content:   DIFFER\n");
}

static int cmp_buffer(u8 *buf1, u8 *buf2, long long int size, ntfs_attr *na)
{
	if (memcmp(buf1, buf2, size)) {
		print_differ(na);
		return -1;
	}
	return 0;
}

struct cmp_ia {
	INDEX_ALLOCATION *ia;
	INDEX_ALLOCATION *tmp_ia;
	u8 *bitmap;
	u8 *byte;
	s64 bm_size;
};

static int setup_cmp_ia(ntfs_attr *na, struct cmp_ia *cia)
{
	cia->bitmap = ntfs_attr_readall(na->ni, AT_BITMAP, na->name,
					na->name_len, &cia->bm_size);
	if (!cia->bitmap) {
		perr_println("Failed to readall BITMAP");
		return -1;
	}
	cia->byte = cia->bitmap;

	cia->tmp_ia = cia->ia = ntfs_malloc(na->data_size);
	if (!cia->tmp_ia)
		goto free_bm;

	if (ntfs_attr_pread(na, 0, na->data_size, cia->ia) != na->data_size) {
		perr_println("Failed to pread INDEX_ALLOCATION");
		goto free_ia;
	}

	return 0;
free_ia:
	free(cia->ia);
free_bm:
	free(cia->bitmap);
	return -1;
}

static void cmp_index_allocation(ntfs_attr *na1, ntfs_attr *na2)
{
	struct cmp_ia cia1, cia2;
	int bit, ret1, ret2;
	u32 ib_size;

	if (setup_cmp_ia(na1, &cia1))
		return;
	if (setup_cmp_ia(na2, &cia2))
		return;
	/*
	 *  FIXME: ia can be the same even if the bitmap sizes are different.
	 */
	if (cia1.bm_size != cia1.bm_size)
		goto out;

	if (cmp_buffer(cia1.bitmap, cia2.bitmap, cia1.bm_size, na1))
		goto out;

	if (cmp_buffer((u8 *)cia1.ia, (u8 *)cia2.ia, 0x18, na1))
		goto out;

	ib_size = le32_to_cpu(cia1.ia->index.allocated_size) + 0x18;

	bit = 0;
	while ((u8 *)cia1.tmp_ia < (u8 *)cia1.ia + na1->data_size) {
		if (*cia1.byte & (1 << bit)) {
			ret1 = ntfs_mst_post_read_fixup((NTFS_RECORD *)
					cia1.tmp_ia, ib_size);
			ret2 = ntfs_mst_post_read_fixup((NTFS_RECORD *)
					cia2.tmp_ia, ib_size);
			if (ret1 != ret2) {
				print_differ(na1);
				goto out;
			}

			if (ret1 == -1)
				continue;

			if (cmp_buffer(((u8 *)cia1.tmp_ia) + 0x18,
					((u8 *)cia2.tmp_ia) + 0x18,
					le32_to_cpu(cia1.ia->
					index.index_length), na1))
				goto out;
		}

		cia1.tmp_ia = (INDEX_ALLOCATION *)((u8 *)cia1.tmp_ia + ib_size);
		cia2.tmp_ia = (INDEX_ALLOCATION *)((u8 *)cia2.tmp_ia + ib_size);

		bit++;
		if (bit > 7) {
			bit = 0;
			cia1.byte++;
		}
	}
out:
	free(cia1.ia);
	free(cia2.ia);
	free(cia1.bitmap);
	free(cia2.bitmap);
	return;
}

static void cmp_attribute_data(ntfs_attr *na1, ntfs_attr *na2)
{
	s64 pos;
	s64 count1 = 0, count2;
	u8  buf1[NTFS_BUF_SIZE];
	u8  buf2[NTFS_BUF_SIZE];

	for (pos = 0; pos <= na1->data_size; pos += count1) {

		count1 = ntfs_attr_pread(na1, pos, NTFS_BUF_SIZE, buf1);
		count2 = ntfs_attr_pread(na2, pos, NTFS_BUF_SIZE, buf2);

		if (count1 != count2) {
			print_na(na1);
			printf("abrupt length:   %lld  !=  %lld ",
			       na1->data_size, na2->data_size);
			printf("(count: %lld  !=  %lld)", count1, count2);
			puts("");
			return;
		}

		if (count1 == -1) {
			err_printf("%s read error: ", __FUNCTION__);
			print_na(na1);
			printf("len = %lld, pos = %lld\n", na1->data_size, pos);
			exit(1);
		}

		if (count1 == 0) {

			if (pos + count1 == na1->data_size)
				return; /* we are ready */

			err_printf("%s read error before EOF: ", __FUNCTION__);
			print_na(na1);
			printf("%lld  !=  %lld\n", pos + count1, na1->data_size);
			exit(1);
		}

		if (cmp_buffer(buf1, buf2, count1, na1))
			return;
	}

	err_printf("%s read overrun: ", __FUNCTION__);
	print_na(na1);
	err_printf("(len = %lld, pos = %lld, count = %lld)\n",
		  na1->data_size, pos, count1);
	exit(1);
}

static int cmp_attribute_header(ATTR_RECORD *a1, ATTR_RECORD *a2)
{
	u32 header_size = offsetof(ATTR_RECORD, resident_end);

	if (a1->non_resident != a2->non_resident)
		return 1;

	if (a1->non_resident) {
		/*
		 * FIXME: includes paddings which are not handled by ntfsinfo!
		 */
		header_size = le32_to_cpu(a1->length);
	}

	return memcmp(a1, a2, header_size);
}

static void cmp_attribute(ntfs_attr_search_ctx *ctx1,
			  ntfs_attr_search_ctx *ctx2)
{
	ATTR_RECORD *a1 = ctx1->attr;
	ATTR_RECORD *a2 = ctx2->attr;
	ntfs_attr *na1, *na2;

	if (cmp_attribute_header(a1, a2)) {
		print_ctx(ctx1);
		printf("header:    DIFFER\n");
	}

	na1 = ntfs_attr_open(base_inode(ctx1), a1->type, GET_ATTR_NAME(a1));
	na2 = ntfs_attr_open(base_inode(ctx2), a2->type, GET_ATTR_NAME(a2));

	if ((!na1 && na2) || (na1 && !na2)) {
		print_ctx(ctx1);
		printf("open:   %s  !=  %s\n", pret2str(na1), pret2str(na2));
		goto close_attribs;
	}

	if (na1 == NULL)
		goto close_attribs;

	if (na1->data_size != na2->data_size) {
		print_na(na1);
		printf("length:   %lld  !=  %lld\n", na1->data_size, na2->data_size);
		goto close_attribs;
	}

	if (ntfs_inode_badclus_bad(inumber(ctx1->ntfs_ino), ctx1->attr) == 1) {
		/*
		 * If difference exists then it's already reported at the
		 * attribute header since the mapping pairs must differ.
		 */
		return;
	}

	if (na1->type == AT_INDEX_ALLOCATION)
		cmp_index_allocation(na1, na2);
	else
		cmp_attribute_data(na1, na2);

close_attribs:
	ntfs_attr_close(na1);
	ntfs_attr_close(na2);
}

static void vprint_attribute(ATTR_TYPES atype, char  *name)
{
	if (!opt.verbose)
		return;

	printf("0x%x", atype);
	if (name)
		printf(":%s", name);
	printf(" ");
}

static void print_attributes(ntfs_inode *ni,
			     ATTR_TYPES atype1,
			     ATTR_TYPES atype2,
			     char  *name1,
			     char  *name2)
{
	if (!opt.verbose)
		return;

	printf("Walking inode %llu attributes: ", inumber(ni));
	vprint_attribute(atype1, name1);
	vprint_attribute(atype2, name2);
	printf("\n");
}

static int new_name(ntfs_attr_search_ctx *ctx, char *prev_name)
{
	int ret = 0;
	char *name = get_attr_name_ctx(ctx);

	if (prev_name && name) {
		if (strcmp(prev_name, name) != 0)
			ret = 1;
	} else if (prev_name || name)
		ret = 1;

	free_name(&name);
	return ret;

}

static int new_attribute(ntfs_attr_search_ctx *ctx,
			 ATTR_TYPES prev_atype,
			 char *prev_name)
{
	if (!prev_atype && !prev_name)
		return 1;

	if (!ctx->attr->non_resident)
		return 1;

	if (prev_atype != ctx->attr->type)
		return 1;

	if (new_name(ctx, prev_name))
		return 1;

	if (opt.verbose) {
		print_inode(base_inode(ctx)->mft_no);
		print_attribute_ctx(ctx);
		printf("record %llu lowest_vcn %lld:    SKIPPED\n",
			ctx->ntfs_ino->mft_no, ctx->attr->lowest_vcn);
	}

	return 0;
}

static void set_prev(char **prev_name, ATTR_TYPES *prev_atype,
		     char *name, ATTR_TYPES atype)
{
	free_name(prev_name);
	if (name) {
		*prev_name = strdup(name);
		if (!*prev_name)
			perr_exit("strdup error");
	}

	*prev_atype = atype;
}

static void set_cmp_attr(ntfs_attr_search_ctx *ctx, ATTR_TYPES *atype, char **name)
{
	*atype = ctx->attr->type;

	free_name(name);
	*name = get_attr_name_ctx(ctx);
}

static int next_attr(ntfs_attr_search_ctx *ctx, ATTR_TYPES *atype, char **name,
		     int *err)
{
	int ret;

	ret = ntfs_attrs_walk(ctx);
	*err = errno;
	if (ret) {
		*atype = AT_END;
		free_name(name);
	} else
		set_cmp_attr(ctx, atype, name);

	return ret;
}

static int cmp_attributes(ntfs_inode *ni1, ntfs_inode *ni2)
{
	int ret = -1;
	int old_ret1, ret1 = 0, ret2 = 0;
	int errno1 = 0, errno2 = 0;
	char  *prev_name = NULL, *name1 = NULL, *name2 = NULL;
	ATTR_TYPES old_atype1, prev_atype = 0, atype1, atype2;
	ntfs_attr_search_ctx *ctx1, *ctx2;

	if (!(ctx1 = attr_get_search_ctx(ni1)))
		return -1;
	if (!(ctx2 = attr_get_search_ctx(ni2)))
		goto out;

	set_cmp_attr(ctx1, &atype1, &name1);
	set_cmp_attr(ctx2, &atype2, &name2);

	while (1) {

		old_atype1 = atype1;
		old_ret1 = ret1;
		if (!ret1 && (le32_to_cpu(atype1) <= le32_to_cpu(atype2) ||
				ret2))
			ret1 = next_attr(ctx1, &atype1, &name1, &errno1);
		if (!ret2 && (le32_to_cpu(old_atype1) >= le32_to_cpu(atype2) ||
					old_ret1))
			ret2 = next_attr(ctx2, &atype2, &name2, &errno2);

		print_attributes(ni1, atype1, atype2, name1, name2);

		if (ret1 && ret2) {
			if (errno1 != errno2) {
				print_inode_ni(ni1);
				printf("attribute walk (errno):   %d  !=  %d\n",
				       errno1, errno2);
			}
			break;
		}

		if (ret2 || le32_to_cpu(atype1) < le32_to_cpu(atype2)) {
			if (new_attribute(ctx1, prev_atype, prev_name)) {
				print_ctx(ctx1);
				printf("presence:   EXISTS   !=   MISSING\n");
				set_prev(&prev_name, &prev_atype, name1,
						atype1);
			}

		} else if (ret1 || le32_to_cpu(atype1) > le32_to_cpu(atype2)) {
			if (new_attribute(ctx2, prev_atype, prev_name)) {
				print_ctx(ctx2);
				printf("presence:   MISSING  !=  EXISTS \n");
				set_prev(&prev_name, &prev_atype, name2, atype2);
			}

		} else /* atype1 == atype2 */ {
			if (new_attribute(ctx1, prev_atype, prev_name)) {
				cmp_attribute(ctx1, ctx2);
				set_prev(&prev_name, &prev_atype, name1, atype1);
			}
		}
	}

	free_name(&prev_name);
	ret = 0;
	ntfs_attr_put_search_ctx(ctx2);
out:
	ntfs_attr_put_search_ctx(ctx1);
	return ret;
}

static int cmp_inodes(ntfs_volume *vol1, ntfs_volume *vol2)
{
	u64 inode;
	int ret1, ret2;
	ntfs_inode *ni1, *ni2;
	struct progress_bar progress;
	int pb_flags = 0;	/* progress bar flags */
	u64 nr_mft_records, nr_mft_records2;

	if (opt.show_progress)
		pb_flags |= NTFS_PROGBAR;

	nr_mft_records  = get_nr_mft_records(vol1);
	nr_mft_records2 = get_nr_mft_records(vol2);

	if (nr_mft_records != nr_mft_records2) {

		printf("Number of mft records:   %lld  !=  %lld\n",
		       nr_mft_records, nr_mft_records2);

		if (nr_mft_records > nr_mft_records2)
			nr_mft_records = nr_mft_records2;
	}

	progress_init(&progress, 0, nr_mft_records - 1, pb_flags);
	progress_update(&progress, 0);

	for (inode = 0; inode < nr_mft_records; inode++) {

		ret1 = inode_open(vol1, (MFT_REF)inode, &ni1);
		ret2 = inode_open(vol2, (MFT_REF)inode, &ni2);

		if (ret1 != ret2) {
			print_inode(inode);
			printf("open:   %s  !=  %s\n",
			       err2string(ret1), err2string(ret2));
			goto close_inodes;
		}

		if (ret1 != NTFSCMP_OK)
			goto close_inodes;

		if (cmp_attributes(ni1, ni2) != 0) {
			inode_close(ni1);
			inode_close(ni2);
			return -1;
		}
close_inodes:
		if (inode_close(ni1) != 0)
			return -1;
		if (inode_close(ni2) != 0)
			return -1;

		progress_update(&progress, inode);
	}
	return 0;
}

static ntfs_volume *mount_volume(const char *volume)
{
	unsigned long mntflag;
	ntfs_volume *vol = NULL;

	if (ntfs_check_if_mounted(volume, &mntflag)) {
		perr_println("Failed to check '%s' mount state", volume);
		printf("Probably /etc/mtab is missing. It's too risky to "
		       "continue. You might try\nan another Linux distro.\n");
		exit(1);
	}
	if (mntflag & NTFS_MF_MOUNTED) {
		if (!(mntflag & NTFS_MF_READONLY))
			err_exit("Device '%s' is mounted read-write. "
				 "You must 'umount' it first.\n", volume);
	}

	vol = ntfs_mount(volume, NTFS_MNT_RDONLY);
	if (vol == NULL) {

		int err = errno;

		perr_println("Opening '%s' as NTFS failed", volume);
		if (err == EINVAL)
			printf(invalid_ntfs_msg, volume);
		else if (err == EIO)
			puts(corrupt_volume_msg);
		else if (err == EPERM)
			puts(hibernated_volume_msg);
		exit(1);
	}

	return vol;
}

int main(int argc, char **argv)
{
	ntfs_volume *vol1;
	ntfs_volume *vol2;

	printf("%s v%s (libntfs %s)\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());

	parse_options(argc, argv);

	utils_set_locale();

	vol1 = mount_volume(opt.vol1);
        vol2 = mount_volume(opt.vol2);

	if (cmp_inodes(vol1, vol2) != 0)
		exit(1);

	ntfs_umount(vol1, FALSE);
	ntfs_umount(vol2, FALSE);

	exit(0);
}

