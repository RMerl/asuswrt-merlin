/* vi: set sw=4 ts=4: */
/*
 * Mini unzip implementation for busybox
 *
 * Copyright (C) 2004 by Ed Clark
 *
 * Loosely based on original busybox unzip applet by Laurence Anderson.
 * All options and features should work in this version.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
/* For reference see
 * http://www.pkware.com/company/standards/appnote/
 * http://www.info-zip.org/pub/infozip/doc/appnote-iz-latest.zip
 *
 * TODO
 * Zip64 + other methods
 */

//config:config UNZIP
//config:	bool "unzip"
//config:	default y
//config:	help
//config:	  unzip will list or extract files from a ZIP archive,
//config:	  commonly found on DOS/WIN systems. The default behavior
//config:	  (with no options) is to extract the archive into the
//config:	  current directory. Use the `-d' option to extract to a
//config:	  directory of your choice.

//applet:IF_UNZIP(APPLET(unzip, BB_DIR_USR_BIN, BB_SUID_DROP))
//kbuild:lib-$(CONFIG_UNZIP) += unzip.o

//usage:#define unzip_trivial_usage
//usage:       "[-lnopq] FILE[.zip] [FILE]... [-x FILE...] [-d DIR]"
//usage:#define unzip_full_usage "\n\n"
//usage:       "Extract FILEs from ZIP archive\n"
//usage:     "\n	-l	List contents (with -q for short form)"
//usage:     "\n	-n	Never overwrite files (default: ask)"
//usage:     "\n	-o	Overwrite"
//usage:     "\n	-p	Print to stdout"
//usage:     "\n	-q	Quiet"
//usage:     "\n	-x FILE	Exclude FILEs"
//usage:     "\n	-d DIR	Extract into DIR"

#include "libbb.h"
#include "bb_archive.h"

#if 0
# define dbg(...) bb_error_msg(__VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

enum {
#if BB_BIG_ENDIAN
	ZIP_FILEHEADER_MAGIC = 0x504b0304,
	ZIP_CDF_MAGIC        = 0x504b0102, /* central directory's file header */
	ZIP_CDE_MAGIC        = 0x504b0506, /* "end of central directory" record */
	ZIP_DD_MAGIC         = 0x504b0708,
#else
	ZIP_FILEHEADER_MAGIC = 0x04034b50,
	ZIP_CDF_MAGIC        = 0x02014b50,
	ZIP_CDE_MAGIC        = 0x06054b50,
	ZIP_DD_MAGIC         = 0x08074b50,
#endif
};

#define ZIP_HEADER_LEN 26

typedef union {
	uint8_t raw[ZIP_HEADER_LEN];
	struct {
		uint16_t version;               /* 0-1 */
		uint16_t zip_flags;             /* 2-3 */
		uint16_t method;                /* 4-5 */
		uint16_t modtime;               /* 6-7 */
		uint16_t moddate;               /* 8-9 */
		uint32_t crc32 PACKED;          /* 10-13 */
		uint32_t cmpsize PACKED;        /* 14-17 */
		uint32_t ucmpsize PACKED;       /* 18-21 */
		uint16_t filename_len;          /* 22-23 */
		uint16_t extra_len;             /* 24-25 */
	} formatted PACKED;
} zip_header_t; /* PACKED - gcc 4.2.1 doesn't like it (spews warning) */

/* Check the offset of the last element, not the length.  This leniency
 * allows for poor packing, whereby the overall struct may be too long,
 * even though the elements are all in the right place.
 */
struct BUG_zip_header_must_be_26_bytes {
	char BUG_zip_header_must_be_26_bytes[
		offsetof(zip_header_t, formatted.extra_len) + 2
			== ZIP_HEADER_LEN ? 1 : -1];
};

