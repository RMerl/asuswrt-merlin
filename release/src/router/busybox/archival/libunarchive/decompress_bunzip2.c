/* vi: set sw=4 ts=4: */
/* Small bzip2 deflate implementation, by Rob Landley (rob@landley.net).

   Based on bzip2 decompression code by Julian R Seward (jseward@acm.org),
   which also acknowledges contributions by Mike Burrows, David Wheeler,
   Peter Fenwick, Alistair Moffat, Radford Neal, Ian H. Witten,
   Robert Sedgewick, and Jon L. Bentley.

   Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

/*
	Size and speed optimizations by Manuel Novoa III  (mjn3@codepoet.org).

	More efficient reading of Huffman codes, a streamlined read_bunzip()
	function, and various other tweaks.  In (limited) tests, approximately
	20% faster than bzcat on x86 and about 10% faster on arm.

	Note that about 2/3 of the time is spent in read_unzip() reversing
	the Burrows-Wheeler transformation.  Much of that time is delay
	resulting from cache misses.

	I would ask that anyone benefiting from this work, especially those
	using it in commercial products, consider making a donation to my local
	non-profit hospice organization (www.hospiceacadiana.com) in the name of
	the woman I loved, Toni W. Hagan, who passed away Feb. 12, 2003.

	Manuel
 */

#include "libbb.h"
#include "unarchive.h"

/* Constants for Huffman coding */
#define MAX_GROUPS          6
#define GROUP_SIZE          50      /* 64 would have been more efficient */
#define MAX_HUFCODE_BITS    20      /* Longest Huffman code allowed */
#define MAX_SYMBOLS         258     /* 256 literals + RUNA + RUNB */
#define SYMBOL_RUNA         0
#define SYMBOL_RUNB         1

/* Status return values */
#define RETVAL_OK                       0
#define RETVAL_LAST_BLOCK               (-1)
#define RETVAL_NOT_BZIP_DATA            (-2)
#define RETVAL_UNEXPECTED_INPUT_EOF     (-3)
#define RETVAL_SHORT_WRITE              (-4)
#define RETVAL_DATA_ERROR               (-5)
#define RETVAL_OUT_OF_MEMORY            (-6)
#define RETVAL_OBSOLETE_INPUT           (-7)

/* Other housekeeping constants */
#define IOBUF_SIZE          4096

/* This is what we know about each Huffman coding group */
struct group_data {
	/* We have an extra slot at the end of limit[] for a sentinel value. */
	int limit[MAX_HUFCODE_BITS+1], base[MAX_HUFCODE_BITS], permute[MAX_SYMBOLS];
	int minLen, maxLen;
};

/* Structure holding all the housekeeping data, including IO buffers and
 * memory that persists between calls to bunzip
 * Found the most used member:
 *  cat this_file.c | sed -e 's/"/ /g' -e "s/'/ /g" | xargs -n1 \
 *  | grep 'bd->' | sed 's/^.*bd->/bd->/' | sort | $PAGER
 * and moved it (inbufBitCount) to offset 0.
 */
struct bunzip_data {
	/* I/O tracking data (file handles, buffers, positions, etc.) */
	unsigned inbufBitCount, inbufBits;
	int in_fd, out_fd, inbufCount, inbufPos /*, outbufPos*/;
	unsigned char *inbuf /*,*outbuf*/;

	/* State for interrupting output loop */
	int writeCopies, writePos, writeRunCountdown, writeCount, writeCurrent;

	/* The CRC values stored in the block header and calculated from the data */
	uint32_t headerCRC, totalCRC, writeCRC;

	/* Intermediate buffer and its size (in bytes) */
	unsigned *dbuf, dbufSize;

	/* For I/O error handling */
	jmp_buf jmpbuf;

	/* Big things go last (register-relative addressing can be larger for big offsets) */
	uint32_t crc32Table[256];
	unsigned char selectors[32768];			/* nSelectors=15 bits */
	struct group_data groups[MAX_GROUPS];	/* Huffman coding tables */
};
/* typedef struct bunzip_data bunzip_data; -- done in .h file */


/* Return the next nnn bits of input.  All reads from the compressed input
   are done through this function.  All reads are big endian */

