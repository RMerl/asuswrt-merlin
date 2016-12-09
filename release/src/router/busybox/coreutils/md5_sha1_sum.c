/* vi: set sw=4 ts=4: */
/*
 *  Copyright (C) 2003 Glenn L. McGrath
 *  Copyright (C) 2003-2004 Erik Andersen
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define md5sum_trivial_usage
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define md5sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_MD5_SHA1_SUM_CHECK(" or check") " MD5 checksums"
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:
//usage:#define md5sum_example_usage
//usage:       "$ md5sum < busybox\n"
//usage:       "6fd11e98b98a58f64ff3398d7b324003\n"
//usage:       "$ md5sum busybox\n"
//usage:       "6fd11e98b98a58f64ff3398d7b324003  busybox\n"
//usage:       "$ md5sum -c -\n"
//usage:       "6fd11e98b98a58f64ff3398d7b324003  busybox\n"
//usage:       "busybox: OK\n"
//usage:       "^D\n"
//usage:
//usage:#define sha1sum_trivial_usage
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define sha1sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_MD5_SHA1_SUM_CHECK(" or check") " SHA1 checksums"
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:
//usage:#define sha256sum_trivial_usage
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define sha256sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_MD5_SHA1_SUM_CHECK(" or check") " SHA256 checksums"
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:
//usage:#define sha512sum_trivial_usage
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define sha512sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_MD5_SHA1_SUM_CHECK(" or check") " SHA512 checksums"
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)
//usage:
//usage:#define sha3sum_trivial_usage
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK("[-c[sw]] ")"[FILE]..."
//usage:#define sha3sum_full_usage "\n\n"
//usage:       "Print" IF_FEATURE_MD5_SHA1_SUM_CHECK(" or check") " SHA3-512 checksums"
//usage:	IF_FEATURE_MD5_SHA1_SUM_CHECK( "\n"
//usage:     "\n	-c	Check sums against list in FILEs"
//usage:     "\n	-s	Don't output anything, status code shows success"
//usage:     "\n	-w	Warn about improperly formatted checksum lines"
//usage:	)

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */

enum {
	/* 4th letter of applet_name is... */
	HASH_MD5 = 's', /* "md5>s<um" */
	HASH_SHA1 = '1',
	HASH_SHA256 = '2',
	HASH_SHA3 = '3',
	HASH_SHA512 = '5',
};

#define FLAG_SILENT  1
#define FLAG_CHECK   2
#define FLAG_WARN    4

/* This might be useful elsewhere */
static unsigned char *hash_bin_to_hex(unsigned char *hash_value,
				unsigned hash_length)
{
	/* xzalloc zero-terminates */
	char *hex_value = xzalloc((hash_length * 2) + 1);
	bin2hex(hex_value, (char*)hash_value, hash_length);
	return (unsigned char *)hex_value;
}

static uint8_t *hash_file(const char *filename)
{
	int src_fd, hash_len, count;
	union _ctx_ {
		sha3_ctx_t sha3;
		sha512_ctx_t sha512;
		sha256_ctx_t sha256;
		sha1_ctx_t sha1;
		md5_ctx_t md5;
	} context;
	uint8_t *hash_value;
	void FAST_FUNC (*update)(void*, const void*, size_t);
	void FAST_FUNC (*final)(void*, void*);
	char hash_algo;

	src_fd = open_or_warn_stdin(filename);
	if (src_fd < 0) {
		return NULL;
	}

	hash_algo = applet_name[3];

	/* figure specific hash algorithms */
	if (ENABLE_MD5SUM && hash_algo == HASH_MD5) {
		md5_begin(&context.md5);
		update = (void*)md5_hash;
		final = (void*)md5_end;
		hash_len = 16;
	} else if (ENABLE_SHA1SUM && hash_algo == HASH_SHA1) {
		sha1_begin(&context.sha1);
		update = (void*)sha1_hash;
		final = (void*)sha1_end;
		hash_len = 20;
	} else if (ENABLE_SHA256SUM && hash_algo == HASH_SHA256) {
		sha256_begin(&context.sha256);
		update = (void*)sha256_hash;
		final = (void*)sha256_end;
		hash_len = 32;
	} else if (ENABLE_SHA512SUM && hash_algo == HASH_SHA512) {
		sha512_begin(&context.sha512);
		update = (void*)sha512_hash;
		final = (void*)sha512_end;
		hash_len = 64;
	} else if (ENABLE_SHA3SUM && hash_algo == HASH_SHA3) {
		sha3_begin(&context.sha3);
		update = (void*)sha3_hash;
		final = (void*)sha3_end;
		hash_len = 64;
	} else {
		xfunc_die(); /* can't reach this */
	}

	{
		RESERVE_CONFIG_UBUFFER(in_buf, 4096);
		while ((count = safe_read(src_fd, in_buf, 4096)) > 0) {
			update(&context, in_buf, count);
		}
		hash_value = NULL;
		if (count < 0)
			bb_perror_msg("can't read '%s'", filename);
		else /* count == 0 */ {
			final(&context, in_buf);
			hash_value = hash_bin_to_hex(in_buf, hash_len);
		}
		RELEASE_CONFIG_BUFFER(in_buf);
	}

	if (src_fd != STDIN_FILENO) {
		close(src_fd);
	}

	return hash_value;
}