#define FIX_ENDIANNESS_ZIP(zip_header) do { \
	(zip_header).formatted.version      = SWAP_LE16((zip_header).formatted.version     ); \
	(zip_header).formatted.method       = SWAP_LE16((zip_header).formatted.method      ); \
	(zip_header).formatted.modtime      = SWAP_LE16((zip_header).formatted.modtime     ); \
	(zip_header).formatted.moddate      = SWAP_LE16((zip_header).formatted.moddate     ); \
	(zip_header).formatted.crc32        = SWAP_LE32((zip_header).formatted.crc32       ); \
	(zip_header).formatted.cmpsize      = SWAP_LE32((zip_header).formatted.cmpsize     ); \
	(zip_header).formatted.ucmpsize     = SWAP_LE32((zip_header).formatted.ucmpsize    ); \
	(zip_header).formatted.filename_len = SWAP_LE16((zip_header).formatted.filename_len); \
	(zip_header).formatted.extra_len    = SWAP_LE16((zip_header).formatted.extra_len   ); \
} while (0)

#define CDF_HEADER_LEN 42

typedef union {
	uint8_t raw[CDF_HEADER_LEN];
	struct {
		/* uint32_t signature; 50 4b 01 02 */
		uint16_t version_made_by;       /* 0-1 */
		uint16_t version_needed;        /* 2-3 */
		uint16_t cdf_flags;             /* 4-5 */
		uint16_t method;                /* 6-7 */
		uint16_t mtime;                 /* 8-9 */
		uint16_t mdate;                 /* 10-11 */
		uint32_t crc32;                 /* 12-15 */
		uint32_t cmpsize;               /* 16-19 */
		uint32_t ucmpsize;              /* 20-23 */
		uint16_t file_name_length;      /* 24-25 */
		uint16_t extra_field_length;    /* 26-27 */
		uint16_t file_comment_length;   /* 28-29 */
		uint16_t disk_number_start;     /* 30-31 */
		uint16_t internal_file_attributes; /* 32-33 */
		uint32_t external_file_attributes PACKED; /* 34-37 */
		uint32_t relative_offset_of_local_header PACKED; /* 38-41 */
	} formatted PACKED;
} cdf_header_t;

struct BUG_cdf_header_must_be_42_bytes {
	char BUG_cdf_header_must_be_42_bytes[
		offsetof(cdf_header_t, formatted.relative_offset_of_local_header) + 4
			== CDF_HEADER_LEN ? 1 : -1];
};

#define FIX_ENDIANNESS_CDF(cdf_header) do { \
	(cdf_header).formatted.crc32        = SWAP_LE32((cdf_header).formatted.crc32       ); \
	(cdf_header).formatted.cmpsize      = SWAP_LE32((cdf_header).formatted.cmpsize     ); \
	(cdf_header).formatted.ucmpsize     = SWAP_LE32((cdf_header).formatted.ucmpsize    ); \
	(cdf_header).formatted.file_name_length = SWAP_LE16((cdf_header).formatted.file_name_length); \
	(cdf_header).formatted.extra_field_length = SWAP_LE16((cdf_header).formatted.extra_field_length); \
	(cdf_header).formatted.file_comment_length = SWAP_LE16((cdf_header).formatted.file_comment_length); \
	IF_DESKTOP( \
	(cdf_header).formatted.version_made_by = SWAP_LE16((cdf_header).formatted.version_made_by); \
	(cdf_header).formatted.external_file_attributes = SWAP_LE32((cdf_header).formatted.external_file_attributes); \
	) \
} while (0)

#define CDE_HEADER_LEN 16

typedef union {
	uint8_t raw[CDE_HEADER_LEN];
	struct {
		/* uint32_t signature; 50 4b 05 06 */
		uint16_t this_disk_no;
		uint16_t disk_with_cdf_no;
		uint16_t cdf_entries_on_this_disk;
		uint16_t cdf_entries_total;
		uint32_t cdf_size;
		uint32_t cdf_offset;
		/* uint16_t file_comment_length; */
		/* .ZIP file comment (variable size) */
	} formatted PACKED;
} cde_header_t;

struct BUG_cde_header_must_be_16_bytes {
	char BUG_cde_header_must_be_16_bytes[
		sizeof(cde_header_t) == CDE_HEADER_LEN ? 1 : -1];
};

#define FIX_ENDIANNESS_CDE(cde_header) do { \
	(cde_header).formatted.cdf_offset = SWAP_LE32((cde_header).formatted.cdf_offset); \
} while (0)