static unsigned get_bits(bunzip_data *bd, int bits_wanted)
{
	unsigned bits = 0;

	/* If we need to get more data from the byte buffer, do so.  (Loop getting
	   one byte at a time to enforce endianness and avoid unaligned access.) */
	while ((int)(bd->inbufBitCount) < bits_wanted) {

		/* If we need to read more data from file into byte buffer, do so */
		if (bd->inbufPos == bd->inbufCount) {
			/* if "no input fd" case: in_fd == -1, read fails, we jump */
			bd->inbufCount = read(bd->in_fd, bd->inbuf, IOBUF_SIZE);
			if (bd->inbufCount <= 0)
				longjmp(bd->jmpbuf, RETVAL_UNEXPECTED_INPUT_EOF);
			bd->inbufPos = 0;
		}

		/* Avoid 32-bit overflow (dump bit buffer to top of output) */
		if (bd->inbufBitCount >= 24) {
			bits = bd->inbufBits & ((1 << bd->inbufBitCount) - 1);
			bits_wanted -= bd->inbufBitCount;
			bits <<= bits_wanted;
			bd->inbufBitCount = 0;
		}

		/* Grab next 8 bits of input from buffer. */
		bd->inbufBits = (bd->inbufBits << 8) | bd->inbuf[bd->inbufPos++];
		bd->inbufBitCount += 8;
	}

	/* Calculate result */
	bd->inbufBitCount -= bits_wanted;
	bits |= (bd->inbufBits >> bd->inbufBitCount) & ((1 << bits_wanted) - 1);

	return bits;
}