int md5_sha1_sum_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int md5_sha1_sum_main(int argc UNUSED_PARAM, char **argv)
{
	int return_value = EXIT_SUCCESS;
	unsigned flags;

	if (ENABLE_FEATURE_MD5_SHA1_SUM_CHECK) {
		/* -b "binary", -t "text" are ignored (shaNNNsum compat) */
		flags = getopt32(argv, "scwbt");
		argv += optind;
		//argc -= optind;
	} else {
		argv += 1;
		//argc -= 1;
	}
	if (!*argv)
		*--argv = (char*)"-";

	if (ENABLE_FEATURE_MD5_SHA1_SUM_CHECK && !(flags & FLAG_CHECK)) {
		if (flags & FLAG_SILENT) {
			bb_error_msg_and_die("-%c is meaningful only with -c", 's');
		}
		if (flags & FLAG_WARN) {
			bb_error_msg_and_die("-%c is meaningful only with -c", 'w');
		}
	}

	do {
		if (ENABLE_FEATURE_MD5_SHA1_SUM_CHECK && (flags & FLAG_CHECK)) {
			FILE *pre_computed_stream;
			char *line;
			int count_total = 0;
			int count_failed = 0;

			pre_computed_stream = xfopen_stdin(*argv);

			while ((line = xmalloc_fgetline(pre_computed_stream)) != NULL) {
				uint8_t *hash_value;
				char *filename_ptr;

				count_total++;
				filename_ptr = strstr(line, "  ");
				/* handle format for binary checksums */
				if (filename_ptr == NULL) {
					filename_ptr = strstr(line, " *");
				}
				if (filename_ptr == NULL) {
					if (flags & FLAG_WARN) {
						bb_error_msg("invalid format");
					}
					count_failed++;
					return_value = EXIT_FAILURE;
					free(line);
					continue;
				}
				*filename_ptr = '\0';
				filename_ptr += 2;

				hash_value = hash_file(filename_ptr);

				if (hash_value && (strcmp((char*)hash_value, line) == 0)) {
					if (!(flags & FLAG_SILENT))
						printf("%s: OK\n", filename_ptr);
				} else {
					if (!(flags & FLAG_SILENT))
						printf("%s: FAILED\n", filename_ptr);
					count_failed++;
					return_value = EXIT_FAILURE;
				}
				/* possible free(NULL) */
				free(hash_value);
				free(line);
			}
			if (count_failed && !(flags & FLAG_SILENT)) {
				bb_error_msg("WARNING: %d of %d computed checksums did NOT match",
						count_failed, count_total);
			}
			fclose_if_not_stdin(pre_computed_stream);
		} else {
			uint8_t *hash_value = hash_file(*argv);
			if (hash_value == NULL) {
				return_value = EXIT_FAILURE;
			} else {
				printf("%s  %s\n", hash_value, *argv);
				free(hash_value);
			}
		}
	} while (*++argv);

	return return_value;
}