enum { zip_fd = 3 };


#if ENABLE_DESKTOP

/* Seen in the wild:
 * Self-extracting PRO2K3XP_32.exe contains 19078464 byte zip archive,
 * where CDE was nearly 48 kbytes before EOF.
 * (Surprisingly, it also apparently has *another* CDE structure
 * closer to the end, with bogus cdf_offset).
 * To make extraction work, bumped PEEK_FROM_END from 16k to 64k.
 */
#define PEEK_FROM_END (64*1024)

/* This value means that we failed to find CDF */
#define BAD_CDF_OFFSET ((uint32_t)0xffffffff)

/* NB: does not preserve file position! */
static uint32_t find_cdf_offset(void)
{
	cde_header_t cde_header;
	unsigned char *p;
	off_t end;
	unsigned char *buf = xzalloc(PEEK_FROM_END);
	uint32_t found;

	end = xlseek(zip_fd, 0, SEEK_END);
	end -= PEEK_FROM_END;
	if (end < 0)
		end = 0;
	dbg("Looking for cdf_offset starting from 0x%"OFF_FMT"x", end);
 	xlseek(zip_fd, end, SEEK_SET);
	full_read(zip_fd, buf, PEEK_FROM_END);

	found = BAD_CDF_OFFSET;
	p = buf;
	while (p <= buf + PEEK_FROM_END - CDE_HEADER_LEN - 4) {
		if (*p != 'P') {
			p++;
			continue;
		}
		if (*++p != 'K')
			continue;
		if (*++p != 5)
			continue;
		if (*++p != 6)
			continue;
		/* we found CDE! */
		memcpy(cde_header.raw, p + 1, CDE_HEADER_LEN);
		FIX_ENDIANNESS_CDE(cde_header);
		/*
		 * I've seen .ZIP files with seemingly valid CDEs
		 * where cdf_offset points past EOF - ??
		 * This check ignores such CDEs:
		 */
		if (cde_header.formatted.cdf_offset < end + (p - buf)) {
			found = cde_header.formatted.cdf_offset;
			dbg("Possible cdf_offset:0x%x at 0x%"OFF_FMT"x",
				(unsigned)found, end + (p-3 - buf));
			dbg("  cdf_offset+cdf_size:0x%x",
				(unsigned)(found + SWAP_LE32(cde_header.formatted.cdf_size)));
			/*
			 * We do not "break" here because only the last CDE is valid.
			 * I've seen a .zip archive which contained a .zip file,
			 * uncompressed, and taking the first CDE was using
			 * the CDE inside that file!
			 */
		}
	}
	free(buf);
	dbg("Found cdf_offset:0x%x", (unsigned)found);
	return found;
};

static uint32_t read_next_cdf(uint32_t cdf_offset, cdf_header_t *cdf_ptr)
{
	off_t org;

	org = xlseek(zip_fd, 0, SEEK_CUR);

	if (!cdf_offset)
		cdf_offset = find_cdf_offset();

	if (cdf_offset != BAD_CDF_OFFSET) {
		dbg("Reading CDF at 0x%x", (unsigned)cdf_offset);
		xlseek(zip_fd, cdf_offset + 4, SEEK_SET);
		xread(zip_fd, cdf_ptr->raw, CDF_HEADER_LEN);
		FIX_ENDIANNESS_CDF(*cdf_ptr);
		dbg("  file_name_length:%u extra_field_length:%u file_comment_length:%u",
			(unsigned)cdf_ptr->formatted.file_name_length,
			(unsigned)cdf_ptr->formatted.extra_field_length,
			(unsigned)cdf_ptr->formatted.file_comment_length
		);
		cdf_offset += 4 + CDF_HEADER_LEN
			+ cdf_ptr->formatted.file_name_length
			+ cdf_ptr->formatted.extra_field_length
			+ cdf_ptr->formatted.file_comment_length;
	}

	dbg("Returning file position to 0x%"OFF_FMT"x", org);
	xlseek(zip_fd, org, SEEK_SET);
	return cdf_offset;
};
#endif

