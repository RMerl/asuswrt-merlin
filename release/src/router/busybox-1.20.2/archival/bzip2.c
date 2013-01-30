/*
 * Copyright (C) 2007 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * This file uses bzip2 library code which is written
 * by Julian Seward <jseward@bzip.org>.
 * See README and LICENSE files in bz/ directory for more information
 * about bzip2 library code.
 */

//usage:#define bzip2_trivial_usage
//usage:       "[OPTIONS] [FILE]..."
//usage:#define bzip2_full_usage "\n\n"
//usage:       "Compress FILEs (or stdin) with bzip2 algorithm\n"
//usage:     "\n	-1..9	Compression level"
//usage:     "\n	-d	Decompress"
//usage:     "\n	-c	Write to stdout"
//usage:     "\n	-f	Force"

#include "libbb.h"
#include "bb_archive.h"

#define CONFIG_BZIP2_FAST 1

/* Speed test:
 * Compiled with gcc 4.2.1, run on Athlon 64 1800 MHz (512K L2 cache).
 * Stock bzip2 is 26.4% slower than bbox bzip2 at SPEED 1
 * (time to compress gcc-4.2.1.tar is 126.4% compared to bbox).
 * At SPEED 5 difference is 32.7%.
 *
 * Test run of all CONFIG_BZIP2_FAST values on a 11Mb text file:
 *     Size   Time (3 runs)
 * 0:  10828  4.145 4.146 4.148
 * 1:  11097  3.845 3.860 3.861
 * 2:  11392  3.763 3.767 3.768
 * 3:  11892  3.722 3.724 3.727
 * 4:  12740  3.637 3.640 3.644
 * 5:  17273  3.497 3.509 3.509
 */


#define BZ_DEBUG 0
/* Takes ~300 bytes, detects corruption caused by bad RAM etc */
#define BZ_LIGHT_DEBUG 0

#include "libarchive/bz/bzlib.h"

#include "libarchive/bz/bzlib_private.h"

#include "libarchive/bz/blocksort.c"
#include "libarchive/bz/bzlib.c"
#include "libarchive/bz/compress.c"
#include "libarchive/bz/huffman.c"

/* No point in being shy and having very small buffer here.
 * bzip2 internal buffers are much bigger anyway, hundreds of kbytes.
 * If iobuf is several pages long, malloc() may use mmap,
 * making iobuf is page aligned and thus (maybe) have one memcpy less
 * if kernel is clever enough.
 */
enum {
	IOBUF_SIZE = 8 * 1024
};

static uint8_t level;

/* NB: compressStream() has to return -1 on errors, not die.
 * bbunpack() will correctly clean up in this case
 * (delete incomplete .bz2 file)
 */

/* Returns:
 * -1 on errors
 * total written bytes so far otherwise
 */
static
IF_DESKTOP(long long) int bz_write(bz_stream *strm, void* rbuf, ssize_t rlen, void *wbuf)
{
	int n, n2, ret;

	strm->avail_in = rlen;
	strm->next_in = rbuf;
	while (1) {
		strm->avail_out = IOBUF_SIZE;
		strm->next_out = wbuf;

		ret = BZ2_bzCompress(strm, rlen ? BZ_RUN : BZ_FINISH);
		if (ret != BZ_RUN_OK /* BZ_RUNning */
		 && ret != BZ_FINISH_OK /* BZ_FINISHing, but not done yet */
		 && ret != BZ_STREAM_END /* BZ_FINISHed */
		) {
			bb_error_msg_and_die("internal error %d", ret);
		}

		n = IOBUF_SIZE - strm->avail_out;
		if (n) {
			n2 = full_write(STDOUT_FILENO, wbuf, n);
			if (n2 != n) {
				if (n2 >= 0)
					errno = 0; /* prevent bogus error message */
				bb_perror_msg(n2 >= 0 ? "short write" : bb_msg_write_error);
				return -1;
			}
		}

		if (ret == BZ_STREAM_END)
			break;
		if (rlen && strm->avail_in == 0)
			break;
	}
	return 0 IF_DESKTOP( + strm->total_out );
}

static
IF_DESKTOP(long long) int FAST_FUNC compressStream(transformer_aux_data_t *aux UNUSED_PARAM)
{
	IF_DESKTOP(long long) int total;
	ssize_t count;
	bz_stream bzs; /* it's small */
#define strm (&bzs)
	char *iobuf;
#define rbuf iobuf
#define wbuf (iobuf + IOBUF_SIZE)

	iobuf = xmalloc(2 * IOBUF_SIZE);
	BZ2_bzCompressInit(strm, level);

	while (1) {
		count = full_read(STDIN_FILENO, rbuf, IOBUF_SIZE);
		if (count < 0) {
			bb_perror_msg(bb_msg_read_error);
			total = -1;
			break;
		}
		/* if count == 0, bz_write finalizes compression */
		total = bz_write(strm, rbuf, count, wbuf);
		if (count == 0 || total < 0)
			break;
	}

	/* Can't be conditional on ENABLE_FEATURE_CLEAN_UP -
	 * we are called repeatedly
	 */
	BZ2_bzCompressEnd(strm);
	free(iobuf);

	return total;
}

int bzip2_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bzip2_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;

	/* standard bzip2 flags
	 * -d --decompress force decompression
	 * -z --compress force compression
	 * -k --keep     keep (don't delete) input files
	 * -f --force    overwrite existing output files
	 * -t --test     test compressed file integrity
	 * -c --stdout   output to standard out
	 * -q --quiet    suppress noncritical error messages
	 * -v --verbose  be verbose (a 2nd -v gives more)
	 * -s --small    use less memory (at most 2500k)
	 * -1 .. -9      set block size to 100k .. 900k
	 * --fast        alias for -1
	 * --best        alias for -9
	 */

	opt_complementary = "s2"; /* -s means -2 (compatibility) */
	/* Must match bbunzip's constants OPT_STDOUT, OPT_FORCE! */
	opt = getopt32(argv, "cfv" IF_BUNZIP2("dt") "123456789qzs");
#if ENABLE_BUNZIP2 /* bunzip2_main may not be visible... */
	if (opt & 0x18) // -d and/or -t
		return bunzip2_main(argc, argv);
	opt >>= 5;
#else
	opt >>= 3;
#endif
	opt = (uint8_t)opt; /* isolate bits for -1..-8 */
	opt |= 0x100; /* if nothing else, assume -9 */
	level = 1;
	while (!(opt & 1)) {
		level++;
		opt >>= 1;
	}

	argv += optind;
	option_mask32 &= 0x7; /* ignore all except -cfv */
	return bbunpack(argv, compressStream, append_ext, "bz2");
}
