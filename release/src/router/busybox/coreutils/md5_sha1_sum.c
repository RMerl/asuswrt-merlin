/* vi: set sw=4 ts=4: */
/*
 *  Copyright (C) 2003 Glenn L. McGrath
 *  Copyright (C) 2003-2004 Erik Andersen
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

typedef enum {
	/* 4th letter of applet_name is... */
	HASH_MD5 = 's', /* "md5>s<um" */
	HASH_SHA1 = '1',
	HASH_SHA256 = '2',
	HASH_SHA512 = '5',
} hash_algo_t;

#define FLAG_SILENT	1
#define FLAG_CHECK	2
#define FLAG_WARN	4

/* This might be useful elsewhere */
static unsigned char *hash_bin_to_hex(unsigned char *hash_value,
				unsigned hash_length)
{
	/* xzalloc zero-terminates */
	char *hex_value = xzalloc((hash_length * 2) + 1);
	bin2hex(hex_value, (char*)hash_value, hash_length);
	return (unsigned char *)hex_value;
}

static uint8_t *hash_file(const char *filename /*, hash_algo_t hash_algo*/)
{
	int src_fd, hash_len, count;
	union _ctx_ {
		sha512_ctx_t sha512;
		sha256_ctx_t sha256;
		sha1_ctx_t sha1;
		md5_ctx_t md5;
	} context;
	uint8_t *hash_value = NULL;
	RESERVE_CONFIG_UBUFFER(in_buf, 4096);
	void FAST_FUNC (*update)(const void*, size_t, void*);
	void FAST_FUNC (*final)(void*, void*);
	hash_algo_t hash_algo = applet_name[3];

	src_fd = open_or_warn_stdin(filename);
	if (src_fd < 0) {
		return NULL;
	}

	/* figure specific hash algorithims */
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
	} else {
		bb_error_msg_and_die("algorithm not supported");
	}

	while (0 < (count = safe_read(src_fd, in_buf, 4096))) {
		update(in_buf, count, &context);
	}

	if (count == 0) {
		final(in_buf, &context);
		hash_value = hash_bin_to_hex(in_buf, hash_len);
	}

	RELEASE_CONFIG_BUFFER(in_buf);

	if (src_fd != STDIN_FILENO) {
		close(src_fd);
	}

	return hash_value;
}

int md5_sha1_sum_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int md5_sha1_sum_main(int argc UNUSED_PARAM, char **argv)
{
	int return_value = EXIT_SUCCESS;
	uint8_t *hash_value;
	unsigned flags;
	/*hash_algo_t hash_algo = applet_name[3];*/

	if (ENABLE_FEATURE_MD5_SHA1_SUM_CHECK) {
		/* -b "binary", -t "text" are ignored (shaNNNsum compat) */
		flags = getopt32(argv, "scwbt");
	}
	else optind = 1;
	argv += optind;
	//argc -= optind;
	if (!*argv)
		*--argv = (char*)"-";

	if (ENABLE_FEATURE_MD5_SHA1_SUM_CHECK && !(flags & FLAG_CHECK)) {
		if (flags & FLAG_SILENT) {
			bb_error_msg_and_die
				("-%c is meaningful only when verifying checksums", 's');
		} else if (flags & FLAG_WARN) {
			bb_error_msg_and_die
				("-%c is meaningful only when verifying checksums", 'w');
		}
	}

	if (ENABLE_FEATURE_MD5_SHA1_SUM_CHECK && (flags & FLAG_CHECK)) {
		FILE *pre_computed_stream;
		int count_total = 0;
		int count_failed = 0;
		char *line;

		if (argv[1]) {
			bb_error_msg_and_die
				("only one argument may be specified when using -c");
		}

		pre_computed_stream = xfopen_stdin(argv[0]);

		while ((line = xmalloc_fgetline(pre_computed_stream)) != NULL) {
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

			hash_value = hash_file(filename_ptr /*, hash_algo*/);

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
		/*
		if (fclose_if_not_stdin(pre_computed_stream) == EOF) {
			bb_perror_msg_and_die("can't close file %s", file_ptr);
		}
		*/
	} else {
		do {
			hash_value = hash_file(*argv/*, hash_algo*/);
			if (hash_value == NULL) {
				return_value = EXIT_FAILURE;
			} else {
				printf("%s  %s\n", hash_value, *argv);
				free(hash_value);
			}
		} while (*++argv);
	}
	return return_value;
}