static void unzip_skip(off_t skip)
{
	if (skip != 0)
		if (lseek(zip_fd, skip, SEEK_CUR) == (off_t)-1)
			bb_copyfd_exact_size(zip_fd, -1, skip);
}

static void unzip_create_leading_dirs(const char *fn)
{
	/* Create all leading directories */
	char *name = xstrdup(fn);
	if (bb_make_directory(dirname(name), 0777, FILEUTILS_RECUR)) {
		xfunc_die(); /* bb_make_directory is noisy */
	}
	free(name);
}

static void unzip_extract(zip_header_t *zip_header, int dst_fd)
{
	if (zip_header->formatted.method == 0) {
		/* Method 0 - stored (not compressed) */
		off_t size = zip_header->formatted.ucmpsize;
		if (size)
			bb_copyfd_exact_size(zip_fd, dst_fd, size);
	} else {
		/* Method 8 - inflate */
		transformer_state_t xstate;
		init_transformer_state(&xstate);
		xstate.bytes_in = zip_header->formatted.cmpsize;
		xstate.src_fd = zip_fd;
		xstate.dst_fd = dst_fd;
		if (inflate_unzip(&xstate) < 0)
			bb_error_msg_and_die("inflate error");
		/* Validate decompression - crc */
		if (zip_header->formatted.crc32 != (xstate.crc32 ^ 0xffffffffL)) {
			bb_error_msg_and_die("crc error");
		}
		/* Validate decompression - size */
		if (zip_header->formatted.ucmpsize != xstate.bytes_out) {
			/* Don't die. Who knows, maybe len calculation
			 * was botched somewhere. After all, crc matched! */
			bb_error_msg("bad length");
		}
	}
}

static void my_fgets80(char *buf80)
{
	fflush_all();
	if (!fgets(buf80, 80, stdin)) {
		bb_perror_msg_and_die("can't read standard input");
	}
}

