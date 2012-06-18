/* vi: set sw=4 ts=4: */
/*
 *  Copyright 2003, Glenn McGrath
 *
 *  Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 *  Based on specification from
 *  http://www.opengroup.org/onlinepubs/007904975/utilities/uuencode.html
 *
 *  Bugs: the spec doesn't mention anything about "`\n`\n" prior to the
 *        "end" line
 */


#include "libbb.h"

static void read_stduu(FILE *src_stream, FILE *dst_stream)
{
	char *line;

	while ((line = xmalloc_fgetline(src_stream)) != NULL) {
		int encoded_len, str_len;
		char *line_ptr, *dst;

		if (strcmp(line, "end") == 0) {
			return; /* the only non-error exit */
		}

		line_ptr = line;
		while (*line_ptr) {
			*line_ptr = (*line_ptr - 0x20) & 0x3f;
			line_ptr++;
		}
		str_len = line_ptr - line;

		encoded_len = line[0] * 4 / 3;
		/* Check that line is not too short. (we tolerate
		 * overly _long_ line to accomodate possible extra '`').
		 * Empty line case is also caught here. */
		if (str_len <= encoded_len) {
			break; /* go to bb_error_msg_and_die("short file"); */
		}
		if (encoded_len <= 0) {
			/* Ignore the "`\n" line, why is it even in the encode file ? */
			free(line);
			continue;
		}
		if (encoded_len > 60) {
			bb_error_msg_and_die("line too long");
		}

		dst = line;
		line_ptr = line + 1;
		do {
			/* Merge four 6 bit chars to three 8 bit chars */
			*dst++ = line_ptr[0] << 2 | line_ptr[1] >> 4;
			encoded_len--;
			if (encoded_len == 0) {
				break;
			}

			*dst++ = line_ptr[1] << 4 | line_ptr[2] >> 2;
			encoded_len--;
			if (encoded_len == 0) {
				break;
			}

			*dst++ = line_ptr[2] << 6 | line_ptr[3];
			line_ptr += 4;
			encoded_len -= 2;
		} while (encoded_len > 0);
		fwrite(line, 1, dst - line, dst_stream);
		free(line);
	}
	bb_error_msg_and_die("short file");
}

static void read_base64(FILE *src_stream, FILE *dst_stream)
{
	int term_count = 1;

	while (1) {
		char translated[4];
		int count = 0;

		while (count < 4) {
			char *table_ptr;
			int ch;

			/* Get next _valid_ character.
			 * global vector bb_uuenc_tbl_base64[] contains this string:
			 * "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n"
			 */
			do {
				ch = fgetc(src_stream);
				if (ch == EOF) {
					bb_error_msg_and_die("short file");
				}
				table_ptr = strchr(bb_uuenc_tbl_base64, ch);
			} while (table_ptr == NULL);

			/* Convert encoded character to decimal */
			ch = table_ptr - bb_uuenc_tbl_base64;

			if (*table_ptr == '=') {
				if (term_count == 0) {
					translated[count] = '\0';
					break;
				}
				term_count++;
			} else if (*table_ptr == '\n') {
				/* Check for terminating line */
				if (term_count == 5) {
					return;
				}
				term_count = 1;
				continue;
			} else {
				translated[count] = ch;
				count++;
				term_count = 0;
			}
		}

		/* Merge 6 bit chars to 8 bit */
		if (count > 1) {
			fputc(translated[0] << 2 | translated[1] >> 4, dst_stream);
		}
		if (count > 2) {
			fputc(translated[1] << 4 | translated[2] >> 2, dst_stream);
		}
		if (count > 3) {
			fputc(translated[2] << 6 | translated[3], dst_stream);
		}
	}
}

int uudecode_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uudecode_main(int argc UNUSED_PARAM, char **argv)
{
	FILE *src_stream;
	char *outname = NULL;
	char *line;

	opt_complementary = "?1"; /* 1 argument max */
	getopt32(argv, "o:", &outname);
	argv += optind;

	if (!*argv)
		*--argv = (char*)"-";
	src_stream = xfopen_stdin(*argv);

	/* Search for the start of the encoding */
	while ((line = xmalloc_fgetline(src_stream)) != NULL) {
		void (*decode_fn_ptr)(FILE *src, FILE *dst);
		char *line_ptr;
		FILE *dst_stream;
		int mode;

		if (strncmp(line, "begin-base64 ", 13) == 0) {
			line_ptr = line + 13;
			decode_fn_ptr = read_base64;
		} else if (strncmp(line, "begin ", 6) == 0) {
			line_ptr = line + 6;
			decode_fn_ptr = read_stduu;
		} else {
			free(line);
			continue;
		}

		/* begin line found. decode and exit */
		mode = bb_strtou(line_ptr, NULL, 8);
		if (outname == NULL) {
			outname = strchr(line_ptr, ' ');
			if ((outname == NULL) || (*outname == '\0')) {
				break;
			}
			outname++;
		}
		dst_stream = stdout;
		if (NOT_LONE_DASH(outname)) {
			dst_stream = xfopen_for_write(outname);
			fchmod(fileno(dst_stream), mode & (S_IRWXU | S_IRWXG | S_IRWXO));
		}
		free(line);
		decode_fn_ptr(src_stream, dst_stream);
		/* fclose_if_not_stdin(src_stream); - redundant */
		return EXIT_SUCCESS;
	}
	bb_error_msg_and_die("no 'begin' line");
}

/* Test script.
Put this into an empty dir with busybox binary, an run.

#!/bin/sh
test -x busybox || { echo "No ./busybox?"; exit; }
ln -sf busybox uudecode
ln -sf busybox uuencode
>A_null
echo -n A >A
echo -n AB >AB
echo -n ABC >ABC
echo -n ABCD >ABCD
echo -n ABCDE >ABCDE
echo -n ABCDEF >ABCDEF
cat busybox >A_bbox
for f in A*; do
    echo uuencode $f
    ./uuencode    $f <$f >u_$f
    ./uuencode -m $f <$f >m_$f
done
mkdir unpk_u unpk_m 2>/dev/null
for f in u_*; do
    ./uudecode <$f -o unpk_u/${f:2}
    diff -a ${f:2} unpk_u/${f:2} >/dev/null 2>&1
    echo uudecode $f: $?
done
for f in m_*; do
    ./uudecode <$f -o unpk_m/${f:2}
    diff -a ${f:2} unpk_m/${f:2} >/dev/null 2>&1
    echo uudecode $f: $?
done
*/