/* Unpacks the next block and sets up for the inverse burrows-wheeler step. */
static int get_next_block(bunzip_data *bd)
{
	struct group_data *hufGroup;
	int dbufCount, nextSym, dbufSize, groupCount, *base, *limit, selector,
		i, j, k, t, runPos, symCount, symTotal, nSelectors, byteCount[256];
	unsigned char uc, symToByte[256], mtfSymbol[256], *selectors;
	unsigned *dbuf, origPtr;

	dbuf = bd->dbuf;
	dbufSize = bd->dbufSize;
	selectors = bd->selectors;

	/* Reset longjmp I/O error handling */
	i = setjmp(bd->jmpbuf);
	if (i) return i;

	/* Read in header signature and CRC, then validate signature.
	   (last block signature means CRC is for whole file, return now) */
	i = get_bits(bd, 24);
	j = get_bits(bd, 24);
	bd->headerCRC = get_bits(bd, 32);
	if ((i == 0x177245) && (j == 0x385090)) return RETVAL_LAST_BLOCK;
	if ((i != 0x314159) || (j != 0x265359)) return RETVAL_NOT_BZIP_DATA;

	/* We can add support for blockRandomised if anybody complains.  There was
	   some code for this in busybox 1.0.0-pre3, but nobody ever noticed that
	   it didn't actually work. */
	if (get_bits(bd, 1)) return RETVAL_OBSOLETE_INPUT;
	origPtr = get_bits(bd, 24);
	if ((int)origPtr > dbufSize) return RETVAL_DATA_ERROR;

	/* mapping table: if some byte values are never used (encoding things
	   like ascii text), the compression code removes the gaps to have fewer
	   symbols to deal with, and writes a sparse bitfield indicating which
	   values were present.  We make a translation table to convert the symbols
	   back to the corresponding bytes. */
	t = get_bits(bd, 16);
	symTotal = 0;
	for (i = 0; i < 16; i++) {
		if (t & (1 << (15-i))) {
			k = get_bits(bd, 16);
			for (j = 0; j < 16; j++)
				if (k & (1 << (15-j)))
					symToByte[symTotal++] = (16*i) + j;
		}
	}

	/* How many different Huffman coding groups does this block use? */
	groupCount = get_bits(bd, 3);
	if (groupCount < 2 || groupCount > MAX_GROUPS)
		return RETVAL_DATA_ERROR;

	/* nSelectors: Every GROUP_SIZE many symbols we select a new Huffman coding
	   group.  Read in the group selector list, which is stored as MTF encoded
	   bit runs.  (MTF=Move To Front, as each value is used it's moved to the
	   start of the list.) */
	nSelectors = get_bits(bd, 15);
	if (!nSelectors) return RETVAL_DATA_ERROR;
	for (i = 0; i < groupCount; i++) mtfSymbol[i] = i;
	for (i = 0; i < nSelectors; i++) {

		/* Get next value */
		for (j = 0; get_bits(bd, 1); j++)
			if (j >= groupCount) return RETVAL_DATA_ERROR;

		/* Decode MTF to get the next selector */
		uc = mtfSymbol[j];
		for (;j;j--) mtfSymbol[j] = mtfSymbol[j-1];
		mtfSymbol[0] = selectors[i] = uc;
	}

	/* Read the Huffman coding tables for each group, which code for symTotal
	   literal symbols, plus two run symbols (RUNA, RUNB) */
	symCount = symTotal + 2;
	for (j = 0; j < groupCount; j++) {
		unsigned char length[MAX_SYMBOLS];
		/* 8 bits is ALMOST enough for temp[], see below */
		unsigned temp[MAX_HUFCODE_BITS+1];
		int minLen, maxLen, pp;

		/* Read Huffman code lengths for each symbol.  They're stored in
		   a way similar to mtf; record a starting value for the first symbol,
		   and an offset from the previous value for everys symbol after that.
		   (Subtracting 1 before the loop and then adding it back at the end is
		   an optimization that makes the test inside the loop simpler: symbol
		   length 0 becomes negative, so an unsigned inequality catches it.) */
		t = get_bits(bd, 5) - 1;
		for (i = 0; i < symCount; i++) {
			for (;;) {
				if ((unsigned)t > (MAX_HUFCODE_BITS-1))
					return RETVAL_DATA_ERROR;

				/* If first bit is 0, stop.  Else second bit indicates whether
				   to increment or decrement the value.  Optimization: grab 2
				   bits and unget the second if the first was 0. */
				k = get_bits(bd, 2);
				if (k < 2) {
					bd->inbufBitCount++;
					break;
				}

				/* Add one if second bit 1, else subtract 1.  Avoids if/else */
				t += (((k+1) & 2) - 1);
			}

			/* Correct for the initial -1, to get the final symbol length */
			length[i] = t + 1;
		}

		/* Find largest and smallest lengths in this group */
		minLen = maxLen = length[0];
		for (i = 1; i < symCount; i++) {
			if (length[i] > maxLen) maxLen = length[i];
			else if (length[i] < minLen) minLen = length[i];
		}

		/* Calculate permute[], base[], and limit[] tables from length[].
		 *
		 * permute[] is the lookup table for converting Huffman coded symbols
		 * into decoded symbols.  base[] is the amount to subtract from the
		 * value of a Huffman symbol of a given length when using permute[].
		 *
		 * limit[] indicates the largest numerical value a symbol with a given
		 * number of bits can have.  This is how the Huffman codes can vary in
		 * length: each code with a value>limit[length] needs another bit.
		 */
		hufGroup = bd->groups + j;
		hufGroup->minLen = minLen;
		hufGroup->maxLen = maxLen;

		/* Note that minLen can't be smaller than 1, so we adjust the base
		   and limit array pointers so we're not always wasting the first
		   entry.  We do this again when using them (during symbol decoding).*/
		base = hufGroup->base - 1;
		limit = hufGroup->limit - 1;

		/* Calculate permute[].  Concurently, initialize temp[] and limit[]. */
		pp = 0;
		for (i = minLen; i <= maxLen; i++) {
			temp[i] = limit[i] = 0;
			for (t = 0; t < symCount; t++)
				if (length[t] == i)
					hufGroup->permute[pp++] = t;
		}

		/* Count symbols coded for at each bit length */
		/* NB: in pathological cases, temp[8] can end ip being 256.
		 * That's why uint8_t is too small for temp[]. */
		for (i = 0; i < symCount; i++) temp[length[i]]++;

		/* Calculate limit[] (the largest symbol-coding value at each bit
		 * length, which is (previous limit<<1)+symbols at this level), and
		 * base[] (number of symbols to ignore at each bit length, which is
		 * limit minus the cumulative count of symbols coded for already). */
		pp = t = 0;
		for (i = minLen; i < maxLen; i++) {
			pp += temp[i];

			/* We read the largest possible symbol size and then unget bits
			   after determining how many we need, and those extra bits could
			   be set to anything.  (They're noise from future symbols.)  At
			   each level we're really only interested in the first few bits,
			   so here we set all the trailing to-be-ignored bits to 1 so they
			   don't affect the value>limit[length] comparison. */
			limit[i] = (pp << (maxLen - i)) - 1;
			pp <<= 1;
			t += temp[i];
			base[i+1] = pp - t;
		}
		limit[maxLen+1] = INT_MAX; /* Sentinel value for reading next sym. */
		limit[maxLen] = pp + temp[maxLen] - 1;
		base[minLen] = 0;
	}

	/* We've finished reading and digesting the block header.  Now read this
	   block's Huffman coded symbols from the file and undo the Huffman coding
	   and run length encoding, saving the result into dbuf[dbufCount++] = uc */

	/* Initialize symbol occurrence counters and symbol Move To Front table */
	memset(byteCount, 0, sizeof(byteCount)); /* smaller, maybe slower? */
	for (i = 0; i < 256; i++) {
		//byteCount[i] = 0;
		mtfSymbol[i] = (unsigned char)i;
	}

	/* Loop through compressed symbols. */

	runPos = dbufCount = selector = 0;
	for (;;) {

		/* Fetch next Huffman coding group from list. */
		symCount = GROUP_SIZE - 1;
		if (selector >= nSelectors) return RETVAL_DATA_ERROR;
		hufGroup = bd->groups + selectors[selector++];
		base = hufGroup->base - 1;
		limit = hufGroup->limit - 1;
 continue_this_group:

		/* Read next Huffman-coded symbol. */

		/* Note: It is far cheaper to read maxLen bits and back up than it is
		   to read minLen bits and then an additional bit at a time, testing
		   as we go.  Because there is a trailing last block (with file CRC),
		   there is no danger of the overread causing an unexpected EOF for a
		   valid compressed file.  As a further optimization, we do the read
		   inline (falling back to a call to get_bits if the buffer runs
		   dry).  The following (up to got_huff_bits:) is equivalent to
		   j = get_bits(bd, hufGroup->maxLen);
		 */
		while ((int)(bd->inbufBitCount) < hufGroup->maxLen) {
			if (bd->inbufPos == bd->inbufCount) {
				j = get_bits(bd, hufGroup->maxLen);
				goto got_huff_bits;
			}
			bd->inbufBits = (bd->inbufBits << 8) | bd->inbuf[bd->inbufPos++];
			bd->inbufBitCount += 8;
		};
		bd->inbufBitCount -= hufGroup->maxLen;
		j = (bd->inbufBits >> bd->inbufBitCount) & ((1 << hufGroup->maxLen) - 1);

 got_huff_bits:

		/* Figure how how many bits are in next symbol and unget extras */
		i = hufGroup->minLen;
		while (j > limit[i]) ++i;
		bd->inbufBitCount += (hufGroup->maxLen - i);

		/* Huffman decode value to get nextSym (with bounds checking) */
		if (i > hufGroup->maxLen)
			return RETVAL_DATA_ERROR;
		j = (j >> (hufGroup->maxLen - i)) - base[i];
		if ((unsigned)j >= MAX_SYMBOLS)
			return RETVAL_DATA_ERROR;
		nextSym = hufGroup->permute[j];

		/* We have now decoded the symbol, which indicates either a new literal
		   byte, or a repeated run of the most recent literal byte.  First,
		   check if nextSym indicates a repeated run, and if so loop collecting
		   how many times to repeat the last literal. */
		if ((unsigned)nextSym <= SYMBOL_RUNB) { /* RUNA or RUNB */

			/* If this is the start of a new run, zero out counter */
			if (!runPos) {
				runPos = 1;
				t = 0;
			}

			/* Neat trick that saves 1 symbol: instead of or-ing 0 or 1 at
			   each bit position, add 1 or 2 instead.  For example,
			   1011 is 1<<0 + 1<<1 + 2<<2.  1010 is 2<<0 + 2<<1 + 1<<2.
			   You can make any bit pattern that way using 1 less symbol than
			   the basic or 0/1 method (except all bits 0, which would use no
			   symbols, but a run of length 0 doesn't mean anything in this
			   context).  Thus space is saved. */
			t += (runPos << nextSym); /* +runPos if RUNA; +2*runPos if RUNB */
			if (runPos < dbufSize) runPos <<= 1;
			goto end_of_huffman_loop;
		}

		/* When we hit the first non-run symbol after a run, we now know
		   how many times to repeat the last literal, so append that many
		   copies to our buffer of decoded symbols (dbuf) now.  (The last
		   literal used is the one at the head of the mtfSymbol array.) */
		if (runPos) {
			runPos = 0;
			if (dbufCount + t >= dbufSize) return RETVAL_DATA_ERROR;

			uc = symToByte[mtfSymbol[0]];
			byteCount[uc] += t;
			while (t--) dbuf[dbufCount++] = uc;
		}

		/* Is this the terminating symbol? */
		if (nextSym > symTotal) break;

		/* At this point, nextSym indicates a new literal character.  Subtract
		   one to get the position in the MTF array at which this literal is
		   currently to be found.  (Note that the result can't be -1 or 0,
		   because 0 and 1 are RUNA and RUNB.  But another instance of the
		   first symbol in the mtf array, position 0, would have been handled
		   as part of a run above.  Therefore 1 unused mtf position minus
		   2 non-literal nextSym values equals -1.) */
		if (dbufCount >= dbufSize) return RETVAL_DATA_ERROR;
		i = nextSym - 1;
		uc = mtfSymbol[i];

		/* Adjust the MTF array.  Since we typically expect to move only a
		 * small number of symbols, and are bound by 256 in any case, using
		 * memmove here would typically be bigger and slower due to function
		 * call overhead and other assorted setup costs. */
		do {
			mtfSymbol[i] = mtfSymbol[i-1];
		} while (--i);
		mtfSymbol[0] = uc;
		uc = symToByte[uc];

		/* We have our literal byte.  Save it into dbuf. */
		byteCount[uc]++;
		dbuf[dbufCount++] = (unsigned)uc;

		/* Skip group initialization if we're not done with this group.  Done
		 * this way to avoid compiler warning. */
 end_of_huffman_loop:
		if (symCount--) goto continue_this_group;
	}

	/* At this point, we've read all the Huffman-coded symbols (and repeated
	   runs) for this block from the input stream, and decoded them into the
	   intermediate buffer.  There are dbufCount many decoded bytes in dbuf[].
	   Now undo the Burrows-Wheeler transform on dbuf.
	   See http://dogma.net/markn/articles/bwt/bwt.htm
	 */

	/* Turn byteCount into cumulative occurrence counts of 0 to n-1. */
	j = 0;
	for (i = 0; i < 256; i++) {
		k = j + byteCount[i];
		byteCount[i] = j;
		j = k;
	}

	/* Figure out what order dbuf would be in if we sorted it. */
	for (i = 0; i < dbufCount; i++) {
		uc = (unsigned char)(dbuf[i] & 0xff);
		dbuf[byteCount[uc]] |= (i << 8);
		byteCount[uc]++;
	}

	/* Decode first byte by hand to initialize "previous" byte.  Note that it
	   doesn't get output, and if the first three characters are identical
	   it doesn't qualify as a run (hence writeRunCountdown=5). */
	if (dbufCount) {
		if ((int)origPtr >= dbufCount) return RETVAL_DATA_ERROR;
		bd->writePos = dbuf[origPtr];
		bd->writeCurrent = (unsigned char)(bd->writePos & 0xff);
		bd->writePos >>= 8;
		bd->writeRunCountdown = 5;
	}
	bd->writeCount = dbufCount;

	return RETVAL_OK;
}

