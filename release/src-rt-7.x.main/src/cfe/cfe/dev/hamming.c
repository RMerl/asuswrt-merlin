/*
 * Hamming ecc algorithm for NAND flash.
 *
 * Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id:  $
 * ftp://ftp.be.netbsd.org/pub/NetBSD/NetBSD-release-6/src/sys/dev/nand/hamming.c
 */
/*
 * Copyright (c) 2008, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <typedefs.h>

enum {
/* Multiple bits are incorrect in the data and they cannot be corrected. */
	HAMMING_ERROR_MULTIPLEBITS = -2,
	HAMMING_ERROR_ECC,
	HAMMING_ECC_OK,
/* The original code has been corrupted. */
	HAMMING_ERROR_SINGLEBIT
};


unsigned int popcount32(uint32 v);
void hamming_compute_256(const uint8 *data, uint8 *code);
int8
hamming_correct_256(uint8 *data, const uint8 *original_code,
    const uint8 *computed_code);
/**
 * Calculates the 22-bit hamming code for a 256-bytes block of data.
 * \param data  Data buffer to calculate code for.
 * \param code  Pointer to a buffer where the code should be stored.
 */
void
hamming_compute_256(const uint8 *data, uint8 *code)
{
	unsigned int i;
	uint8 column_sum = 0;
	uint8 even_line_code = 0;
	uint8 odd_line_code = 0;
	uint8 even_column_code = 0;
	uint8 odd_column_code = 0;

	/*
	 * Xor all bytes together to get the column sum;
	 * At the same time, calculate the even and odd line codes
	 */
	for (i = 0; i < 256; i++) {
		column_sum ^= data[i];

		/*
		 * If the xor sum of the byte is 0, then this byte has no
		 * incidence on the computed code; so check if the sum is 1.
		 */
	if ((popcount32((uint32)data[i]) & 1) == 1) {
			/*
			 * Parity groups are formed by forcing a particular
			 * index bit to 0 (even) or 1 (odd).
			 * Example on one byte:
			 *
			 * bits (dec)  7   6   5   4   3   2   1   0
			 *      (bin) 111 110 101 100 011 010 001 000
			 *                            '---'---'---'----------.
			 *                                                   |
			 * groups P4' ooooooooooooooo eeeeeeeeeeeeeee P4     |
			 *        P2' ooooooo eeeeeee ooooooo eeeeeee P2     |
			 *        P1' ooo eee ooo eee ooo eee ooo eee P1     |
			 *                                                   |
			 * We can see that:                                  |
			 *  - P4  -> bit 2 of index is 0 --------------------'
			 *  - P4' -> bit 2 of index is 1.
			 *  - P2  -> bit 1 of index if 0.
			 *  - etc...
			 * We deduce that a bit position has an impact on all
			 * even Px if the log2(x)nth bit of its index is 0
			 *     ex: log2(4) = 2,
			 * bit2 of the index must be 0 (-> 0 1 2 3)
			 * and on all odd Px' if the log2(x)nth bit
			 * of its index is 1
			 *     ex: log2(2) = 1,
			 * bit1 of the index must be 1 (-> 0 1 4 5)
			 *
			 * As such, we calculate all the possible Px and Px'
			 * values at the same time in two variables,
			 * even_line_code and odd_line_code, such as
			 *     even_line_code bits: P128  P64  P32
			 *                        P16  P8  P4  P2  P1
			 *     odd_line_code  bits: P128' P64' P32' P16'
			 *                        P8' P4' P2' P1'
			 */
			even_line_code ^= (255 - i);
			odd_line_code ^= i;
		}
	}

	/*
	 * At this point, we have the line parities, and the column sum.
	 * First, We must caculate the parity group values on the column sum.
	 */
	for (i = 0; i < 8; i++) {
		if (column_sum & 1) {
			even_column_code ^= (7 - i);
			odd_column_code ^= i;
		}
		column_sum >>= 1;
	}

	/*
	 * Now, we must interleave the parity values,
	 * to obtain the following layout:
	 * Code[0] = Line1
	 * Code[1] = Line2
	 * Code[2] = Column
	 * Line = Px' Px P(x-1)- P(x-1) ...
	 * Column = P4' P4 P2' P2 P1' P1 PadBit PadBit
	 */
	code[0] = 0;
	code[1] = 0;
	code[2] = 0;

	for (i = 0; i < 4; i++) {
		code[0] <<= 2;
		code[1] <<= 2;
		code[2] <<= 2;

		/* Line 1 */
		if ((odd_line_code & 0x80) != 0) {

			code[0] |= 2;
		}
		if ((even_line_code & 0x80) != 0) {

			code[0] |= 1;
		}

		/* Line 2 */
		if ((odd_line_code & 0x08) != 0) {

			code[1] |= 2;
		}
		if ((even_line_code & 0x08) != 0) {

			code[1] |= 1;
		}

		/* Column */
		if ((odd_column_code & 0x04) != 0) {

			code[2] |= 2;
		}
		if ((even_column_code & 0x04) != 0) {

			code[2] |= 1;
		}

		odd_line_code <<= 1;
		even_line_code <<= 1;
		odd_column_code <<= 1;
		even_column_code <<= 1;
	}

	/* Invert codes (linux compatibility) */
	code[0] = ~code[0];
	code[1] = ~code[1];
	code[2] = ~code[2];
}

/**
 * Verifies and corrects a 256-bytes block of data using the given 22-bits
 * hamming code.
 * Returns 0 if there is no error, otherwise returns a HAMMING_ERROR code.
 * param data  Data buffer to check.
 * \param original_code  Hamming code to use for verifying the data.
 */
int8
hamming_correct_256(uint8 *data, const uint8 *original_code,
    const uint8 *computed_code)
{
	/* Calculate new code */
	/* we allocate 4 bytes so we can use popcount32 in one step */
	uint8 correction_code[4];

	/* this byte should remain zero all the time */
	correction_code[3] = 0;

	/* Xor both codes together */
	correction_code[0] = computed_code[0] ^ original_code[0];
	correction_code[1] = computed_code[1] ^ original_code[1];
	correction_code[2] = computed_code[2] ^ original_code[2];

	/* If all bytes are 0, there is no error */
	if (*(uint32 *)correction_code == 0) {
		return 0;
	}
	/* If there is a single bit error, there are 11 bits set to 1 */
	if (popcount32(*(uint32 *)correction_code) == 11) {
		/* Get byte and bit indexes */
		uint8 byte = correction_code[0] & 0x80;
		byte |= (correction_code[0] << 1) & 0x40;
		byte |= (correction_code[0] << 2) & 0x20;
		byte |= (correction_code[0] << 3) & 0x10;

		byte |= (correction_code[1] >> 4) & 0x08;
		byte |= (correction_code[1] >> 3) & 0x04;
		byte |= (correction_code[1] >> 2) & 0x02;
		byte |= (correction_code[1] >> 1) & 0x01;

		uint8 bit = (correction_code[2] >> 5) & 0x04;
		bit |= (correction_code[2] >> 4) & 0x02;
		bit |= (correction_code[2] >> 3) & 0x01;

		/* Correct bit */
		data[byte] ^= (1 << bit);

		return HAMMING_ERROR_SINGLEBIT;
	}
	/* Check if ECC has been corrupted */
	if (popcount32(*(uint32 *)correction_code) == 1) {
		return HAMMING_ERROR_ECC;
	} else {
		/* Otherwise, this is a multi-bit error */
		return HAMMING_ERROR_MULTIPLEBITS;
	}
}