int unzip_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int unzip_main(int argc, char **argv)
{
	enum { O_PROMPT, O_NEVER, O_ALWAYS };

	zip_header_t zip_header;
	smallint quiet = 0;
	IF_NOT_DESKTOP(const) smallint verbose = 0;
	smallint listing = 0;
	smallint overwrite = O_PROMPT;
	smallint x_opt_seen;
#if ENABLE_DESKTOP
	uint32_t cdf_offset;
#endif
	unsigned long total_usize;
	unsigned long total_size;
	unsigned total_entries;
	int dst_fd = -1;
	char *src_fn = NULL;
	char *dst_fn = NULL;
	llist_t *zaccept = NULL;
	llist_t *zreject = NULL;
	char *base_dir = NULL;
	int i, opt;
	char key_buf[80]; /* must match size used by my_fgets80 */
	struct stat stat_buf;

/* -q, -l and -v: UnZip 5.52 of 28 February 2005, by Info-ZIP:
 *
 * # /usr/bin/unzip -qq -v decompress_unlzma.i.zip
 *   204372  Defl:N    35278  83%  09-06-09 14:23  0d056252  decompress_unlzma.i
 * # /usr/bin/unzip -q -v decompress_unlzma.i.zip
 *  Length   Method    Size  Ratio   Date   Time   CRC-32    Name
 * --------  ------  ------- -----   ----   ----   ------    ----
 *   204372  Defl:N    35278  83%  09-06-09 14:23  0d056252  decompress_unlzma.i
 * --------          -------  ---                            -------
 *   204372            35278  83%                            1 file
 * # /usr/bin/unzip -v decompress_unlzma.i.zip
 * Archive:  decompress_unlzma.i.zip
 *  Length   Method    Size  Ratio   Date   Time   CRC-32    Name
 * --------  ------  ------- -----   ----   ----   ------    ----
 *   204372  Defl:N    35278  83%  09-06-09 14:23  0d056252  decompress_unlzma.i
 * --------          -------  ---                            -------
 *   204372            35278  83%                            1 file
 * # unzip -v decompress_unlzma.i.zip
 * Archive:  decompress_unlzma.i.zip
 *   Length     Date   Time    Name
 *  --------    ----   ----    ----
 *    204372  09-06-09 14:23   decompress_unlzma.i
 *  --------                   -------
 *    204372                   1 files
 * # /usr/bin/unzip -l -qq decompress_unlzma.i.zip
 *    204372  09-06-09 14:23   decompress_unlzma.i
 * # /usr/bin/unzip -l -q decompress_unlzma.i.zip
 *   Length     Date   Time    Name
 *  --------    ----   ----    ----
 *    204372  09-06-09 14:23   decompress_unlzma.i
 *  --------                   -------
 *    204372                   1 file
 * # /usr/bin/unzip -l decompress_unlzma.i.zip
 * Archive:  decompress_unlzma.i.zip
 *   Length     Date   Time    Name
 *  --------    ----   ----    ----
 *    204372  09-06-09 14:23   decompress_unlzma.i
 *  --------                   -------
 *    204372                   1 file
 */

	x_opt_seen = 0;
	/* '-' makes getopt return 1 for non-options */
	while ((opt = getopt(argc, argv, "-d:lnopqxv")) != -1) {
		switch (opt) {
		case 'd':  /* Extract to base directory */
			base_dir = optarg;
			break;

		case 'l': /* List */
			listing = 1;
			break;

		case 'n': /* Never overwrite existing files */
			overwrite = O_NEVER;
			break;

		case 'o': /* Always overwrite existing files */
			overwrite = O_ALWAYS;
			break;

		case 'p': /* Extract files to stdout and fall through to set verbosity */
			dst_fd = STDOUT_FILENO;

		case 'q': /* Be quiet */
			quiet++;
			break;

		case 'v': /* Verbose list */
			IF_DESKTOP(verbose++;)
			listing = 1;
			break;

		case 'x':
			x_opt_seen = 1;
			break;

		case 1:
			if (!src_fn) {
				/* The zip file */
				/* +5: space for ".zip" and NUL */
				src_fn = xmalloc(strlen(optarg) + 5);
				strcpy(src_fn, optarg);
			} else if (!x_opt_seen) {
				/* Include files */
				llist_add_to(&zaccept, optarg);
			} else {
				/* Exclude files */
				llist_add_to(&zreject, optarg);
			}
			break;

		default:
			bb_show_usage();
		}
	}

#ifndef __GLIBC__
	/*
	 * This code is needed for non-GNU getopt
	 * which doesn't understand "-" in option string.
	 * The -x option won't work properly in this case:
	 * "unzip a.zip q -x w e" will be interpreted as
	 * "unzip a.zip q w e -x" = "unzip a.zip q w e"
	 */
	argv += optind;
	if (argv[0]) {
		/* +5: space for ".zip" and NUL */
		src_fn = xmalloc(strlen(argv[0]) + 5);
		strcpy(src_fn, argv[0]);
		while (*++argv)
			llist_add_to(&zaccept, *argv);
	}
#endif

	if (!src_fn) {
		bb_show_usage();
	}

	/* Open input file */
	if (LONE_DASH(src_fn)) {
		xdup2(STDIN_FILENO, zip_fd);
		/* Cannot use prompt mode since zip data is arriving on STDIN */
		if (overwrite == O_PROMPT)
			overwrite = O_NEVER;
	} else {
		static const char extn[][5] ALIGN1 = { ".zip", ".ZIP" };
		char *ext = src_fn + strlen(src_fn);
		int src_fd;

		i = 0;
		for (;;) {
			src_fd = open(src_fn, O_RDONLY);
			if (src_fd >= 0)
				break;
			if (++i > 2) {
				*ext = '\0';
				bb_error_msg_and_die("can't open %s[.zip]", src_fn);
			}
			strcpy(ext, extn[i - 1]);
		}
		xmove_fd(src_fd, zip_fd);
	}

	/* Change dir if necessary */
	if (base_dir)
		xchdir(base_dir);

	if (quiet <= 1) { /* not -qq */
		if (quiet == 0)
			printf("Archive:  %s\n", src_fn);
		if (listing) {
			puts(verbose ?
				" Length   Method    Size  Cmpr    Date    Time   CRC-32   Name\n"
				"--------  ------  ------- ---- ---------- ----- --------  ----"
				:
				"  Length      Date    Time    Name\n"
				"---------  ---------- -----   ----"
				);
		}
	}

/* Example of an archive with one 0-byte long file named 'z'
 * created by Zip 2.31 on Unix:
 * 0000 [50 4b]03 04 0a 00 00 00 00 00 42 1a b8 3c 00 00 |PK........B..<..|
 *       sig........ vneed flags compr mtime mdate crc32>
 * 0010  00 00 00 00 00 00 00 00 00 00 01 00 15 00 7a 55 |..............zU|
 *      >..... csize...... usize...... fnlen exlen fn ex>
 * 0020  54 09 00 03 cc d3 f9 4b cc d3 f9 4b 55 78 04 00 |T......K...KUx..|
 *      >tra_field......................................
 * 0030  00 00 00 00[50 4b]01 02 17 03 0a 00 00 00 00 00 |....PK..........|
 *       ........... sig........ vmade vneed flags compr
 * 0040  42 1a b8 3c 00 00 00 00 00 00 00 00 00 00 00 00 |B..<............|
 *       mtime mdate crc32...... csize...... usize......
 * 0050  01 00 0d 00 00 00 00 00 00 00 00 00 a4 81 00 00 |................|
 *       fnlen exlen clen. dnum. iattr eattr...... relofs> (eattr = rw-r--r--)
 * 0060  00 00 7a 55 54 05 00 03 cc d3 f9 4b 55 78 00 00 |..zUT......KUx..|
 *      >..... fn extra_field...........................
 * 0070 [50 4b]05 06 00 00 00 00 01 00 01 00 3c 00 00 00 |PK..........<...|
 * 0080  34 00 00 00 00 00                               |4.....|
 */
	total_usize = 0;
	total_size = 0;
	total_entries = 0;
#if ENABLE_DESKTOP
	cdf_offset = 0;
#endif
	while (1) {
		uint32_t magic;
		mode_t dir_mode = 0777;
#if ENABLE_DESKTOP
		mode_t file_mode = 0666;
#endif

		/* Check magic number */
		xread(zip_fd, &magic, 4);
		/* Central directory? It's at the end, so exit */
		if (magic == ZIP_CDF_MAGIC) {
			dbg("got ZIP_CDF_MAGIC");
			break;
		}
#if ENABLE_DESKTOP
		/* Data descriptor? It was a streaming file, go on */
		if (magic == ZIP_DD_MAGIC) {
			dbg("got ZIP_DD_MAGIC");
			/* skip over duplicate crc32, cmpsize and ucmpsize */
			unzip_skip(3 * 4);
			continue;
		}
#endif
		if (magic != ZIP_FILEHEADER_MAGIC)
			bb_error_msg_and_die("invalid zip magic %08X", (int)magic);
		dbg("got ZIP_FILEHEADER_MAGIC");

		/* Read the file header */
		xread(zip_fd, zip_header.raw, ZIP_HEADER_LEN);
		FIX_ENDIANNESS_ZIP(zip_header);
		if ((zip_header.formatted.method != 0) && (zip_header.formatted.method != 8)) {
			bb_error_msg_and_die("unsupported method %d", zip_header.formatted.method);
		}
#if !ENABLE_DESKTOP
		if (zip_header.formatted.zip_flags & SWAP_LE16(0x0009)) {
			bb_error_msg_and_die("zip flags 1 and 8 are not supported");
		}
#else
		if (zip_header.formatted.zip_flags & SWAP_LE16(0x0001)) {
			/* 0x0001 - encrypted */
			bb_error_msg_and_die("zip flag 1 (encryption) is not supported");
		}

		if (cdf_offset != BAD_CDF_OFFSET) {
			cdf_header_t cdf_header;
			cdf_offset = read_next_cdf(cdf_offset, &cdf_header);
			/*
			 * Note: cdf_offset can become BAD_CDF_OFFSET after the above call.
			 */
			if (zip_header.formatted.zip_flags & SWAP_LE16(0x0008)) {
				/* 0x0008 - streaming. [u]cmpsize can be reliably gotten
				 * only from Central Directory. See unzip_doc.txt
				 */
				zip_header.formatted.crc32    = cdf_header.formatted.crc32;
				zip_header.formatted.cmpsize  = cdf_header.formatted.cmpsize;
				zip_header.formatted.ucmpsize = cdf_header.formatted.ucmpsize;
			}
			if ((cdf_header.formatted.version_made_by >> 8) == 3) {
				/* This archive is created on Unix */
				dir_mode = file_mode = (cdf_header.formatted.external_file_attributes >> 16);
			}
		}
		if (cdf_offset == BAD_CDF_OFFSET
		 && (zip_header.formatted.zip_flags & SWAP_LE16(0x0008))
		) {
			/* If it's a streaming zip, we _require_ CDF */
			bb_error_msg_and_die("can't find file table");
		}
#endif
		dbg("File cmpsize:0x%x extra_len:0x%x ucmpsize:0x%x",
			(unsigned)zip_header.formatted.cmpsize,
			(unsigned)zip_header.formatted.extra_len,
			(unsigned)zip_header.formatted.ucmpsize
		);

		/* Read filename */
		free(dst_fn);
		dst_fn = xzalloc(zip_header.formatted.filename_len + 1);
		xread(zip_fd, dst_fn, zip_header.formatted.filename_len);

		/* Skip extra header bytes */
		unzip_skip(zip_header.formatted.extra_len);

		/* Guard against "/abspath", "/../" and similar attacks */
		overlapping_strcpy(dst_fn, strip_unsafe_prefix(dst_fn));

		/* Filter zip entries */
		if (find_list_entry(zreject, dst_fn)
		 || (zaccept && !find_list_entry(zaccept, dst_fn))
		) { /* Skip entry */
			i = 'n';
		} else {
			if (listing) {
				/* List entry */
				char dtbuf[sizeof("mm-dd-yyyy hh:mm")];
				sprintf(dtbuf, "%02u-%02u-%04u %02u:%02u",
					(zip_header.formatted.moddate >> 5) & 0xf,  // mm: 0x01e0
					(zip_header.formatted.moddate)      & 0x1f, // dd: 0x001f
					(zip_header.formatted.moddate >> 9) + 1980, // yy: 0xfe00
					(zip_header.formatted.modtime >> 11),       // hh: 0xf800
					(zip_header.formatted.modtime >> 5) & 0x3f  // mm: 0x07e0
					// seconds/2 are not shown, encoded in ----------- 0x001f
				);
				if (!verbose) {
					//      "  Length      Date    Time    Name\n"
					//      "---------  ---------- -----   ----"
					printf(       "%9u  " "%s   "         "%s\n",
						(unsigned)zip_header.formatted.ucmpsize,
						dtbuf,
						dst_fn);
				} else {
					unsigned long percents = zip_header.formatted.ucmpsize - zip_header.formatted.cmpsize;
					if ((int32_t)percents < 0)
						percents = 0; /* happens if ucmpsize < cmpsize */
					percents = percents * 100;
					if (zip_header.formatted.ucmpsize)
						percents /= zip_header.formatted.ucmpsize;
					//      " Length   Method    Size  Cmpr    Date    Time   CRC-32   Name\n"
					//      "--------  ------  ------- ---- ---------- ----- --------  ----"
					printf(      "%8u  %s"        "%9u%4u%% " "%s "         "%08x  "  "%s\n",
						(unsigned)zip_header.formatted.ucmpsize,
						zip_header.formatted.method == 0 ? "Stored" : "Defl:N", /* Defl is method 8 */
/* TODO: show other methods?
 *  1 - Shrunk
 *  2 - Reduced with compression factor 1
 *  3 - Reduced with compression factor 2
 *  4 - Reduced with compression factor 3
 *  5 - Reduced with compression factor 4
 *  6 - Imploded
 *  7 - Reserved for Tokenizing compression algorithm
 *  9 - Deflate64
 * 10 - PKWARE Data Compression Library Imploding
 * 11 - Reserved by PKWARE
 * 12 - BZIP2
 */
						(unsigned)zip_header.formatted.cmpsize,
						(unsigned)percents,
						dtbuf,
						zip_header.formatted.crc32,
						dst_fn);
					total_size += zip_header.formatted.cmpsize;
				}
				total_usize += zip_header.formatted.ucmpsize;
				i = 'n';
			} else if (dst_fd == STDOUT_FILENO) {
				/* Extracting to STDOUT */
				i = -1;
			} else if (last_char_is(dst_fn, '/')) {
				/* Extract directory */
				if (stat(dst_fn, &stat_buf) == -1) {
					if (errno != ENOENT) {
						bb_perror_msg_and_die("can't stat '%s'", dst_fn);
					}
					if (!quiet) {
						printf("   creating: %s\n", dst_fn);
					}
					unzip_create_leading_dirs(dst_fn);
					if (bb_make_directory(dst_fn, dir_mode, FILEUTILS_IGNORE_CHMOD_ERR)) {
						xfunc_die();
					}
				} else {
					if (!S_ISDIR(stat_buf.st_mode)) {
						bb_error_msg_and_die("'%s' exists but is not a %s",
							dst_fn, "directory");
					}
				}
				i = 'n';
			} else {
				/* Extract file */
 check_file:
				if (stat(dst_fn, &stat_buf) == -1) {
					/* File does not exist */
					if (errno != ENOENT) {
						bb_perror_msg_and_die("can't stat '%s'", dst_fn);
					}
					i = 'y';
				} else {
					/* File already exists */
					if (overwrite == O_NEVER) {
						i = 'n';
					} else if (S_ISREG(stat_buf.st_mode)) {
						/* File is regular file */
						if (overwrite == O_ALWAYS) {
							i = 'y';
						} else {
							printf("replace %s? [y]es, [n]o, [A]ll, [N]one, [r]ename: ", dst_fn);
							my_fgets80(key_buf);
							i = key_buf[0];
						}
					} else {
						/* File is not regular file */
						bb_error_msg_and_die("'%s' exists but is not a %s",
							dst_fn, "regular file");
					}
				}
			}
		}

		switch (i) {
		case 'A':
			overwrite = O_ALWAYS;
		case 'y': /* Open file and fall into unzip */
			unzip_create_leading_dirs(dst_fn);
#if ENABLE_DESKTOP
			dst_fd = xopen3(dst_fn, O_WRONLY | O_CREAT | O_TRUNC, file_mode);
#else
			dst_fd = xopen(dst_fn, O_WRONLY | O_CREAT | O_TRUNC);
#endif
		case -1: /* Unzip */
			if (!quiet) {
				printf("  inflating: %s\n", dst_fn);
			}
			unzip_extract(&zip_header, dst_fd);
			if (dst_fd != STDOUT_FILENO) {
				/* closing STDOUT is potentially bad for future business */
				close(dst_fd);
			}
			break;

		case 'N':
			overwrite = O_NEVER;
		case 'n':
			/* Skip entry data */
			unzip_skip(zip_header.formatted.cmpsize);
			break;

		case 'r':
			/* Prompt for new name */
			printf("new name: ");
			my_fgets80(key_buf);
			free(dst_fn);
			dst_fn = xstrdup(key_buf);
			chomp(dst_fn);
			goto check_file;

		default:
			printf("error: invalid response [%c]\n", (char)i);
			goto check_file;
		}

		total_entries++;
	}

	if (listing && quiet <= 1) {
		if (!verbose) {
			//	"  Length      Date    Time    Name\n"
			//	"---------  ---------- -----   ----"
			printf( " --------%21s"               "-------\n"
				     "%9lu%21s"               "%u files\n",
				"",
				total_usize, "", total_entries);
		} else {
			unsigned long percents = total_usize - total_size;
			if ((long)percents < 0)
				percents = 0; /* happens if usize < size */
			percents = percents * 100;
			if (total_usize)
				percents /= total_usize;
			//	" Length   Method    Size  Cmpr    Date    Time   CRC-32   Name\n"
			//	"--------  ------  ------- ---- ---------- ----- --------  ----"
			printf( "--------          ------- ----%28s"                      "----\n"
				"%8lu"              "%17lu%4u%%%28s"                      "%u files\n",
				"",
				total_usize, total_size, (unsigned)percents, "",
				total_entries);
		}
	}

	return 0;
}