/* Undo burrows-wheeler transform on intermediate buffer to produce output.
   If start_bunzip was initialized with out_fd=-1, then up to len bytes of
   data are written to outbuf.  Return value is number of bytes written or
   error (all errors are negative numbers).  If out_fd!=-1, outbuf and len
   are ignored, data is written to out_fd and return is RETVAL_OK or error.
*/
int FAST_FUNC read_bunzip(bunzip_data *bd, char *outbuf, int len)
{
	const unsigned *dbuf;
	int pos, current, previous, gotcount;

	/* If last read was short due to end of file, return last block now */
	if (bd->writeCount < 0) return bd->writeCount;

	gotcount = 0;
	dbuf = bd->dbuf;
	pos = bd->writePos;
	current = bd->writeCurrent;

	/* We will always have pending decoded data to write into the output
	   buffer unless this is the very first call (in which case we haven't
	   Huffman-decoded a block into the intermediate buffer yet). */
	if (bd->writeCopies) {

		/* Inside the loop, writeCopies means extra copies (beyond 1) */
		--bd->writeCopies;

		/* Loop outputting bytes */
		for (;;) {

			/* If the output buffer is full, snapshot state and return */
			if (gotcount >= len) {
				bd->writePos = pos;
				bd->writeCurrent = current;
				bd->writeCopies++;
				return len;
			}

			/* Write next byte into output buffer, updating CRC */
			outbuf[gotcount++] = current;
			bd->writeCRC = (bd->writeCRC << 8)
				^ bd->crc32Table[(bd->writeCRC >> 24) ^ current];

			/* Loop now if we're outputting multiple copies of this byte */
			if (bd->writeCopies) {
				--bd->writeCopies;
				continue;
			}
 decode_next_byte:
			if (!bd->writeCount--) break;
			/* Follow sequence vector to undo Burrows-Wheeler transform */
			previous = current;
			pos = dbuf[pos];
			current = pos & 0xff;
			pos >>= 8;

			/* After 3 consecutive copies of the same byte, the 4th
			 * is a repeat count.  We count down from 4 instead
			 * of counting up because testing for non-zero is faster */
			if (--bd->writeRunCountdown) {
				if (current != previous)
					bd->writeRunCountdown = 4;
			} else {

				/* We have a repeated run, this byte indicates the count */
				bd->writeCopies = current;
				current = previous;
				bd->writeRunCountdown = 5;

				/* Sometimes there are just 3 bytes (run length 0) */
				if (!bd->writeCopies) goto decode_next_byte;

				/* Subtract the 1 copy we'd output anyway to get extras */
				--bd->writeCopies;
			}
		}

		/* Decompression of this block completed successfully */
		bd->writeCRC = ~bd->writeCRC;
		bd->totalCRC = ((bd->totalCRC << 1) | (bd->totalCRC >> 31)) ^ bd->writeCRC;

		/* If this block had a CRC error, force file level CRC error. */
		if (bd->writeCRC != bd->headerCRC) {
			bd->totalCRC = bd->headerCRC + 1;
			return RETVAL_LAST_BLOCK;
		}
	}

	/* Refill the intermediate buffer by Huffman-decoding next block of input */
	/* (previous is just a convenient unused temp variable here) */
	previous = get_next_block(bd);
	if (previous) {
		bd->writeCount = previous;
		return (previous != RETVAL_LAST_BLOCK) ? previous : gotcount;
	}
	bd->writeCRC = ~0;
	pos = bd->writePos;
	current = bd->writeCurrent;
	goto decode_next_byte;
}

/* Allocate the structure, read file header.  If in_fd==-1, inbuf must contain
   a complete bunzip file (len bytes long).  If in_fd!=-1, inbuf and len are
   ignored, and data is read from file handle into temporary buffer. */

/* Because bunzip2 is used for help text unpacking, and because bb_show_usage()
   should work for NOFORK applets too, we must be extremely careful to not leak
   any allocations! */
int FAST_FUNC start_bunzip(bunzip_data **bdp, int in_fd, const unsigned char *inbuf,
						int len)
{
	bunzip_data *bd;
	unsigned i;
	enum {
		BZh0 = ('B' << 24) + ('Z' << 16) + ('h' << 8) + '0',
		h0 = ('h' << 8) + '0',
	};

	/* Figure out how much data to allocate */
	i = sizeof(bunzip_data);
	if (in_fd != -1) i += IOBUF_SIZE;

	/* Allocate bunzip_data.  Most fields initialize to zero. */
	bd = *bdp = xzalloc(i);

	/* Setup input buffer */
	bd->in_fd = in_fd;
	if (-1 == in_fd) {
		/* in this case, bd->inbuf is read-only */
		bd->inbuf = (void*)inbuf; /* cast away const-ness */
		bd->inbufCount = len;
	} else
		bd->inbuf = (unsigned char *)(bd + 1);

	/* Init the CRC32 table (big endian) */
	crc32_filltable(bd->crc32Table, 1);

	/* Setup for I/O error handling via longjmp */
	i = setjmp(bd->jmpbuf);
	if (i) return i;

	/* Ensure that file starts with "BZh['1'-'9']." */
	/* Update: now caller verifies 1st two bytes, makes .gz/.bz2
	 * integration easier */
	/* was: */
	/* i = get_bits(bd, 32); */
	/* if ((unsigned)(i - BZh0 - 1) >= 9) return RETVAL_NOT_BZIP_DATA; */
	i = get_bits(bd, 16);
	if ((unsigned)(i - h0 - 1) >= 9) return RETVAL_NOT_BZIP_DATA;

	/* Fourth byte (ascii '1'-'9') indicates block size in units of 100k of
	   uncompressed data.  Allocate intermediate buffer for block. */
	/* bd->dbufSize = 100000 * (i - BZh0); */
	bd->dbufSize = 100000 * (i - h0);

	/* Cannot use xmalloc - may leak bd in NOFORK case! */
	bd->dbuf = malloc_or_warn(bd->dbufSize * sizeof(int));
	if (!bd->dbuf) {
		free(bd);
		xfunc_die();
	}
	return RETVAL_OK;
}

void FAST_FUNC dealloc_bunzip(bunzip_data *bd)
{
	free(bd->dbuf);
	free(bd);
}


/* Decompress src_fd to dst_fd.  Stops at end of bzip data, not end of file. */
IF_DESKTOP(long long) int FAST_FUNC
unpack_bz2_stream(int src_fd, int dst_fd)
{
	IF_DESKTOP(long long total_written = 0;)
	char *outbuf;
	bunzip_data *bd;
	int i;

	outbuf = xmalloc(IOBUF_SIZE);
	i = start_bunzip(&bd, src_fd, NULL, 0);
	if (!i) {
		for (;;) {
			i = read_bunzip(bd, outbuf, IOBUF_SIZE);
			if (i <= 0) break;
			if (i != full_write(dst_fd, outbuf, i)) {
				i = RETVAL_SHORT_WRITE;
				break;
			}
			IF_DESKTOP(total_written += i;)
		}
	}

	/* Check CRC and release memory */

	if (i == RETVAL_LAST_BLOCK) {
		if (bd->headerCRC != bd->totalCRC) {
			bb_error_msg("CRC error");
		} else {
			i = RETVAL_OK;
		}
	} else if (i == RETVAL_SHORT_WRITE) {
		bb_error_msg("short write");
	} else {
		bb_error_msg("bunzip error %d", i);
	}
	dealloc_bunzip(bd);
	free(outbuf);

	return i ? i : IF_DESKTOP(total_written) + 0;
}

IF_DESKTOP(long long) int FAST_FUNC
unpack_bz2_stream_prime(int src_fd, int dst_fd)
{
	uint16_t magic2;
	xread(src_fd, &magic2, 2);
	if (magic2 != BZIP2_MAGIC) {
		bb_error_msg_and_die("invalid magic");
	}
	return unpack_bz2_stream(src_fd, dst_fd);
}

#ifdef TESTING

static char *const bunzip_errors[] = {
	NULL, "Bad file checksum", "Not bzip data",
	"Unexpected input EOF", "Unexpected output EOF", "Data error",
	"Out of memory", "Obsolete (pre 0.9.5) bzip format not supported"
};

/* Dumb little test thing, decompress stdin to stdout */
int main(int argc, char **argv)
{
	int i;
	char c;

	int i = unpack_bz2_stream_prime(0, 1);
	if (i < 0)
		fprintf(stderr, "%s\n", bunzip_errors[-i]);
	else if (read(STDIN_FILENO, &c, 1))
		fprintf(stderr, "Trailing garbage ignored\n");
	return -i;
}
#endif
